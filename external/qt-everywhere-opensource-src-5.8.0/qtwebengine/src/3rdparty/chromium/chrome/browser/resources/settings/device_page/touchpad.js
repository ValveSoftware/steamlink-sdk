// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-touchpad' is the settings subpage with touchpad settings.
 */
Polymer({
  is: 'settings-touchpad',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Prevents the link from activating its parent paper-radio-button.
   * @param {!Event} e
   * @private
   */
  onLearnMoreLinkActivated_: function(e) {
    settings.DevicePageBrowserProxyImpl.getInstance().handleLinkEvent(e);
  },
});
