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

  behaviors: [SettingsPageVisibility, RoutableBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current active route.
     * @type {SettingsRoute}
     */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @type {string} Selector to get the sections.
   * TODO(michaelpg): replace duplicate docs with @override once b/24294625
   * is fixed.
   */
  sectionSelector: 'settings-section',
});
