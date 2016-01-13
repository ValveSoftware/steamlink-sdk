// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * A confirmation overlay for disabling kiosk app bailout shortcut.
   * @constructor
   */
  function KioskDisableBailoutConfirm() {
  }

  cr.addSingletonGetter(KioskDisableBailoutConfirm);

  KioskDisableBailoutConfirm.prototype = {
    /**
     * Initialize the page.
     */
    initialize: function() {
      var overlay = $('kiosk-disable-bailout-confirm-overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleCancel);

      var el = $('kiosk-disable-bailout-shortcut');
      el.addEventListener('change', this.handleDisableBailoutShortcutChange_);

      $('kiosk-disable-bailout-confirm-button').onclick = function(e) {
        extensions.ExtensionSettings.showOverlay($('kiosk-apps-page'));
        chrome.send('setDisableBailoutShortcut', [true]);
      };
      $('kiosk-disable-bailout-cancel-button').onclick = this.handleCancel;
    },

    /** Handles overlay being canceled. */
    handleCancel: function() {
      extensions.ExtensionSettings.showOverlay($('kiosk-apps-page'));
      $('kiosk-disable-bailout-shortcut').checked = false;
    },

    /**
     * Custom change handler for the disable bailout shortcut checkbox.
     * It blocks the underlying pref being changed and brings up confirmation
     * alert to user.
     * @private
     */
    handleDisableBailoutShortcutChange_: function() {
      // Just set the pref if user un-checks the box.
      if (!$('kiosk-disable-bailout-shortcut').checked) {
        chrome.send('setDisableBailoutShortcut', [false]);
        return false;
      }

      // Otherwise, show the confirmation overlay.
      extensions.ExtensionSettings.showOverlay($(
          'kiosk-disable-bailout-confirm-overlay'));
      return true;
    }
  };

  // Export
  return {
    KioskDisableBailoutConfirm: KioskDisableBailoutConfirm
  };
});

