// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * CertificateRestoreOverlay class
   * Encapsulated handling of the 'enter restore password' overlay page.
   * @class
   */
  function CertificateRestoreOverlay() {
    OptionsPage.call(this, 'certificateRestore', '',
                     'certificateRestoreOverlay');
  }

  cr.addSingletonGetter(CertificateRestoreOverlay);

  CertificateRestoreOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('certificateRestoreCancelButton').onclick = function(event) {
        self.cancelRestore_();
      };
      $('certificateRestoreOkButton').onclick = function(event) {
        self.finishRestore_();
      };

      self.clearInputFields_();
    },

    /** @override */
    didShowPage: function() {
      $('certificateRestorePassword').focus();
    },

    /**
     * Clears any uncommitted input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      OptionsPage.closeOverlay();
    },

    /**
     * Attempt the restore operation.
     * The overlay will be left up with inputs disabled until the backend
     * finishes and dismisses it.
     * @private
     */
    finishRestore_: function() {
      chrome.send('importPersonalCertificatePasswordSelected',
                  [$('certificateRestorePassword').value]);
      $('certificateRestoreCancelButton').disabled = true;
      $('certificateRestoreOkButton').disabled = true;
    },

    /**
     * Cancel the restore operation.
     * @private
     */
    cancelRestore_: function() {
      chrome.send('cancelImportExportCertificate');
      this.dismissOverlay_();
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('certificateRestorePassword').value = '';
      $('certificateRestoreCancelButton').disabled = false;
      $('certificateRestoreOkButton').disabled = false;
    },
  };

  CertificateRestoreOverlay.show = function() {
    CertificateRestoreOverlay.getInstance().clearInputFields_();
    OptionsPage.navigateToPage('certificateRestore');
  };

  CertificateRestoreOverlay.dismiss = function() {
    CertificateRestoreOverlay.getInstance().dismissOverlay_();
  };

  // Export
  return {
    CertificateRestoreOverlay: CertificateRestoreOverlay
  };

});
