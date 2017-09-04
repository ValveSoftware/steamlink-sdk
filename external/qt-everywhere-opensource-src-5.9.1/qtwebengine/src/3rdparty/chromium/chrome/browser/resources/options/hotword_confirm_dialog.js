// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ConfirmDialog = options.ConfirmDialog;
  /** @const */ var SettingsDialog = options.SettingsDialog;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * A dialog that will pop up when the user attempts to set the value of the
   * Boolean |pref| to |true|, asking for confirmation. It will first check for
   * any errors and if any exist, not display the dialog but toggle the
   * indicator. Like its superclass, if the user clicks OK, the new value is
   * committed to Chrome. If the user clicks Cancel or leaves the settings page,
   * the new value is discarded.
   * @constructor
   * @extends {options.ConfirmDialog}
   */
  function HotwordConfirmDialog() {
    ConfirmDialog.call(this,
        'hotwordConfim',  // name
        loadTimeData.getString('hotwordConfirmOverlayTabTitle'),
        'hotword-confirm-overlay',  // pageDivName
        assertInstanceof($('hotword-confirm-ok'), HTMLButtonElement),
        assertInstanceof($('hotword-confirm-cancel'), HTMLButtonElement),
        $('hotword-search-enable')['pref'], // pref
        $('hotword-search-enable')['metric']); // metric

    this.indicator = $('hotword-search-setting-indicator');
  }

  HotwordConfirmDialog.prototype = {
    // TODO(dbeam): this class should probably derive SettingsDialog again as it
    // evily duplicates much of ConfirmDialog's functionality, calls methods
    // on SettingsDialog.prototype, and shadows private method names.
    __proto__: ConfirmDialog.prototype,

    /**
     * Handle changes to |pref|. Only uncommitted changes are relevant as these
     * originate from user and need to be explicitly committed to take effect.
     * Pop up the dialog if there are no errors from the indicator or commit the
     * change, depending on whether confirmation is needed.
     * @param {Event} event Change event.
     * @private
     */
    onPrefChanged_: function(event) {
      if (!event.value.uncommitted)
        return;

      if (event.value.value && !this.confirmed_) {
        // If there is an error, show the indicator icon with more information.
        if (this.indicator.errorText)
          this.indicator.updateBasedOnError();

        // Show confirmation dialog (regardless of whether there is an error).
        PageManager.showPageByName(this.name, false);
      } else {
        Preferences.getInstance().commitPref(this.pref, this.metric);
      }
    },

    /**
     * Override the initializePage function so that an updated version of
     * onPrefChanged_ can be used.
     * @override
     */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);

      this.okButton.onclick = this.handleConfirm.bind(this);
      this.cancelButton.onclick = this.handleCancel.bind(this);
      Preferences.getInstance().addEventListener(
          this.pref, this.onPrefChanged_.bind(this));
    }
  };

  return {
    HotwordConfirmDialog: HotwordConfirmDialog
  };
});
