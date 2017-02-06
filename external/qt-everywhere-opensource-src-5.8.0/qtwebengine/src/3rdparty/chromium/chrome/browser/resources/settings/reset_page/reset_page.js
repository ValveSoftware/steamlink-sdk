// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-reset-page prefs="{{prefs}}">
 *      </settings-reset-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-reset-page',

  properties: {
    allowPowerwash_: {
      type: Boolean,
      value: cr.isChromeOS ? loadTimeData.getBoolean('allowPowerwash') : false
    },
  },

  /** @private */
  onShowResetProfileDialog_: function() {
    this.showDialog_('settings-reset-profile-dialog');
  },

  /** @private */
  onShowPowerwashDialog_: function() {
    this.showDialog_('settings-powerwash-dialog');
  },


  /**
   * Creates and shows the specified dialog.
   * @param {string} dialogName
   * @private
   */
  showDialog_: function(dialogName) {
    var dialog = document.createElement(dialogName);
    this.shadowRoot.appendChild(dialog);
    dialog.open();

    dialog.addEventListener('iron-overlay-closed', function() {
      dialog.remove();
    });
  },
});
