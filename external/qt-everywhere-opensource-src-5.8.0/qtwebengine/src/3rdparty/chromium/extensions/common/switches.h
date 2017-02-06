// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SWITCHES_H_
#define EXTENSIONS_COMMON_SWITCHES_H_

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
namespace extensions {

namespace switches {

extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowLegacyExtensionManifests[];
extern const char kDisableDesktopCaptureAudio[];
extern const char kDisableDesktopCapturePickerOldUI[];
extern const char kDisableTabForDesktopShare[];
extern const char kEmbeddedExtensionOptions[];
extern const char kEnableAppsShowOnFirstPaint[];
extern const char kEnableAppWindowControls[];
extern const char kEnableEmbeddedExtensionOptions[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableExtensionActionRedesign[];
extern const char kEnableMojoSerialService[];
extern const char kEnableOverrideBookmarksUI[];
extern const char kEnableBLEAdvertising[];
extern const char kEnableTabForDesktopShare[];
extern const char kErrorConsole[];
extern const char kExtensionActionRedesign[];
extern const char kExtensionProcess[];
extern const char kExtensionsOnChromeURLs[];
extern const char kForceDevModeHighlighting[];
extern const char kIsolateExtensions[];
extern const char kLoadApps[];
extern const char kScriptsRequireAction[];
extern const char kEnableScriptsRequireAction[];
#if defined(CHROMIUM_BUILD)
extern const char kPromptForExternalExtensions[];
#endif
extern const char kShowComponentExtensionOptions[];
extern const char kTraceAppSource[];
extern const char kWhitelistedExtensionID[];
extern const char kEnableCrxHashCheck[];

}  // namespace switches

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SWITCHES_H_
