// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/permission_context_utils.h"

#include "build/build_config.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/permissions/contexts/accessibility_permission_context.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/contexts/clipboard_read_write_permission_context.h"
#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/midi_permission_context.h"
#include "components/permissions/contexts/midi_sysex_permission_context.h"
#include "components/permissions/contexts/nfc_permission_context.h"
#include "components/permissions/contexts/payment_handler_permission_context.h"
#include "components/permissions/contexts/sensor_permission_context.h"
#include "components/permissions/contexts/wake_lock_permission_context.h"
#include "components/permissions/contexts/webxr_permission_context.h"

#if defined(OS_ANDROID)
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/permissions/contexts/nfc_permission_context_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MAC)
#include "components/permissions/contexts/geolocation_permission_context_mac.h"
#endif  // defined(OS_MAC)

namespace embedder_support {

PermissionContextDelegates::PermissionContextDelegates() = default;

PermissionContextDelegates::PermissionContextDelegates(
    PermissionContextDelegates&&) = default;

PermissionContextDelegates& PermissionContextDelegates::operator=(
    PermissionContextDelegates&&) = default;

PermissionContextDelegates::~PermissionContextDelegates() = default;

permissions::PermissionManager::PermissionContextMap
CreateDefaultPermissionContexts(content::BrowserContext* browser_context,
                                PermissionContextDelegates delegates) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;

  DCHECK(delegates.geolocation_permission_context_delegate);
#if defined(OS_MAC)
  DCHECK(delegates.geolocation_manager);
#endif  // defined(OS_MAC)
  DCHECK(delegates.media_stream_device_enumerator);
  DCHECK(delegates.nfc_permission_context_delegate);

  permission_contexts[ContentSettingsType::ACCESSIBILITY_EVENTS] =
      std::make_unique<permissions::AccessibilityPermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::AR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          browser_context, ContentSettingsType::AR);
  permission_contexts[ContentSettingsType::BACKGROUND_SYNC] =
      std::make_unique<BackgroundSyncPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::CAMERA_PAN_TILT_ZOOM] =
      std::make_unique<permissions::CameraPanTiltZoomPermissionContext>(
          browser_context, delegates.media_stream_device_enumerator);
  permission_contexts[ContentSettingsType::CLIPBOARD_READ_WRITE] =
      std::make_unique<permissions::ClipboardReadWritePermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::CLIPBOARD_SANITIZED_WRITE] =
      std::make_unique<permissions::ClipboardSanitizedWritePermissionContext>(
          browser_context);
#if defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextAndroid>(
          browser_context,
          std::move(delegates.geolocation_permission_context_delegate));
#elif defined(OS_MAC)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextMac>(
          browser_context,
          std::move(delegates.geolocation_permission_context_delegate),
          delegates.geolocation_manager);
#else
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContext>(
          browser_context,
          std::move(delegates.geolocation_permission_context_delegate));
#endif
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<permissions::MidiPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<permissions::MidiSysexPermissionContext>(
          browser_context);
#if defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContextAndroid>(
          browser_context,
          std::move(delegates.nfc_permission_context_delegate));
#else
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContext>(
          browser_context,
          std::move(delegates.nfc_permission_context_delegate));
#endif  // defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::PAYMENT_HANDLER] =
      std::make_unique<payments::PaymentHandlerPermissionContext>(
          browser_context);
  permission_contexts[ContentSettingsType::SENSORS] =
      std::make_unique<permissions::SensorPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::VR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          browser_context, ContentSettingsType::VR);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SCREEN] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          browser_context, ContentSettingsType::WAKE_LOCK_SCREEN);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SYSTEM] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          browser_context, ContentSettingsType::WAKE_LOCK_SYSTEM);

  return permission_contexts;
}

}  // namespace embedder_support
