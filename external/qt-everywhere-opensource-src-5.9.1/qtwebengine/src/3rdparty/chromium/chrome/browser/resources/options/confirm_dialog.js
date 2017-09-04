// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * A dialog that will pop up when the user attempts to set the value of the
   * Boolean |pref| to |true|, asking for confirmation. If the user clicks OK,
   * the new value is committed to Chrome. If the user clicks Cancel or leaves
   * the settings page, the new value is discarded.
   * @constructor
   * @param {string} name See Page constructor.
   * @param {string} title See Page constructor.
   * @param {string} pageDivName See Page constructor.
   * @param {HTMLButtonElement} okButton The confirmation button element.
   * @param {HTMLButtonElement} cancelButton The cancellation button element.
   * @param {string} pref The pref that requires confirmation.
   * @param {string} metric User metrics identifier.
   * @param {string=} opt_confirmedPref A pref used to remember whether the
   *     user has confirmed the dialog before. This ensures that the user is
   *     presented with the dialog only once. If left |undefined|, the dialog
   *     will pop up every time the user attempts to set |pref| to |true|.
   * @param {boolean=} opt_confirmValue The value to which changing should
   *     trigger the confirmation dialog.  Defaults to |true| if left
   *     |undefined|.
   * @extends {options.SettingsDialog}
   */
  function ConfirmDialog(name, title, pageDivName, okButton, cancelButton, pref,
                         metric, opt_confirmedPref, opt_confirmValue) {
    SettingsDialog.call(this, name, title, pageDivName, okButton, cancelButton);

    /** @protected */
    this.pref = pref;

    /** @protected */
    this.metric = metric;

    /** @private */
    this.confirmedPref_ = opt_confirmedPref;

    /** @private */
    this.confirmed_ = false;

    /** @private */
    this.confirmValue_ = opt_confirmValue === false ? false : true;
  }

  ConfirmDialog.prototype = {
    // Set up the prototype chain
    __proto__: SettingsDialog.prototype,

    /**
     * Handle changes to |pref|. Only uncommitted changes are relevant as these
     * originate from user and need to be explicitly committed to take effect.
     * Pop up the dialog or commit the change, depending on whether confirmation
     * is needed.
     * @param {Event} event Change event.
     * @private
     */
    onPrefChanged_: function(event) {
      if (!event.value.uncommitted)
        return;

      if (event.value.value == this.confirmValue_ && !this.confirmed_)
        PageManager.showPageByName(this.name, false);
      else
        Preferences.getInstance().commitPref(this.pref, this.metric);
    },

    /**
     * Handle changes to |confirmedPref_| by caching them.
     * @param {Event} event Change event.
     * @private
     */
    onConfirmedChanged_: function(event) {
      this.confirmed_ = event.value.value;
    },

    /** @override */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);

      this.okButton.onclick = this.handleConfirm.bind(this);
      this.cancelButton.onclick = this.handleCancel.bind(this);
      Preferences.getInstance().addEventListener(
          this.pref, this.onPrefChanged_.bind(this));
      if (this.confirmedPref_) {
        Preferences.getInstance().addEventListener(
            this.confirmedPref_, this.onConfirmedChanged_.bind(this));
      }
    },

    /**
     * Handle the confirm button by committing the |pref| change. If
     * |confirmedPref_| has been specified, also remember that the dialog has
     * been confirmed to avoid bringing it up in the future.
     * @override
     */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);

      Preferences.getInstance().commitPref(this.pref, this.metric);
      if (this.confirmedPref_)
        Preferences.setBooleanPref(
            this.confirmedPref_, this.confirmValue_, true);
    },

    /**
     * Handle the cancel button by rolling back the |pref| change without it
     * ever taking effect.
     * @override
     */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      Preferences.getInstance().rollbackPref(this.pref);
    },

    /**
     * When a user navigates away from a confirm dialog, treat as a cancel.
     * @protected
     * @override
     */
    willHidePage: function() {
      if (this.visible)
        Preferences.getInstance().rollbackPref(this.pref);
    },
  };

  return {
    ConfirmDialog: ConfirmDialog
  };
});
