// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/extensions_strings.h"

namespace extensions {

namespace {

const char kOldAlwaysOnTopWindowsPermission[] = "alwaysOnTopWindows";
const char kOldFullscreenPermission[] = "fullscreen";
const char kOldOverrideEscFullscreenPermission[] = "overrideEscFullscreen";
const char kOldUnlimitedStoragePermission[] = "unlimited_storage";

template <typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ExtensionsAPIPermissions::GetAllPermissions()
    const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      {APIPermission::kAlarms, "alarms"},
      {APIPermission::kAlphaEnabled, "app.window.alpha"},
      {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop"},
      {APIPermission::kAppView,
       "appview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kAudio, "audio"},
      {APIPermission::kAudioCapture, "audioCapture"},
      {APIPermission::kBluetoothPrivate,
       "bluetoothPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kClipboardRead,
       "clipboardRead",
       APIPermissionInfo::kFlagSupportsContentCapabilities},
      {APIPermission::kClipboardWrite,
       "clipboardWrite",
       APIPermissionInfo::kFlagSupportsContentCapabilities},
      {APIPermission::kDeclarativeWebRequest, "declarativeWebRequest"},
      {APIPermission::kDiagnostics,
       "diagnostics",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDisplaySource, "displaySource"},
      {APIPermission::kDns, "dns"},
      {APIPermission::kDocumentScan, "documentScan"},
      {APIPermission::kExtensionView,
       "extensionview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kExternallyConnectableAllUrls,
       "externally_connectable.all_urls"},
      {APIPermission::kFullscreen, "app.window.fullscreen"},
      {APIPermission::kHid, "hid"},
      {APIPermission::kImeWindowEnabled, "app.window.ime"},
      {APIPermission::kOverrideEscFullscreen,
       "app.window.fullscreen.overrideEsc"},
      {APIPermission::kIdle, "idle"},
      {APIPermission::kNetworkingConfig, "networking.config"},
      {APIPermission::kNetworkingPrivate,
       "networkingPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kPower, "power"},
      {APIPermission::kPrinterProvider, "printerProvider"},
      {APIPermission::kSerial, "serial"},
      {APIPermission::kSocket,
       "socket",
       APIPermissionInfo::kFlagCannotBeOptional,
       &CreateAPIPermission<SocketPermission>},
      {APIPermission::kStorage, "storage"},
      {APIPermission::kSystemCpu, "system.cpu"},
      {APIPermission::kSystemMemory, "system.memory"},
      {APIPermission::kSystemNetwork, "system.network"},
      {APIPermission::kSystemDisplay, "system.display"},
      {APIPermission::kSystemStorage, "system.storage"},
      {APIPermission::kU2fDevices, "u2fDevices"},
      {APIPermission::kUnlimitedStorage,
       "unlimitedStorage",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagSupportsContentCapabilities},
      {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone},
      {APIPermission::kUsbDevice,
       "usbDevices",
       APIPermissionInfo::kFlagNone,
       &CreateAPIPermission<UsbDevicePermission>},
      {APIPermission::kVideoCapture, "videoCapture"},
      {APIPermission::kVpnProvider,
       "vpnProvider",
       APIPermissionInfo::kFlagCannotBeOptional},
      // NOTE(kalman): This is provided by a manifest property but needs to
      // appear in the install permission dialogue, so we need a fake
      // permission for it. See http://crbug.com/247857.
      {APIPermission::kWebConnectable,
       "webConnectable",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal},
      {APIPermission::kWebRequest, "webRequest"},
      {APIPermission::kWebRequestBlocking, "webRequestBlocking"},
      {APIPermission::kWebView,
       "webview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWindowShape, "app.window.shape"},
  };

  std::vector<APIPermissionInfo*> permissions;
  for (size_t i = 0; i < arraysize(permissions_to_register); ++i)
    permissions.push_back(new APIPermissionInfo(permissions_to_register[i]));
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
ExtensionsAPIPermissions::GetAllAliases() const {
  std::vector<PermissionsProvider::AliasInfo> aliases;
  aliases.push_back(PermissionsProvider::AliasInfo(
      "app.window.alwaysOnTop", kOldAlwaysOnTopWindowsPermission));
  aliases.push_back(PermissionsProvider::AliasInfo("app.window.fullscreen",
                                                   kOldFullscreenPermission));
  aliases.push_back(
      PermissionsProvider::AliasInfo("app.window.fullscreen.overrideEsc",
                                     kOldOverrideEscFullscreenPermission));
  aliases.push_back(PermissionsProvider::AliasInfo(
      "unlimitedStorage", kOldUnlimitedStoragePermission));
  return aliases;
}

}  // namespace extensions
