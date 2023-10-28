// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"

#include <string>
#include <unordered_map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

namespace {

// Default values for dark mode settings.
const constexpr DarkModeInversionAlgorithm kDefaultDarkModeInversionAlgorithm =
    DarkModeInversionAlgorithm::kInvertLightnessLAB;
const constexpr DarkModeImagePolicy kDefaultDarkModeImagePolicy =
    DarkModeImagePolicy::kFilterNone;
const constexpr int kDefaultTextBrightnessThreshold = 150;
const constexpr int kDefaultBackgroundBrightnessThreshold = 205;
const constexpr bool kDefaultDarkModeIsGrayscale = false;
const constexpr float kDefaultDarkModeContrastPercent = 0.0f;
const constexpr float kDefaultDarkModeImageGrayscalePercent = 0.0f;

typedef std::unordered_map<std::string, std::string> SwitchParams;

SwitchParams ParseDarkModeSettings() {
  SwitchParams switch_params;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch("dark-mode-settings"))
    return switch_params;

  std::vector<std::string> param_values = base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "dark-mode-settings"),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto param_value : param_values) {
    std::vector<std::string> pair = base::SplitString(
        param_value, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (pair.size() == 2)
      switch_params[base::ToLowerASCII(pair[0])] = base::ToLowerASCII(pair[1]);
  }

  return switch_params;
}

template <typename T>
T GetIntegerSwitchParamValue(const SwitchParams& switch_params,
                             std::string param,
                             T default_value) {
  auto it = switch_params.find(base::ToLowerASCII(param));
  if (it == switch_params.end())
    return default_value;

  int result;
  return base::StringToInt(it->second, &result) ? static_cast<T>(result)
                                                : default_value;
}

float GetFloatSwitchParamValue(const SwitchParams& switch_params,
                               std::string param,
                               float default_value) {
  auto it = switch_params.find(base::ToLowerASCII(param));
  if (it == switch_params.end())
    return default_value;

  double result;
  return base::StringToDouble(it->second, &result) ? static_cast<float>(result)
                                                   : default_value;
}

DarkModeInversionAlgorithm GetMode(const SwitchParams& switch_params) {
  switch (features::kForceDarkInversionMethodParam.Get()) {
    case ForceDarkInversionMethod::kUseBlinkSettings:
      return GetIntegerSwitchParamValue<DarkModeInversionAlgorithm>(
          switch_params, "InversionAlgorithm",
          kDefaultDarkModeInversionAlgorithm);
    case ForceDarkInversionMethod::kCielabBased:
      return DarkModeInversionAlgorithm::kInvertLightnessLAB;
    case ForceDarkInversionMethod::kHslBased:
      return DarkModeInversionAlgorithm::kInvertLightness;
    case ForceDarkInversionMethod::kRgbBased:
      return DarkModeInversionAlgorithm::kInvertBrightness;
  }
  NOTREACHED();
}

DarkModeImagePolicy GetImagePolicy(const SwitchParams& switch_params) {
  switch (features::kForceDarkImageBehaviorParam.Get()) {
    case ForceDarkImageBehavior::kUseBlinkSettings:
      return GetIntegerSwitchParamValue<DarkModeImagePolicy>(
          switch_params, "ImagePolicy", kDefaultDarkModeImagePolicy);
    case ForceDarkImageBehavior::kInvertNone:
      return DarkModeImagePolicy::kFilterNone;
    case ForceDarkImageBehavior::kInvertSelectively:
      return DarkModeImagePolicy::kFilterSmart;
  }
}

int GetTextBrightnessThreshold(const SwitchParams& switch_params) {
  const int flag_value = base::GetFieldTrialParamByFeatureAsInt(
      features::kForceWebContentsDarkMode,
      features::kForceDarkTextLightnessThresholdParam.name, -1);
  return flag_value >= 0 ? flag_value
                         : GetIntegerSwitchParamValue<int>(
                               switch_params, "TextBrightnessThreshold",
                               kDefaultTextBrightnessThreshold);
}

int GetBackgroundBrightnessThreshold(const SwitchParams& switch_params) {
  const int flag_value = base::GetFieldTrialParamByFeatureAsInt(
      features::kForceWebContentsDarkMode,
      features::kForceDarkBackgroundLightnessThresholdParam.name, -1);
  return flag_value >= 0 ? flag_value
                         : GetIntegerSwitchParamValue<int>(
                               switch_params, "BackgroundBrightnessThreshold",
                               kDefaultBackgroundBrightnessThreshold);
}

template <typename T>
T Clamp(T value, T min_value, T max_value) {
  return std::max(min_value, std::min(value, max_value));
}

bool GetIncreaseTextContrast(const SwitchParams& switch_params) {
  switch (features::kForceDarkIncreaseTextContrastParam.Get()) {
    case ForceDarkIncreaseTextContrast::kUseBlinkSettings:
      return GetIntegerSwitchParamValue<int>(switch_params,
                                             "IncreaseTextContrast", 0);
    case ForceDarkIncreaseTextContrast::kFalse:
      return false;
    case ForceDarkIncreaseTextContrast::kTrue:
      return true;
  }
  NOTREACHED();
}

DarkModeSettings BuildDarkModeSettings() {
  SwitchParams switch_params = ParseDarkModeSettings();

  DarkModeSettings settings;
  settings.mode = Clamp<DarkModeInversionAlgorithm>(
      GetMode(switch_params), DarkModeInversionAlgorithm::kFirst,
      DarkModeInversionAlgorithm::kLast);
  settings.image_policy = Clamp<DarkModeImagePolicy>(
      GetImagePolicy(switch_params), DarkModeImagePolicy::kFirst,
      DarkModeImagePolicy::kLast);
  settings.text_brightness_threshold =
      Clamp<int>(GetTextBrightnessThreshold(switch_params), 0, 255);
  settings.background_brightness_threshold =
      Clamp<int>(GetBackgroundBrightnessThreshold(switch_params), 0, 255);
  settings.grayscale = GetIntegerSwitchParamValue<bool>(
      switch_params, "IsGrayScale", kDefaultDarkModeIsGrayscale);
  settings.contrast =
      Clamp<float>(GetFloatSwitchParamValue(switch_params, "ContrastPercent",
                                            kDefaultDarkModeContrastPercent),
                   -1.0f, 1.0f);
  settings.image_grayscale_percent = Clamp<float>(
      GetFloatSwitchParamValue(switch_params, "ImageGrayScalePercent",
                               kDefaultDarkModeImageGrayscalePercent),
      0.0f, 1.0f);

  settings.increase_text_contrast = GetIncreaseTextContrast(switch_params);

  return settings;
}

}  // namespace

const DarkModeSettings& GetCurrentDarkModeSettings() {
  static DarkModeSettings settings = BuildDarkModeSettings();
  return settings;
}

}  // namespace blink
