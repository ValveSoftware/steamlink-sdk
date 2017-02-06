// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include <stddef.h>

#include "base/macros.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "extensions/strings/grit/extensions_strings.h"

namespace extensions {

namespace {

const char kWindowsPermission[] = "windows";

template <typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ChromeAPIPermissions::GetAllPermissions()
    const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      // Register permissions for all extension types.
      {APIPermission::kBackground, "background"},
      {APIPermission::kDeclarativeContent, "declarativeContent"},
      {APIPermission::kDesktopCapture, "desktopCapture"},
      {APIPermission::kDesktopCapturePrivate, "desktopCapturePrivate"},
      {APIPermission::kDownloads, "downloads"},
      {APIPermission::kDownloadsOpen, "downloads.open"},
      {APIPermission::kDownloadsShelf, "downloads.shelf"},
      {APIPermission::kEasyUnlockPrivate, "easyUnlockPrivate"},
      {APIPermission::kIdentity, "identity"},
      {APIPermission::kIdentityEmail, "identity.email"},
      {APIPermission::kExperimental, "experimental",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kEmbeddedExtensionOptions, "embeddedExtensionOptions",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kGeolocation, "geolocation",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kNotifications, "notifications"},
      {APIPermission::kGcdPrivate, "gcdPrivate"},
      {APIPermission::kGcm, "gcm"},
      {APIPermission::kNotificationProvider, "notificationProvider"},

      // Register extension permissions.
      {APIPermission::kAccessibilityFeaturesModify,
       "accessibilityFeatures.modify"},
      {APIPermission::kAccessibilityFeaturesRead, "accessibilityFeatures.read"},
      {APIPermission::kAccessibilityPrivate, "accessibilityPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kActiveTab, "activeTab"},
      {APIPermission::kAudioModem, "audioModem"},
      {APIPermission::kBookmark, "bookmarks"},
      {APIPermission::kBrailleDisplayPrivate, "brailleDisplayPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kBrowsingData, "browsingData"},
      {APIPermission::kCertificateProvider, "certificateProvider"},
      {APIPermission::kContentSettings, "contentSettings"},
      {APIPermission::kContextMenus, "contextMenus"},
      {APIPermission::kCookie, "cookies"},
      {APIPermission::kCopresence, "copresence"},
      {APIPermission::kCopresencePrivate, "copresencePrivate"},
      {APIPermission::kCryptotokenPrivate, "cryptotokenPrivate"},
      {APIPermission::kDataReductionProxy, "dataReductionProxy",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kEnterpriseDeviceAttributes,
       "enterprise.deviceAttributes"},
      {APIPermission::kEnterprisePlatformKeys, "enterprise.platformKeys"},
      {APIPermission::kFileBrowserHandler, "fileBrowserHandler",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kFontSettings, "fontSettings",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kHistory, "history"},
      {APIPermission::kIdltest, "idltest"},
      {APIPermission::kInput, "input"},
      {APIPermission::kManagement, "management"},
      {APIPermission::kMDns, "mdns", APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kNativeMessaging, "nativeMessaging"},
      {APIPermission::kPlatformKeys, "platformKeys"},
      {APIPermission::kPrivacy, "privacy"},
      {APIPermission::kProcesses, "processes"},
      {APIPermission::kSessions, "sessions"},
      {APIPermission::kSignedInDevices, "signedInDevices"},
      {APIPermission::kSyncFileSystem, "syncFileSystem"},
      {APIPermission::kTab, "tabs"},
      {APIPermission::kTopSites, "topSites"},
      {APIPermission::kTts, "tts", APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kTtsEngine, "ttsEngine",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWallpaper, "wallpaper",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebNavigation, "webNavigation"},

      // Register private permissions.
      {APIPermission::kScreenlockPrivate, "screenlockPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kActivityLogPrivate, "activityLogPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kAutoTestPrivate, "autotestPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kCast, "cast", APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kCommandsAccessibility, "commands.accessibility",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kCommandLinePrivate, "commandLinePrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDeveloperPrivate, "developerPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDial, "dial", APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDownloadsInternal, "downloadsInternal"},
      {APIPermission::kExperienceSamplingPrivate, "experienceSamplingPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kFileManagerPrivate, "fileManagerPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kHotwordPrivate, "hotwordPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kIdentityPrivate, "identityPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kLogPrivate, "logPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebcamPrivate, "webcamPrivate"},
      {APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kMediaRouterPrivate, "mediaRouterPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kMetricsPrivate, "metricsPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kMusicManagerPrivate, "musicManagerPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kPreferencesPrivate, "preferencesPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kSystemPrivate, "systemPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kInputMethodPrivate, "inputMethodPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kEchoPrivate, "echoPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kFeedbackPrivate, "feedbackPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kImageWriterPrivate, "imageWriterPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kRtcPrivate, "rtcPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kTerminalPrivate, "terminalPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWallpaperPrivate, "wallpaperPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebstorePrivate, "webstorePrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kStreamsPrivate, "streamsPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kEnterprisePlatformKeysPrivate,
       "enterprise.platformKeysPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebrtcAudioPrivate, "webrtcAudioPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebrtcDesktopCapturePrivate,
       "webrtcDesktopCapturePrivate", APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebrtcLoggingPrivate, "webrtcLoggingPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kFirstRunPrivate, "firstRunPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kInlineInstallPrivate, "inlineInstallPrivate"},
      {APIPermission::kSettingsPrivate, "settingsPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kAutofillPrivate, "autofillPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWebstoreWidgetPrivate, "webstoreWidgetPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kPasswordsPrivate, "passwordsPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kUsersPrivate, "usersPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kLanguageSettingsPrivate, "languageSettingsPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kResourcesPrivate, "resourcesPrivate",
       APIPermissionInfo::kFlagCannotBeOptional},

      // Full url access permissions.
      {APIPermission::kDebugger, "debugger",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDevtools, "devtools",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal},
      {APIPermission::kPageCapture, "pageCapture",
       APIPermissionInfo::kFlagImpliesFullURLAccess},
      {APIPermission::kTabCapture, "tabCapture",
       APIPermissionInfo::kFlagImpliesFullURLAccess},
      {APIPermission::kTabCaptureForTab, "tabCaptureForTab",
       APIPermissionInfo::kFlagInternal},
      {APIPermission::kPlugin, "plugin",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::kFlagImpliesFullAccess |
           APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal},
      {APIPermission::kProxy, "proxy",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::kFlagCannotBeOptional},

      // Platform-app permissions.

      // The permission string for "fileSystem" is only shown when
      // "write" or "directory" is present. Read-only access is only
      // granted after the user has been shown a file or directory
      // chooser dialog and selected a file or directory. Selecting
      // the file or directory is considered consent to read it.
      {APIPermission::kFileSystem, "fileSystem"},
      {APIPermission::kFileSystemDirectory, "fileSystem.directory"},
      {APIPermission::kFileSystemProvider, "fileSystemProvider"},
      {APIPermission::kFileSystemRequestFileSystem,
       "fileSystem.requestFileSystem"},
      {APIPermission::kFileSystemRetainEntries, "fileSystem.retainEntries"},
      {APIPermission::kFileSystemWrite, "fileSystem.write"},
      {APIPermission::kMediaGalleries, "mediaGalleries",
       APIPermissionInfo::kFlagNone,
       &CreateAPIPermission<MediaGalleriesPermission>},
      {APIPermission::kPointerLock, "pointerLock"},
      {APIPermission::kCastStreaming, "cast.streaming"},
      {APIPermission::kBrowser, "browser"},
      {APIPermission::kLauncherSearchProvider, "launcherSearchProvider"},

      // Settings override permissions.
      {APIPermission::kHomepage, "homepage",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal,
       &CreateAPIPermission<SettingsOverrideAPIPermission>},
      {APIPermission::kSearchProvider, "searchProvider",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal,
       &CreateAPIPermission<SettingsOverrideAPIPermission>},
      {APIPermission::kStartupPages, "startupPages",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal,
       &CreateAPIPermission<SettingsOverrideAPIPermission>},
  };

  std::vector<APIPermissionInfo*> permissions;

  for (size_t i = 0; i < arraysize(permissions_to_register); ++i)
    permissions.push_back(new APIPermissionInfo(permissions_to_register[i]));
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
ChromeAPIPermissions::GetAllAliases() const {
  // Register aliases.
  std::vector<PermissionsProvider::AliasInfo> aliases;
  aliases.push_back(PermissionsProvider::AliasInfo("tabs", kWindowsPermission));
  return aliases;
}

}  // namespace extensions
