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
   * Shows a <settings-reset-profile-dialog>.
   * @private
   */
  showDialog_: function() {
    this.showResetProfileDialog_ = true;
  },

  /** @private */
  onDialogClose_: function() {
    this.showResetProfileDialog_ = false;
  },
});
