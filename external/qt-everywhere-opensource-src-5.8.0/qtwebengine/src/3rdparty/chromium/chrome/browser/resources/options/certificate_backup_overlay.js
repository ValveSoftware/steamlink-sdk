// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * CertificateBackupOverlay class
   * Encapsulated handling of the 'enter backup password' overlay page.
   * @class
   */
  function CertificateBackupOverlay() {
    Page.call(this, 'certificateBackupOverlay', '', 'certificateBackupOverlay');
  }

  cr.addSingletonGetter(CertificateBackupOverlay);

  CertificateBackupOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var self = this;
      $('certificateBackupCancelButton').onclick = function(event) {
        self.cancelBackup_();
      };
      $('certificateBackupOkButton').onclick = function(event) {
        self.finishBackup_();
      };
      var onBackupPasswordInput = function(event) {
        self.comparePasswords_();
      };
      $('certificateBackupPassword').oninput = onBackupPasswordInput;
      $('certificateBackupPassword2').oninput = onBackupPasswordInput;

      self.clearInputFields_();
    },

    /**
     * Clears any uncommitted input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      PageManager.closeOverlay();
    },

    /**
     * Attempt the Backup operation.
     * The overlay will be left up with inputs disabled until the backend
     * finishes and dismisses it.
     * @private
     */
    finishBackup_: function() {
      chrome.send('exportPersonalCertificatePasswordSelected',
                  [$('certificateBackupPassword').value]);
      $('certificateBackupCancelButton').disabled = true;
      $('certificateBackupOkButton').disabled = true;
      $('certificateBackupPassword').disabled = true;
      $('certificateBackupPassword2').disabled = true;
    },

    /**
     * Cancel the Backup operation.
     * @private
     */
    cancelBackup_: function() {
      chrome.send('cancelImportExportCertificate');
      this.dismissOverlay_();
    },

    /**
     * Compares the password fields and sets the button state appropriately.
     * @private
     */
    comparePasswords_: function() {
      var password1 = $('certificateBackupPassword').value;
      var password2 = $('certificateBackupPassword2').value;
      $('certificateBackupOkButton').disabled =
          !password1 || password1 != password2;
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('certificateBackupPassword').value = '';
      $('certificateBackupPassword2').value = '';
      $('certificateBackupPassword').disabled = false;
      $('certificateBackupPassword2').disabled = false;
      $('certificateBackupCancelButton').disabled = false;
      $('certificateBackupOkButton').disabled = true;
    },
  };

  CertificateBackupOverlay.show = function() {
    CertificateBackupOverlay.getInstance().clearInputFields_();
    PageManager.showPageByName('certificateBackupOverlay');
  };

  CertificateBackupOverlay.dismiss = function() {
    CertificateBackupOverlay.getInstance().dismissOverlay_();
  };

  // Export
  return {
    CertificateBackupOverlay: CertificateBackupOverlay
  };
});
