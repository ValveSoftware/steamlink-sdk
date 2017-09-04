// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_UI_CONSTANTS_H_
#define COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_UI_CONSTANTS_H_

namespace physical_web_ui {

// Resource paths.
// Must match the resource file names.
extern const char kPhysicalWebJS[];
extern const char kPhysicalWebCSS[];

// Message handlers.
// Must match the constants used in the resource files.
extern const char kRequestNearbyUrls[];
extern const char kPhysicalWebItemClicked[];

// Strings.
// Must match the constants used in the resource files.
extern const char kTitle[];
extern const char kPageInfo[];
extern const char kPageInfoDescription[];
extern const char kPageInfoIcon[];
extern const char kPageInfoTitle[];
extern const char kResolvedUrl[];
extern const char kScannedUrl[];
extern const char kIndex[];
extern const char kReturnNearbyUrls[];
extern const char kMetadata[];

}  // namespace physical_web_ui

#endif  // COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_UI_CONSTANTS_H_
