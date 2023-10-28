// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"

#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

static int ComputeEdgeWidth(const BorderImageLength& border_slice,
                            int border_side,
                            int image_side,
                            int box_extent) {
  if (border_slice.IsNumber())
    return LayoutUnit(border_slice.Number() * border_side).Floor();
  if (border_slice.length().IsAuto())
    return image_side;
  return ValueForLength(border_slice.length(), LayoutUnit(box_extent)).Floor();
}

static float ComputeEdgeSlice(const Length& slice,
                              float slice_scale,
                              float maximum) {
  float resolved;
  // If the slice is a <number> (stored as a fixed Length), scale it by the
  // slice scale to get to the same space as the image.
  if (slice.IsFixed()) {
    resolved = slice.Value() * slice_scale;
  } else {
    DCHECK(slice.IsPercent());
    resolved = FloatValueForLength(slice, maximum);
  }
  return std::min(maximum, resolved);
}

// Scale the width of the |start| and |end| edges using |scale_factor|.
// Always round the width of |start|. Based on available space (|box_extent|),
// the width of |end| is either rounded or floored. This should keep abutting
// edges flush, while not producing potentially "uneven" widths for a
// non-overlapping case.
static void ScaleEdgeWidths(NinePieceImageGrid::Edge& start,
                            NinePieceImageGrid::Edge& end,
                            int box_extent,
                            float scale_factor) {
  LayoutUnit start_width(start.width);
  start_width *= scale_factor;
  LayoutUnit end_width(end.width);
  end_width *= scale_factor;
  start.width = start_width.Round();
  int remaining = box_extent - start.width;
  int rounded_end = end_width.Round();
  end.width = rounded_end > remaining ? end_width.Floor() : rounded_end;
}

NinePieceImageGrid::NinePieceImageGrid(const NinePieceImage& nine_piece_image,
                                       FloatSize image_size,
                                       const FloatSize& slice_scale,
                                       float zoom,
                                       IntRect border_image_area,
                                       const IntRectOutsets& border_widths,
                                       PhysicalBoxSides sides_to_include)
    : border_image_area_(border_image_area),
      image_size_(image_size),
      horizontal_tile_rule_(nine_piece_image.HorizontalRule()),
      vertical_tile_rule_(nine_piece_image.VerticalRule()),
      fill_(nine_piece_image.Fill()) {
  const LengthBox& image_slices = nine_piece_image.ImageSlices();
  top_.slice = ComputeEdgeSlice(image_slices.Top(), slice_scale.Height(),
                                image_size.Height());
  right_.slice = ComputeEdgeSlice(image_slices.Right(), slice_scale.Width(),
                                  image_size.Width());
  bottom_.slice = ComputeEdgeSlice(image_slices.Bottom(), slice_scale.Height(),
                                   image_size.Height());
  left_.slice = ComputeEdgeSlice(image_slices.Left(), slice_scale.Width(),
                                 image_size.Width());

  // TODO(fs): Compute edge widths to LayoutUnit, and then only round to
  // integer at the end - after (potential) compensation for overlapping edges.

  // |Edge::slice| is in image-local units (physical pixels for raster images),
  // but when using it to resolve 'auto' for border-image-widths we want it to
  // be in zoomed CSS pixels, so divide by |slice_scale| and multiply by zoom.
  const FloatSize auto_slice_adjustment(zoom / slice_scale.Width(),
                                        zoom / slice_scale.Height());
  const BorderImageLengthBox& border_slices = nine_piece_image.BorderSlices();
  top_.width =
      sides_to_include.top
          ? ComputeEdgeWidth(border_slices.Top(), border_widths.Top(),
                             top_.slice * auto_slice_adjustment.Height(),
                             border_image_area.Height())
          : 0;
  right_.width =
      sides_to_include.right
          ? ComputeEdgeWidth(border_slices.Right(), border_widths.Right(),
                             right_.slice * auto_slice_adjustment.Width(),
                             border_image_area.Width())
          : 0;
  bottom_.width =
      sides_to_include.bottom
          ? ComputeEdgeWidth(border_slices.Bottom(), border_widths.Bottom(),
                             bottom_.slice * auto_slice_adjustment.Height(),
                             border_image_area.Height())
          : 0;
  left_.width =
      sides_to_include.left
          ? ComputeEdgeWidth(border_slices.Left(), border_widths.Left(),
                             left_.slice * auto_slice_adjustment.Width(),
                             border_image_area.Width())
          : 0;

  // The spec says: Given Lwidth as the width of the border image area, Lheight
  // as its height, and Wside as the border image width offset for the side, let
  // f = min(Lwidth/(Wleft+Wright), Lheight/(Wtop+Wbottom)). If f < 1, then all
  // W are reduced by multiplying them by f.
  int border_side_width = base::ClampAdd(left_.width, right_.width).Max(1);
  int border_side_height = base::ClampAdd(top_.width, bottom_.width).Max(1);
  float border_side_scale_factor =
      std::min((float)border_image_area.Width() / border_side_width,
               (float)border_image_area.Height() / border_side_height);
  if (border_side_scale_factor < 1) {
    ScaleEdgeWidths(top_, bottom_, border_image_area.Height(),
                    border_side_scale_factor);
    ScaleEdgeWidths(left_, right_, border_image_area.Width(),
                    border_side_scale_factor);
  }
}

// Given a rectangle, construct a subrectangle using offset, width and height.
// Negative offsets are relative to the extent of the given rectangle.
static FloatRect Subrect(FloatRect rect,
                         float offset_x,
                         float offset_y,
                         float width,
                         float height) {
  float base_x = rect.X();
  if (offset_x < 0)
    base_x = rect.MaxX();

  float base_y = rect.Y();
  if (offset_y < 0)
    base_y = rect.MaxY();

  return FloatRect(base_x + offset_x, base_y + offset_y, width, height);
}

static FloatRect Subrect(IntRect rect,
                         float offset_x,
                         float offset_y,
                         float width,
                         float height) {
  return Subrect(FloatRect(rect), offset_x, offset_y, width, height);
}

static FloatRect Subrect(FloatSize size,
                         float offset_x,
                         float offset_y,
                         float width,
                         float height) {
  return Subrect(FloatRect(FloatPoint(), size), offset_x, offset_y, width,
                 height);
}

static inline void SetCornerPiece(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    bool is_drawable,
    const FloatRect& source,
    const FloatRect& destination) {
  draw_info.is_drawable = is_drawable;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
  }
}

void NinePieceImageGrid::SetDrawInfoCorner(NinePieceDrawInfo& draw_info,
                                           NinePiece piece) const {
  switch (piece) {
    case kTopLeftPiece:
      SetCornerPiece(
          draw_info, top_.IsDrawable() && left_.IsDrawable(),
          Subrect(image_size_, 0, 0, left_.slice, top_.slice),
          Subrect(border_image_area_, 0, 0, left_.width, top_.width));
      break;
    case kBottomLeftPiece:
      SetCornerPiece(
          draw_info, bottom_.IsDrawable() && left_.IsDrawable(),
          Subrect(image_size_, 0, -bottom_.slice, left_.slice, bottom_.slice),
          Subrect(border_image_area_, 0, -bottom_.width, left_.width,
                  bottom_.width));
      break;
    case kTopRightPiece:
      SetCornerPiece(
          draw_info, top_.IsDrawable() && right_.IsDrawable(),
          Subrect(image_size_, -right_.slice, 0, right_.slice, top_.slice),
          Subrect(border_image_area_, -right_.width, 0, right_.width,
                  top_.width));
      break;
    case kBottomRightPiece:
      SetCornerPiece(draw_info, bottom_.IsDrawable() && right_.IsDrawable(),
                     Subrect(image_size_, -right_.slice, -bottom_.slice,
                             right_.slice, bottom_.slice),
                     Subrect(border_image_area_, -right_.width, -bottom_.width,
                             right_.width, bottom_.width));
      break;
    default:
      NOTREACHED();
      break;
  }
}

static inline void SetHorizontalEdge(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    const NinePieceImageGrid::Edge& edge,
    const FloatRect& source,
    const FloatRect& destination,
    ENinePieceImageRule tile_rule) {
  draw_info.is_drawable =
      edge.IsDrawable() && source.Width() > 0 && destination.Width() > 0;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
    draw_info.tile_scale = FloatSize(edge.Scale(), edge.Scale());
    draw_info.tile_rule = {tile_rule, kStretchImageRule};
  }
}

static inline void SetVerticalEdge(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    const NinePieceImageGrid::Edge& edge,
    const FloatRect& source,
    const FloatRect& destination,
    ENinePieceImageRule tile_rule) {
  draw_info.is_drawable =
      edge.IsDrawable() && source.Height() > 0 && destination.Height() > 0;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
    draw_info.tile_scale = FloatSize(edge.Scale(), edge.Scale());
    draw_info.tile_rule = {kStretchImageRule, tile_rule};
  }
}

void NinePieceImageGrid::SetDrawInfoEdge(NinePieceDrawInfo& draw_info,
                                         NinePiece piece) const {
  FloatSize edge_source_size =
      image_size_ -
      FloatSize(left_.slice + right_.slice, top_.slice + bottom_.slice);
  IntSize edge_destination_size =
      border_image_area_.Size() -
      IntSize(left_.width + right_.width, top_.width + bottom_.width);

  switch (piece) {
    case kLeftPiece:
      SetVerticalEdge(draw_info, left_,
                      Subrect(image_size_, 0, top_.slice, left_.slice,
                              edge_source_size.Height()),
                      Subrect(border_image_area_, 0, top_.width, left_.width,
                              edge_destination_size.Height()),
                      vertical_tile_rule_);
      break;
    case kRightPiece:
      SetVerticalEdge(draw_info, right_,
                      Subrect(image_size_, -right_.slice, top_.slice,
                              right_.slice, edge_source_size.Height()),
                      Subrect(border_image_area_, -right_.width, top_.width,
                              right_.width, edge_destination_size.Height()),
                      vertical_tile_rule_);
      break;
    case kTopPiece:
      SetHorizontalEdge(draw_info, top_,
                        Subrect(image_size_, left_.slice, 0,
                                edge_source_size.Width(), top_.slice),
                        Subrect(border_image_area_, left_.width, 0,
                                edge_destination_size.Width(), top_.width),
                        horizontal_tile_rule_);
      break;
    case kBottomPiece:
      SetHorizontalEdge(draw_info, bottom_,
                        Subrect(image_size_, left_.slice, -bottom_.slice,
                                edge_source_size.Width(), bottom_.slice),
                        Subrect(border_image_area_, left_.width, -bottom_.width,
                                edge_destination_size.Width(), bottom_.width),
                        horizontal_tile_rule_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void NinePieceImageGrid::SetDrawInfoMiddle(NinePieceDrawInfo& draw_info) const {
  FloatSize source_size = image_size_ - FloatSize(left_.slice + right_.slice,
                                                  top_.slice + bottom_.slice);
  IntSize destination_size =
      border_image_area_.Size() -
      IntSize(left_.width + right_.width, top_.width + bottom_.width);

  draw_info.is_drawable =
      fill_ && !source_size.IsEmpty() && !destination_size.IsEmpty();
  if (!draw_info.is_drawable)
    return;

  draw_info.source = Subrect(image_size_, left_.slice, top_.slice,
                             source_size.Width(), source_size.Height());
  draw_info.destination =
      Subrect(border_image_area_, left_.width, top_.width,
              destination_size.Width(), destination_size.Height());

  FloatSize middle_scale_factor(1, 1);

  if (top_.IsDrawable())
    middle_scale_factor.SetWidth(top_.Scale());
  else if (bottom_.IsDrawable())
    middle_scale_factor.SetWidth(bottom_.Scale());

  if (left_.IsDrawable())
    middle_scale_factor.SetHeight(left_.Scale());
  else if (right_.IsDrawable())
    middle_scale_factor.SetHeight(right_.Scale());

  if (!source_size.IsEmpty()) {
    // For "stretch" rules, just override the scale factor and replace. We only
    // have to do this for the center tile, since sides don't even use the scale
    // factor unless they have a rule other than "stretch". The middle however
    // can have "stretch" specified in one axis but not the other, so we have to
    // correct the scale here.
    if (horizontal_tile_rule_ == kStretchImageRule) {
      middle_scale_factor.SetWidth(destination_size.Width() /
                                   source_size.Width());
    }
    if (vertical_tile_rule_ == kStretchImageRule) {
      middle_scale_factor.SetHeight(destination_size.Height() /
                                    source_size.Height());
    }
  }

  draw_info.tile_scale = middle_scale_factor;
  draw_info.tile_rule = {horizontal_tile_rule_, vertical_tile_rule_};
}

NinePieceImageGrid::NinePieceDrawInfo NinePieceImageGrid::GetNinePieceDrawInfo(
    NinePiece piece) const {
  NinePieceDrawInfo draw_info;
  draw_info.is_corner_piece =
      piece == kTopLeftPiece || piece == kTopRightPiece ||
      piece == kBottomLeftPiece || piece == kBottomRightPiece;

  if (draw_info.is_corner_piece)
    SetDrawInfoCorner(draw_info, piece);
  else if (piece != kMiddlePiece)
    SetDrawInfoEdge(draw_info, piece);
  else
    SetDrawInfoMiddle(draw_info);

  return draw_info;
}

}  // namespace blink
