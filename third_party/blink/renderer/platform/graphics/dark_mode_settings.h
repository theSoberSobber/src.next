// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_

namespace blink {

enum class DarkModeInversionAlgorithm {
  // For testing only, does a simple 8-bit invert of every RGB pixel component.
  kSimpleInvertForTesting,
  kInvertBrightness,
  kInvertLightness,
  kInvertLightnessLAB,

  kFirst = kSimpleInvertForTesting,  // First enum value.
  kLast = kInvertLightnessLAB,       // Last enum value.
};

enum class DarkModeImagePolicy {
  kFilterAll,    // Apply dark-mode filter to all images.
  kFilterNone,   // Never apply dark-mode filter to any images.
  kFilterSmart,  // Apply dark-mode based on image content.

  kFirst = kFilterAll,   // First enum value.
  kLast = kFilterSmart,  // Last enum value.
};

// New variables added to this struct should be considered in
// third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h
struct DarkModeSettings {
  DarkModeInversionAlgorithm mode =
      DarkModeInversionAlgorithm::kInvertLightnessLAB;
  bool grayscale = false;
  float image_grayscale_percent = 0.0;  // Valid range from 0.0 to 1.0
  float contrast = 0.0;                 // Valid range from -1.0 to 1.0
  DarkModeImagePolicy image_policy = DarkModeImagePolicy::kFilterNone;

  // Text colors with brightness below this threshold will be inverted, and
  // above it will be left as in the original, non-dark-mode page.  Set to 256
  // to always invert text color or to 0 to never invert text color.
  int text_brightness_threshold = 256;

  // Background elements with brightness above this threshold will be inverted,
  // and below it will be left as in the original, non-dark-mode page.  Set to
  // 256 to never invert the color or to 0 to always invert it.
  //
  // Warning: This behavior is the opposite of text_brightness_threshold!
  int background_brightness_threshold = 0;

  // True if text contrast should be increased by painting an outline.
  bool increase_text_contrast = false;

  // True if the user interface is dark / black
  bool is_dark_ui = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
