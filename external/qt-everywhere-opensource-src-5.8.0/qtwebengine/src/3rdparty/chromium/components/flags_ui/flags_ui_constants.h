// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_
#define COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_

namespace flags_ui {

// Resource paths.
// Must match the resource file names.
extern const char kFlagsJS[];

// Message handlers.
// Must match the constants used in the resource files.
extern const char kEnableExperimentalFeature[];
extern const char kRequestExperimentalFeatures[];
extern const char kResetAllFlags[];
extern const char kRestartBrowser[];

// Other values.
// Must match the constants used in the resource files.
extern const char kChannelPromoBeta[];
extern const char kChannelPromoDev[];
extern const char kDisable[];
extern const char kEnable[];
extern const char kFlagsBlurb[];
extern const char kFlagsLongTitle[];
extern const char kFlagsNoExperimentsAvailable[];
extern const char kFlagsNotSupported[];
extern const char kFlagsRestartButton[];
extern const char kFlagsRestartNotice[];
extern const char kFlagsTableTitle[];
extern const char kFlagsUnsupportedTableTitle[];
extern const char kFlagsWarningHeader[];
extern const char kNeedsRestart[];
extern const char kOwnerWarning[];
extern const char kResetAllButton[];
extern const char kReturnExperimentalFeatures[];
extern const char kShowBetaChannelPromotion[];
extern const char kShowDevChannelPromotion[];
extern const char kShowOwnerWarning[];
extern const char kSupportedFeatures[];
extern const char kUnsupportedFeatures[];

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_
