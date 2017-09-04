// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for settings_ui.
 */

/**
 * Specifies page visibility in guest mode in cr and cros.
 * @typedef {{
 *   advancedSettings: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   dateTime: (boolean|undefined|DateTimePageVisibility),
 *   defaultBrowser: (boolean|undefined),
 *   downloads: (undefined|DownloadsPageVisibility),
 *   onStartup: (boolean|undefined),
 *   passwordsAndForms: (boolean|undefined),
 *   people: (boolean|undefined),
 *   privacy: (undefined|PrivacyPageVisibility),
 *   reset:(boolean|undefined),
 * }}
 */
var GuestModePageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
var AppearancePageVisibility;

/**
 * @typedef {{
 *   timeZoneSelector: boolean,
 * }}
 */
var DateTimePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean
 * }}
 */
var DownloadsPageVisibility;

/**
 * @typedef {{
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 * }}
 */
var PrivacyPageVisibility;

// TODO(mahmadi): Dummy code for closure compiler to process this file.
(function foo() {})();
