// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

// Adjusts paint chunk properties, cull rect of the input PaintInfo and finds
// the paint offset for a LayoutObject or an NGPaintFragment before painting.
//
// Normally a Paint(const PaintInfo&) method creates an ScopedPaintState and
// holds it in the stack, and pass its GetPaintInfo() and PaintOffset() to the
// other PaintXXX() methods that paint different parts of the object.
//
// Each object create its own ScopedPaintState, so ScopedPaintState created for
// one object won't be passed to another object. Instead, PaintInfo is passed
// between objects.
class ScopedPaintState {
  STACK_ALLOCATED();

 public:
  ScopedPaintState(const LayoutObject& object,
                   const PaintInfo& paint_info,
                   const FragmentData* fragment_data)
      : fragment_to_paint_(fragment_data), input_paint_info_(paint_info) {
    if (!fragment_to_paint_) {
      // The object has nothing to paint in the current fragment.
      // TODO(wangxianzhu): Use DCHECK(fragment_to_paint_) in PaintOffset()
      // when all painters check FragmentToPaint() before painting.
      paint_offset_ =
          PhysicalOffset(LayoutUnit::NearlyMax(), LayoutUnit::NearlyMax());
      return;
    }
    paint_offset_ = fragment_to_paint_->PaintOffset();
    if (&object == paint_info.PaintContainer()) {
      // PaintLayerPainter already adjusted for PaintOffsetTranslation for
      // PaintContainer.
      return;
    }
    const auto* properties = fragment_to_paint_->PaintProperties();
    if (!properties)
      return;
    if (properties->PaintOffsetTranslation()) {
      AdjustForPaintOffsetTranslation(object,
                                      *properties->PaintOffsetTranslation());
    } else if (object.IsNGSVGText()) {
      if (const auto* transform = properties->Transform()) {
        adjusted_paint_info_.emplace(paint_info);
        adjusted_paint_info_->TransformCullRect(*transform);
      }
    }
  }

  ScopedPaintState(const LayoutObject& object, const PaintInfo& paint_info)
      : ScopedPaintState(object,
                         paint_info,
                         paint_info.FragmentToPaint(object)) {}

  ScopedPaintState(const NGPhysicalFragment& fragment,
                   const PaintInfo& paint_info)
      : ScopedPaintState(*fragment.GetLayoutObject(),
                         paint_info,
                         paint_info.FragmentToPaint(fragment)) {}

  ~ScopedPaintState() {
    if (paint_offset_translation_as_drawing_)
      FinishPaintOffsetTranslationAsDrawing();
  }

  const PaintInfo& GetPaintInfo() const {
    return adjusted_paint_info_ ? *adjusted_paint_info_ : input_paint_info_;
  }

  PaintInfo& MutablePaintInfo() {
    if (!adjusted_paint_info_)
      adjusted_paint_info_.emplace(input_paint_info_);
    return *adjusted_paint_info_;
  }

  PhysicalOffset PaintOffset() const { return paint_offset_; }

  const FragmentData* FragmentToPaint() const { return fragment_to_paint_; }

  PhysicalRect LocalCullRect() const {
    PhysicalRect cull_rect(LayoutRect(GetPaintInfo().GetCullRect().Rect()));
    cull_rect.Move(-PaintOffset());
    return cull_rect;
  }

  bool LocalRectIntersectsCullRect(const PhysicalRect& local_rect) const {
    return GetPaintInfo().IntersectsCullRect(local_rect, PaintOffset());
  }

 protected:
  // Constructors for subclasses to create the initial state before adjustment.
  ScopedPaintState(const ScopedPaintState& input)
      : fragment_to_paint_(input.fragment_to_paint_),
        input_paint_info_(input.GetPaintInfo()),
        paint_offset_(input.PaintOffset()) {}
  ScopedPaintState(const PaintInfo& paint_info,
                   const PhysicalOffset& paint_offset,
                   const LayoutObject& object)
      : fragment_to_paint_(paint_info.FragmentToPaint(object)),
        input_paint_info_(paint_info),
        paint_offset_(paint_offset) {}

 private:
  void AdjustForPaintOffsetTranslation(
      const LayoutObject&,
      const TransformPaintPropertyNode& paint_offset_translation);

  void FinishPaintOffsetTranslationAsDrawing();

 protected:
  const FragmentData* fragment_to_paint_;
  const PaintInfo& input_paint_info_;
  PhysicalOffset paint_offset_;
  absl::optional<PaintInfo> adjusted_paint_info_;
  absl::optional<ScopedPaintChunkProperties> chunk_properties_;
  bool paint_offset_translation_as_drawing_ = false;
};

// Adjusts paint chunk properties, cull rect and paint offset of the input
// ScopedPaintState for box contents if needed.
class ScopedBoxContentsPaintState : public ScopedPaintState {
 public:
  ScopedBoxContentsPaintState(const ScopedPaintState& input,
                              const LayoutBox& box)
      : ScopedPaintState(input) {
    AdjustForBoxContents(box);
  }

  ScopedBoxContentsPaintState(const PaintInfo& paint_info,
                              const PhysicalOffset& paint_offset,
                              const LayoutBox& box)
      : ScopedPaintState(paint_info, paint_offset, box) {
    AdjustForBoxContents(box);
  }

 private:
  void AdjustForBoxContents(const LayoutBox&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_
