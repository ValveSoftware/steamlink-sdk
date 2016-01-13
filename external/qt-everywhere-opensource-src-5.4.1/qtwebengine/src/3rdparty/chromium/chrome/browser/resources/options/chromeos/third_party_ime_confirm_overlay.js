// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * HomePageOverlay class
   * Dialog that allows users to set the home page.
   * @extends {SettingsDialog}
   */
  function ThirdPartyImeConfirmOverlay() {
    SettingsDialog.call(
        this, 'thirdPartyImeConfirm',
        loadTimeData.getString('thirdPartyImeConfirmOverlayTabTitle'),
        'third-party-ime-confirm-overlay',
        $('third-party-ime-confirm-ok'),
        $('third-party-ime-confirm-cancel'));
  }

  cr.addSingletonGetter(ThirdPartyImeConfirmOverlay);

  ThirdPartyImeConfirmOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

   /**
    * Callback to authorize use of an input method.
    * @type {Function}
    * @private
    */
   confirmationCallback_: null,

   /**
    * Callback to cancel enabling an input method.
    * @type {Function}
    * @private
    */
   cancellationCallback_: null,

    /**
     * Confirms enabling of a third party IME.
     */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);
      this.confirmationCallback_();
    },

    /**
     * Resets state of the checkobx.
     */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      this.cancellationCallback_();
    },

    /**
     * Displays a confirmation dialog indicating the risk fo enabling
     * a third party IME.
     * @param {{extension: string, confirm: Function, cancel: Function}} data
     *     Options for the confirmation dialog.
     * @private
     */
    showConfirmationDialog_: function(data) {
      this.confirmationCallback_ = data.confirm;
      this.cancellationCallback_ = data.cancel;
      var message = loadTimeData.getStringF('thirdPartyImeConfirmMessage',
                                             data.extension);
      $('third-party-ime-confirm-text').textContent = message;
      OptionsPage.showPageByName(this.name, false);
    },
  };

  /**
   * Displays a confirmation dialog indicating the risk fo enabling
   * a third party IME.
   * @param {{extension: string, confirm: Function, cancel: Function}} data
   *     Options for the confirmation dialog.
   */
  ThirdPartyImeConfirmOverlay.showConfirmationDialog = function(data) {
    ThirdPartyImeConfirmOverlay.getInstance().showConfirmationDialog_(data);
  };

  // Export
  return {
    ThirdPartyImeConfirmOverlay: ThirdPartyImeConfirmOverlay
  };
});
