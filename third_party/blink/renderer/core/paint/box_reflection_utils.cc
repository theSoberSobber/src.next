// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_reflection_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

BoxReflection BoxReflectionForPaintLayer(const PaintLayer& layer,
                                         const ComputedStyle& style) {
  const StyleReflection* reflect_style = style.BoxReflect();

  LayoutRect frame_layout_rect = layer.GetLayoutBox()->FrameRect();
  FloatRect frame_rect(frame_layout_rect);
  BoxReflection::ReflectionDirection direction =
      BoxReflection::kVerticalReflection;
  float offset = 0;
  switch (reflect_style->Direction()) {
    case kReflectionAbove:
      direction = BoxReflection::kVerticalReflection;
      offset =
          -FloatValueForLength(reflect_style->Offset(), frame_rect.Height());
      break;
    case kReflectionBelow:
      direction = BoxReflection::kVerticalReflection;
      offset =
          2 * frame_rect.Height() +
          FloatValueForLength(reflect_style->Offset(), frame_rect.Height());
      break;
    case kReflectionLeft:
      direction = BoxReflection::kHorizontalReflection;
      offset =
          -FloatValueForLength(reflect_style->Offset(), frame_rect.Width());
      break;
    case kReflectionRight:
      direction = BoxReflection::kHorizontalReflection;
      offset = 2 * frame_rect.Width() +
               FloatValueForLength(reflect_style->Offset(), frame_rect.Width());
      break;
  }

  const NinePieceImage& mask_nine_piece = reflect_style->Mask();
  if (!mask_nine_piece.HasImage())
    return BoxReflection(direction, offset, nullptr, FloatRect());

  PhysicalRect mask_rect(PhysicalOffset(), frame_layout_rect.Size());
  PhysicalRect mask_bounding_rect(mask_rect);
  mask_bounding_rect.Expand(style.ImageOutsets(mask_nine_piece));

  PaintRecordBuilder builder;
  {
    GraphicsContext& context = builder.Context();
    DrawingRecorder recorder(context, layer.GetLayoutObject(),
                             DisplayItem::kReflectionMask);
    Node* node = nullptr;
    const LayoutObject* layout_object = &layer.GetLayoutObject();
    for (; layout_object && !node; layout_object = layout_object->Parent())
      node = layout_object->GeneratingNode();
    NinePieceImagePainter::Paint(builder.Context(), layer.GetLayoutObject(),
                                 layer.GetLayoutObject().GetDocument(), node,
                                 mask_rect, style, mask_nine_piece);
  }
  return BoxReflection(direction, offset, builder.EndRecording(),
                       FloatRect(mask_bounding_rect));
}

}  // namespace blink
