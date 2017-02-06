// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
Polymer({
  is: 'settings-device-page',

  properties: {
    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Handler for tapping the Touchpad settings menu item.
   * @private
   */
  onTouchpadTap_: function() {
    this.$.pages.setSubpageChain(['touchpad']);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_: function() {
    this.$.pages.setSubpageChain(['keyboard']);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_: function() {
    this.$.pages.setSubpageChain(['display']);
  },
});
