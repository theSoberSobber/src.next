// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_BORDER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_BORDER_PAINTER_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class Path;
struct PhysicalRect;

typedef unsigned BorderEdgeFlags;

class BoxBorderPainter {
  STACK_ALLOCATED();

 public:
  static void PaintBorder(GraphicsContext& context,
                          const PhysicalRect& border_rect,
                          const ComputedStyle& style,
                          BackgroundBleedAvoidance bleed_avoidance,
                          PhysicalBoxSides sides_to_include) {
    BoxBorderPainter(context, border_rect, style, bleed_avoidance,
                     sides_to_include)
        .Paint();
  }

  static void PaintSingleRectOutline(GraphicsContext& context,
                                     const ComputedStyle& style,
                                     const PhysicalRect& outer,
                                     const PhysicalRect& inner,
                                     const BorderEdge& edge) {
    BoxBorderPainter(context, style, outer, inner, edge).Paint();
  }

  static void DrawBoxSide(GraphicsContext& context,
                          const IntRect& snapped_edge_rect,
                          BoxSide side,
                          Color color,
                          EBorderStyle style) {
    DrawLineForBoxSide(context, snapped_edge_rect.X(), snapped_edge_rect.Y(),
                       snapped_edge_rect.MaxX(), snapped_edge_rect.MaxY(), side,
                       color, style, 0, 0, true);
  }

  // TODO(crbug.com/1201762): The float parameters are truncated to int in the
  // function, which implicitly snaps to whole pixels perhaps unexpectedly. To
  // avoid the problem, we should use the above function which requires the
  // caller to snap to whole pixels explicitly.
  static void DrawLineForBoxSide(GraphicsContext&,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2,
                                 BoxSide,
                                 Color,
                                 EBorderStyle,
                                 int adjacent_edge_width1,
                                 int adjacent_edge_width2,
                                 bool antialias = false);

 private:
  // For PaintBorder().
  BoxBorderPainter(GraphicsContext&,
                   const PhysicalRect& border_rect,
                   const ComputedStyle&,
                   BackgroundBleedAvoidance,
                   PhysicalBoxSides sides_to_include);
  // For PaintSingleRectOutline().
  BoxBorderPainter(GraphicsContext&,
                   const ComputedStyle&,
                   const PhysicalRect& outer,
                   const PhysicalRect& inner,
                   const BorderEdge&);

  void Paint() const;

  struct ComplexBorderInfo;
  enum MiterType {
    kNoMiter,
    kSoftMiter,  // Anti-aliased
    kHardMiter,  // Not anti-aliased
  };

  void ComputeBorderProperties();

  BorderEdgeFlags PaintOpacityGroup(const ComplexBorderInfo&,
                                    unsigned index,
                                    float accumulated_opacity) const;
  void PaintSide(const ComplexBorderInfo&,
                 BoxSide,
                 unsigned alpha,
                 BorderEdgeFlags) const;
  void PaintOneBorderSide(const FloatRect& side_rect,
                          BoxSide,
                          BoxSide adjacent_side1,
                          BoxSide adjacent_side2,
                          const Path*,
                          bool antialias,
                          Color,
                          BorderEdgeFlags) const;
  bool PaintBorderFastPath() const;
  void DrawDoubleBorder() const;

  void DrawBoxSideFromPath(const Path&,
                           float thickness,
                           float draw_thickness,
                           BoxSide,
                           Color,
                           EBorderStyle) const;
  void DrawDashedDottedBoxSideFromPath(float thickness,
                                       float draw_thickness,
                                       Color,
                                       EBorderStyle) const;
  void DrawWideDottedBoxSideFromPath(const Path&, float thickness) const;
  void DrawDoubleBoxSideFromPath(const Path&,
                                 float thickness,
                                 float draw_thickness,
                                 BoxSide,
                                 Color) const;
  void DrawRidgeGrooveBoxSideFromPath(const Path&,
                                      float thickness,
                                      float draw_thickness,
                                      BoxSide,
                                      Color,
                                      EBorderStyle) const;
  void ClipBorderSidePolygon(BoxSide, MiterType miter1, MiterType miter2) const;
  FloatRect CalculateSideRectIncludingInner(BoxSide) const;
  void ClipBorderSideForComplexInnerPath(BoxSide) const;

  MiterType ComputeMiter(BoxSide,
                         BoxSide adjacent_side,
                         BorderEdgeFlags,
                         bool antialias) const;
  static bool MitersRequireClipping(MiterType miter1,
                                    MiterType miter2,
                                    EBorderStyle,
                                    bool antialias);

  LayoutRectOutsets DoubleStripeInsets(
      BorderEdge::DoubleBorderStripe stripe) const;
  LayoutRectOutsets CenterInsets() const;

  bool ColorsMatchAtCorner(BoxSide side, BoxSide adjacent_side) const;

  const BorderEdge& FirstEdge() const {
    DCHECK(visible_edge_set_);
    return edges_[first_visible_edge_];
  }

  BorderEdge& Edge(BoxSide side) { return edges_[static_cast<unsigned>(side)]; }
  const BorderEdge& Edge(BoxSide side) const {
    return edges_[static_cast<unsigned>(side)];
  }

  GraphicsContext& context_;

  // const inputs
  const PhysicalRect border_rect_;
  const ComputedStyle& style_;
  const BackgroundBleedAvoidance bleed_avoidance_;
  const PhysicalBoxSides sides_to_include_;

  // computed attributes
  FloatRoundedRect outer_;
  FloatRoundedRect inner_;
  BorderEdge edges_[4];

  unsigned visible_edge_count_;
  unsigned first_visible_edge_;
  BorderEdgeFlags visible_edge_set_;

  bool is_uniform_style_;
  bool is_uniform_width_;
  bool is_uniform_color_;
  bool is_rounded_;
  bool has_alpha_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_BORDER_PAINTER_H_
