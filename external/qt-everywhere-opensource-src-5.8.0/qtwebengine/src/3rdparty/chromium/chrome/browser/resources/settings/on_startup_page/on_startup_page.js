// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <settings-on-startup-page prefs="{{prefs}}">
 *      </settings-on-startup-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 */
Polymer({
  is: 'settings-on-startup-page',

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
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Enum values for the 'session.restore_on_startup' preference.
     * @private {!Object<string, number>}
     */
    prefValues_: {
      readOnly: true,
      type: Object,
      value: {
        OPEN_NEW_TAB: 5,
        CONTINUE: 1,
        OPEN_SPECIFIC: 4,
      },
    },
  },

  /**
    * Determine whether to show the user defined startup pages.
    * @param {number} restoreOnStartup Enum value from prefValues_.
    * @return {boolean} Whether the open specific pages is selected.
    * @private
    */
  showStartupUrls_: function(restoreOnStartup) {
    return restoreOnStartup == this.prefValues_.OPEN_SPECIFIC;
  },
});
