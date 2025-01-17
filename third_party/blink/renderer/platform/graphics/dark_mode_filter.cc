// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"

#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace blink {

namespace {

const size_t kMaxCacheSize = 1024u;

bool IsRasterSideDarkModeForImagesEnabled() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRasterSideDarkModeForImages);
  return enabled;
}

bool ShouldUseRasterSidePath(Image* image) {
  DCHECK(image);

  // Raster-side path is not enabled.
  if (!IsRasterSideDarkModeForImagesEnabled())
    return false;

  // Raster-side path is only supported for bitmap images.
  return image->IsBitmapImage();
}

sk_sp<SkColorFilter> GetDarkModeFilterForImageOnMainThread(
    DarkModeFilter* filter,
    Image* image,
    const SkIRect& rounded_src) {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.DarkMode.ApplyToImageOnMainThread");

  sk_sp<SkColorFilter> color_filter;
  DarkModeImageCache* cache = image->GetDarkModeImageCache();
  DCHECK(cache);
  if (cache->Exists(rounded_src)) {
    color_filter = cache->Get(rounded_src);
  } else {
    // Performance warning: Calling AsSkBitmapForCurrentFrame() will
    // synchronously decode image.
    SkBitmap bitmap =
        image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
    SkPixmap pixmap;
    bitmap.peekPixels(&pixmap);
    color_filter = filter->GenerateImageFilter(pixmap, rounded_src);

    // Using blink side dark mode for images, it is hard to implement
    // caching mechanism for partially loaded bitmap image content, as
    // content id for the image frame being rendered gets decided during
    // rastering only. So caching of dark mode result will be deferred until
    // default frame is completely received. This will help get correct
    // classification results for incremental content received for the given
    // image.
    if (!image->IsBitmapImage() || image->CurrentFrameIsComplete())
      cache->Add(rounded_src, color_filter);
  }
  return color_filter;
}

// TODO(gilmanmh): If grayscaling images in dark mode proves popular among
// users, consider experimenting with different grayscale algorithms.
sk_sp<SkColorFilter> MakeGrayscaleFilter(float grayscale_percent) {
  DCHECK_GE(grayscale_percent, 0.0f);
  DCHECK_LE(grayscale_percent, 1.0f);

  SkColorMatrix grayscale_matrix;
  grayscale_matrix.setSaturation(1.0f - grayscale_percent);
  return SkColorFilters::Matrix(grayscale_matrix);
}

}  // namespace

// DarkModeInvertedColorCache - Implements cache for inverted colors.
class DarkModeInvertedColorCache {
 public:
  DarkModeInvertedColorCache() : cache_(kMaxCacheSize) {}
  ~DarkModeInvertedColorCache() = default;

  SkColor GetInvertedColor(DarkModeColorFilter* filter, SkColor color) {
    SkColor key(color);
    auto it = cache_.Get(key);
    if (it != cache_.end())
      return it->second;

    SkColor inverted_color = filter->InvertColor(color);
    cache_.Put(key, static_cast<SkColor>(inverted_color));
    return inverted_color;
  }

  void Clear() { cache_.Clear(); }

  size_t size() { return cache_.size(); }

 private:
  base::HashingLRUCache<SkColor, SkColor> cache_;
};

DarkModeFilter::DarkModeFilter(const DarkModeSettings& settings)
    : immutable_(settings),
      inverted_color_cache_(new DarkModeInvertedColorCache()) {}

DarkModeFilter::~DarkModeFilter() {}

DarkModeFilter::ImmutableData::ImmutableData(const DarkModeSettings& settings)
    : settings(settings),
      text_classifier(nullptr),
      background_classifier(nullptr),
      image_classifier(nullptr),
      color_filter(nullptr),
      image_filter(nullptr) {
  color_filter = DarkModeColorFilter::FromSettings(settings);
  if (!color_filter)
    return;

  if (settings.image_grayscale_percent > 0.0f)
    image_filter = MakeGrayscaleFilter(settings.image_grayscale_percent);
  else
    image_filter = color_filter->ToSkColorFilter();

  text_classifier = DarkModeColorClassifier::MakeTextColorClassifier(settings);
  background_classifier =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings);
  image_classifier = std::make_unique<DarkModeImageClassifier>();
}

SkColor DarkModeFilter::InvertColorIfNeeded(SkColor color, ElementRole role) {
  if (!immutable_.color_filter)
    return color;

  if (role_override_.has_value())
    role = role_override_.value();

  if (ShouldApplyToColor(color, role)) {
    return inverted_color_cache_->GetInvertedColor(
        immutable_.color_filter.get(), color);
  }

  return color;
}

DarkModeResult DarkModeFilter::AnalyzeShouldApplyToImage(
    const SkIRect& src,
    const SkIRect& dst) const {
  if (immutable_.settings.image_policy == DarkModeImagePolicy::kFilterNone)
    return DarkModeResult::kDoNotApplyFilter;

  if (immutable_.settings.image_policy == DarkModeImagePolicy::kFilterAll)
    return DarkModeResult::kApplyFilter;

  // Images being drawn from very smaller |src| rect, i.e. one of the dimensions
  // is very small, can be used for the border around the content or showing
  // separator. Consider these images irrespective of size of the rect being
  // drawn to. Classifying them will not be too costly.
  if (src.width() <= kMinImageLength || src.height() <= kMinImageLength)
    return DarkModeResult::kNotClassified;

  // Do not consider images being drawn into bigger rect as these images are not
  // meant for icons or representing smaller widgets. These images are
  // considered as photos which should be untouched.
  return (dst.width() <= kMaxImageLength && dst.height() <= kMaxImageLength)
             ? DarkModeResult::kNotClassified
             : DarkModeResult::kDoNotApplyFilter;
}

bool DarkModeFilter::ShouldApplyFilterToImage(ImageType type) const {
  DarkModeImagePolicy image_policy = GetDarkModeImagePolicy();
  if (image_policy == DarkModeImagePolicy::kFilterNone)
    return false;
  if (image_policy == DarkModeImagePolicy::kFilterAll)
    return true;

  // kIcon: Do not consider images being drawn into bigger rect as these
  // images are not meant for icons or representing smaller widgets. These
  // images are considered as photos which should be untouched.
  // kSeparator: Images being drawn from very smaller |src| rect, i.e. one of
  // the dimensions is very small, can be used for the border around the content
  // or showing separator. Consider these images irrespective of size of the
  // rect being drawn to. Classifying them will not be too costly.
  return type == ImageType::kIcon || type == ImageType::kSeparator;
}

sk_sp<SkColorFilter> DarkModeFilter::GenerateImageFilter(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  DCHECK(immutable_.settings.image_policy == DarkModeImagePolicy::kFilterSmart);
  DCHECK(immutable_.image_filter);

  return (immutable_.image_classifier->Classify(pixmap, src) ==
          DarkModeResult::kApplyFilter)
             ? immutable_.image_filter
             : nullptr;
}

sk_sp<SkColorFilter> DarkModeFilter::GetImageFilter() const {
  DCHECK(immutable_.image_filter);
  return immutable_.image_filter;
}

absl::optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags,
    ElementRole role) {
  if (!immutable_.color_filter)
    return absl::nullopt;

  if (role_override_.has_value())
    role = role_override_.value();

  cc::PaintFlags dark_mode_flags = flags;
  if (flags.HasShader()) {
    PaintShader::Type shader_type = flags.getShader()->shader_type();
    if (shader_type != PaintShader::Type::kImage &&
        shader_type != PaintShader::Type::kPaintRecord) {
      dark_mode_flags.setColorFilter(
          immutable_.color_filter->ToSkColorFilter());
    }
  } else if (ShouldApplyToColor(flags.getColor(), role)) {
    dark_mode_flags.setColor(inverted_color_cache_->GetInvertedColor(
        immutable_.color_filter.get(), flags.getColor()));
  }

  return absl::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

bool DarkModeFilter::ShouldApplyToColor(SkColor color, ElementRole role) {
  switch (role) {
    case ElementRole::kText:
      DCHECK(immutable_.text_classifier);
      return immutable_.text_classifier->ShouldInvertColor(color) ==
             DarkModeResult::kApplyFilter;
    case ElementRole::kListSymbol:
      // TODO(prashant.n): Rename text_classifier to foreground_classifier,
      // so that same classifier can be used for all roles which are supposed
      // to be at foreground.
      DCHECK(immutable_.text_classifier);
      return immutable_.text_classifier->ShouldInvertColor(color) ==
             DarkModeResult::kApplyFilter;
    case ElementRole::kBackground:
      DCHECK(immutable_.background_classifier);
      return immutable_.background_classifier->ShouldInvertColor(color) ==
             DarkModeResult::kApplyFilter;
    case ElementRole::kSVG:
      // 1) Inline SVG images are considered as individual shapes and do not
      // have an Image object associated with them. So they do not go through
      // the regular image classification pipeline. Do not apply any filter to
      // the SVG shapes until there is a way to get the classification for the
      // entire image to which these shapes belong.

      // 2) Non-inline SVG images are already classified at this point and have
      // a filter applied if necessary.
      return false;
    default:
      return false;
  }
  NOTREACHED();
}

size_t DarkModeFilter::GetInvertedColorCacheSizeForTesting() {
  return inverted_color_cache_->size();
}

ScopedDarkModeElementRoleOverride::ScopedDarkModeElementRoleOverride(
    GraphicsContext* graphics_context,
    DarkModeFilter::ElementRole role)
    : graphics_context_(graphics_context) {
  previous_role_override_ =
      graphics_context_->GetDarkModeFilter()->role_override_;
  graphics_context_->GetDarkModeFilter()->role_override_ = role;
}

ScopedDarkModeElementRoleOverride::~ScopedDarkModeElementRoleOverride() {
  graphics_context_->GetDarkModeFilter()->role_override_ =
      previous_role_override_;
}

}  // namespace blink
