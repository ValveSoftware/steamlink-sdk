// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for dialogs that require saving preferences on
 *     confirm and resetting preference inputs on cancel.
 */

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Base class for settings dialogs.
   * @constructor
   * @param {string} name See OptionsPage constructor.
   * @param {string} title See OptionsPage constructor.
   * @param {string} pageDivName See OptionsPage constructor.
   * @param {HTMLInputElement} okButton The confirmation button element.
   * @param {HTMLInputElement} cancelButton The cancellation button element.
   * @extends {OptionsPage}
   */
  function SettingsDialog(name, title, pageDivName, okButton, cancelButton) {
    OptionsPage.call(this, name, title, pageDivName);
    this.okButton = okButton;
    this.cancelButton = cancelButton;
  }

  SettingsDialog.prototype = {
    __proto__: OptionsPage.prototype,

    /** @override */
    initializePage: function() {
      this.okButton.onclick = this.handleConfirm.bind(this);
      this.cancelButton.onclick = this.handleCancel.bind(this);
    },

    /**
     * Handles the confirm button by saving the dialog preferences.
     */
    handleConfirm: function() {
      OptionsPage.closeOverlay();

      var prefs = Preferences.getInstance();
      var els = this.pageDiv.querySelectorAll('[dialog-pref]');
      for (var i = 0; i < els.length; i++) {
        if (els[i].pref)
          prefs.commitPref(els[i].pref, els[i].metric);
      }
    },

    /**
     * Handles the cancel button by closing the overlay.
     */
    handleCancel: function() {
      OptionsPage.closeOverlay();

      var prefs = Preferences.getInstance();
      var els = this.pageDiv.querySelectorAll('[dialog-pref]');
      for (var i = 0; i < els.length; i++) {
        if (els[i].pref)
          prefs.rollbackPref(els[i].pref);
      }
    },
  };

  return {
    SettingsDialog: SettingsDialog
  };
});
