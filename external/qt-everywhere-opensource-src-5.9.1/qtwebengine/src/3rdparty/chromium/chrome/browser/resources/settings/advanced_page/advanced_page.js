// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-advanced-page' is the settings page containing the advanced
 * settings.
 */
Polymer({
  is: 'settings-advanced-page',

  behaviors: [SettingsPageVisibility, MainPageBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
