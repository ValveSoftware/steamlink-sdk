// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 */
Polymer({
  is: 'settings-powerwash-dialog',

  open: function() {
    settings.ResetBrowserProxyImpl.getInstance().onPowerwashDialogShow();
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onRestartTap_: function() {
    settings.LifetimeBrowserProxyImpl.getInstance().factoryReset();
  },
});
