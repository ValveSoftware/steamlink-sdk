// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
Polymer({
  is: 'settings-a11y-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

<if expr="chromeos">
  /** @private */
  onManageAccessibilityFeaturesTap_: function() {
    settings.navigateTo(settings.Route.MANAGE_ACCESSIBILITY);
  },
</if>

  /** @private */
  onMoreFeaturesTap_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },
});
