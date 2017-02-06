// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-default-browser-page' is the settings page that contains
 * settings to change the default browser (i.e. which the OS will open).
 */
Polymer({
  is: 'settings-default-browser-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * A message about whether Chrome is the default browser.
     */
    message_: {
      type: String,
    },

    /**
     * Indicates if the next updateDefaultBrowserState_ invocation is following
     * a call to SettingsDefaultBrowser.setAsDefaultBrowser().
     */
    startedSetAsDefault_: {
      type: Boolean,
      value: false,
    },

    /**
     * Show or hide an error indicator showing whether SetAsDefault succeeded.
     */
    showError_: {
      type: Boolean,
      value: false,
    },

    /**
     * Only show the SetAsDefault button if we have permission to set it.
     */
    showButton_: {
      type: Boolean,
    },
  },

  ready: function() {
    var self = this;
    cr.define('Settings', function() {
      return {
        updateDefaultBrowserState: function() {
          return self.updateDefaultBrowserState_.apply(self, arguments);
        },
      };
    });
    chrome.send('SettingsDefaultBrowser.requestDefaultBrowserState');
  },

  /**
   * @param {boolean} isDefault Whether Chrome is currently the user's default
   *   browser.
   * @param {boolean} canBeDefault Whether Chrome can be the default browser on
   *   this system.
   * @private
   */
  updateDefaultBrowserState_: function(isDefault, canBeDefault) {
    if (this.startedSetAsDefault_ && !isDefault) {
      this.startedSetAsDefault_ = false;
      this.showError_ = true;
    } else {
      this.showError_ = false;
    }

    this.showButton_ = !isDefault && canBeDefault;
    if (!this.showButton_) {
      this.message_ = loadTimeData.getString(
          canBeDefault ? 'defaultBrowserDefault' : 'defaultBrowserUnknown');
    }
  },

  /** @private */
  onSetDefaultBrowserTap_: function() {
    this.startedSetAsDefault_ = true;
    chrome.send('SettingsDefaultBrowser.setAsDefaultBrowser');
  },
});
