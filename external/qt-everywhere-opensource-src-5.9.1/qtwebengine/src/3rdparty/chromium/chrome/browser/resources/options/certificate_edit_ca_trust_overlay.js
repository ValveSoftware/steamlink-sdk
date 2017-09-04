// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * CertificateEditCaTrustOverlay class
   * Encapsulated handling of the 'edit ca trust' and 'import ca' overlay pages.
   * @class
   */
  function CertificateEditCaTrustOverlay() {
    Page.call(this, 'certificateEditCaTrustOverlay', '',
              'certificateEditCaTrustOverlay');
  }

  cr.addSingletonGetter(CertificateEditCaTrustOverlay);

  CertificateEditCaTrustOverlay.prototype = {
    __proto__: Page.prototype,

    /**
     * Dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      PageManager.closeOverlay();
    },

    /**
     * Enables or disables input fields.
     * @private
     */
    enableInputs_: function(enabled) {
      $('certificateCaTrustSSLCheckbox').disabled =
      $('certificateCaTrustEmailCheckbox').disabled =
      $('certificateCaTrustObjSignCheckbox').disabled =
      $('certificateEditCaTrustCancelButton').disabled =
      $('certificateEditCaTrustOkButton').disabled = !enabled;
    },

    /**
     * Attempt the Edit operation.
     * The overlay will be left up with inputs disabled until the backend
     * finishes and dismisses it.
     * @private
     */
    finishEdit_: function() {
      // TODO(mattm): Send checked values as booleans.  For now send them as
      // strings, since WebUIBindings::send does not support any other types :(
      chrome.send('editCaCertificateTrust',
                  [this.certId,
                   $('certificateCaTrustSSLCheckbox').checked.toString(),
                   $('certificateCaTrustEmailCheckbox').checked.toString(),
                   $('certificateCaTrustObjSignCheckbox').checked.toString()]);
      this.enableInputs_(false);
    },

    /**
     * Cancel the Edit operation.
     * @private
     */
    cancelEdit_: function() {
      this.dismissOverlay_();
    },

    /**
     * Attempt the Import operation.
     * The overlay will be left up with inputs disabled until the backend
     * finishes and dismisses it.
     * @private
     */
    finishImport_: function() {
      // TODO(mattm): Send checked values as booleans.  For now send them as
      // strings, since WebUIBindings::send does not support any other types :(
      chrome.send('importCaCertificateTrustSelected',
                  [$('certificateCaTrustSSLCheckbox').checked.toString(),
                   $('certificateCaTrustEmailCheckbox').checked.toString(),
                   $('certificateCaTrustObjSignCheckbox').checked.toString()]);
      this.enableInputs_(false);
    },

    /**
     * Cancel the Import operation.
     * @private
     */
    cancelImport_: function() {
      chrome.send('cancelImportExportCertificate');
      this.dismissOverlay_();
    },
  };

  /**
   * Callback from CertificateManagerHandler with the trust values.
   * @param {boolean} trustSSL The initial value of SSL trust checkbox.
   * @param {boolean} trustEmail The initial value of Email trust checkbox.
   * @param {boolean} trustObjSign The initial value of Object Signing trust.
   */
  CertificateEditCaTrustOverlay.populateTrust = function(
      trustSSL, trustEmail, trustObjSign) {
    $('certificateCaTrustSSLCheckbox').checked = trustSSL;
    $('certificateCaTrustEmailCheckbox').checked = trustEmail;
    $('certificateCaTrustObjSignCheckbox').checked = trustObjSign;
    CertificateEditCaTrustOverlay.getInstance().enableInputs_(true);
  };

  /**
   * Show the Edit CA Trust overlay.
   * @param {string} certId The id of the certificate to be passed to the
   * certificate manager model.
   * @param {string} certName The display name of the certificate.
   * checkbox.
   */
  CertificateEditCaTrustOverlay.show = function(certId, certName) {
    var self = CertificateEditCaTrustOverlay.getInstance();
    self.certId = certId;
    $('certificateEditCaTrustCancelButton').onclick = function(event) {
      self.cancelEdit_();
    };
    $('certificateEditCaTrustOkButton').onclick = function(event) {
      self.finishEdit_();
    };
    $('certificateEditCaTrustDescription').textContent =
        loadTimeData.getStringF('certificateEditCaTrustDescriptionFormat',
                                certName);
    self.enableInputs_(false);
    PageManager.showPageByName('certificateEditCaTrustOverlay');
    chrome.send('getCaCertificateTrust', [certId]);
  };

  /**
   * Show the Import CA overlay.
   * @param {string} certName The display name of the certificate.
   * checkbox.
   */
  CertificateEditCaTrustOverlay.showImport = function(certName) {
    var self = CertificateEditCaTrustOverlay.getInstance();
    // TODO(mattm): do we want a view certificate button here like firefox has?
    $('certificateEditCaTrustCancelButton').onclick = function(event) {
      self.cancelImport_();
    };
    $('certificateEditCaTrustOkButton').onclick = function(event) {
      self.finishImport_();
    };
    $('certificateEditCaTrustDescription').textContent =
        loadTimeData.getStringF('certificateImportCaDescriptionFormat',
                                certName);
    CertificateEditCaTrustOverlay.populateTrust(false, false, false);
    PageManager.showPageByName('certificateEditCaTrustOverlay');
  };

  CertificateEditCaTrustOverlay.dismiss = function() {
    CertificateEditCaTrustOverlay.getInstance().dismissOverlay_();
  };

  // Export
  return {
    CertificateEditCaTrustOverlay: CertificateEditCaTrustOverlay
  };
});
