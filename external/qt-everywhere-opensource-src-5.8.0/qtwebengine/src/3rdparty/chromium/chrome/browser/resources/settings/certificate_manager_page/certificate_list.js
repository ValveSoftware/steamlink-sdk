// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-certificate-list' is an element that displays a list
 * of certificates.
 */
Polymer({
  is: 'settings-certificate-list',

  properties: {
    /** @type {!Array<!Certificate>} */
    certificates: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!CertificateType} */
    certificateType: String,
  },

  behaviors: [I18nBehavior],

  /**
   * @return {string}
   * @private
   */
  getDescription_: function() {
    if (this.certificates.length == 0)
      return this.i18n('certificateManagerNoCertificates');

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return this.i18n('certificateManagerYourCertificatesDescription');
      case CertificateType.SERVER:
        return this.i18n('certificateManagerServersDescription');
      case CertificateType.CA:
        return this.i18n('certificateManagerAuthoritiesDescription');
      case CertificateType.OTHER:
        return this.i18n('certificateManagerOthersDescription');
    }

    assertNotReached();
  },

  /**
   * @return {boolean}
   * @private
   */
  canImport_: function() {
    return this.certificateType != CertificateType.OTHER;
  },

  /**
   * Handles a rejected Promise returned from |browserProxy_|.
   * @param {*} error Expects {!CertificatesError|!CertificatesImportError}.
   * @private
   */
  onRejected_: function(error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // a native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', error);
  },


  /**
   * @param {?NewCertificateSubNode} subnode
   * @private
   */
  dispatchImportActionEvent_: function(subnode) {
    this.fire(
        settings.CertificateActionEvent,
        /** @type {!CertificateActionEventDetail} */ ({
          action: CertificateAction.IMPORT,
          subnode: subnode,
          certificateType: this.certificateType,
        }));
  },

  /** @private */
  onImportTap_: function() {
    var browserProxy = settings.CertificatesBrowserProxyImpl.getInstance();
    if (this.certificateType == CertificateType.PERSONAL) {
      browserProxy.importPersonalCertificate(false).then(
          function(showPasswordPrompt) {
            if (showPasswordPrompt)
              this.dispatchImportActionEvent_(null);
          }.bind(this),
          this.onRejected_.bind(this));
    } else if (this.certificateType == CertificateType.CA) {
      browserProxy.importCaCertificate().then(
          function(certificateName) {
            this.dispatchImportActionEvent_({name: certificateName});
          }.bind(this),
          this.onRejected_.bind(this));
    } else if (this.certificateType == CertificateType.SERVER) {
      browserProxy.importServerCertificate().catch(
          this.onRejected_.bind(this));
    } else {
      assertNotReached();
    }
  },
});
