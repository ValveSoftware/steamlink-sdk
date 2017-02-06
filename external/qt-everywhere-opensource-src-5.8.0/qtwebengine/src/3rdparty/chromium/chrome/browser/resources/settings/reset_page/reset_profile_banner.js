// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-banner' is the banner shown for clearing profile
 * settings.
 */
Polymer({
  is: 'settings-reset-profile-banner',

  properties: {
    showResetProfileDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onCloseTap_: function() {
    settings.ResetBrowserProxyImpl.getInstance().onHideResetProfileBanner();
    this.remove();
  },

  /**
   * Creates and shows a <settings-reset-profile-dialog>.
   * @private
   */
  showDialog_: function(dialogName) {
    this.showResetProfileDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-reset-profile-dialog');
      dialog.open();
    }.bind(this));
  },

  onResetDone_: function() {
    this.showResetProfileDialog_ = false;
  },
});
