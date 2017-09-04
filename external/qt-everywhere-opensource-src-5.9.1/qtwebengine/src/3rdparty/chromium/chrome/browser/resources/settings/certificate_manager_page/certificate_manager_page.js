// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-certificate-manager-page' is the settings page
 * containing SSL certificate settings.
 */
Polymer({
  is: 'settings-certificate-manager-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {number} */
    selected: {
      type: Number,
      value: 0,
    },

    /** @type {!Array<!Certificate>} */
    personalCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    serverCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    caCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    otherCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @private */
    certificateTypeEnum_: {
      type: Object,
      value: CertificateType,
      readOnly: true,
    },

    /** @private */
    showCaTrustEditDialog_: Boolean,

    /** @private */
    showDeleteConfirmationDialog_: Boolean,

    /** @private */
    showPasswordEncryptionDialog_: Boolean,

    /** @private */
    showPasswordDecryptionDialog_: Boolean,

    /** @private */
    showErrorDialog_: Boolean,

    /**
     * The model to be passed to dialogs that refer to a given certificate.
     * @private {?CertificateSubnode}
     */
    dialogModel_: Object,

    /**
     * The certificate type to be passed to dialogs that refer to a given
     * certificate.
     * @private {?CertificateType}
     */
    dialogModelCertificateType_: String,

    /**
     * The model to be passed to the error dialog.
     * @private {null|!CertificatesError|!CertificatesImportError}
     */
    errorDialogModel_: Object,
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('certificates-changed', this.set.bind(this));
    settings.CertificatesBrowserProxyImpl.getInstance().refreshCertificates();
  },

  /**
   * @param {number} selectedIndex
   * @param {number} tabIndex
   * @return {boolean} Whether to show tab at |tabIndex|.
   * @private
   */
  isTabSelected_: function(selectedIndex, tabIndex) {
    return selectedIndex == tabIndex;
  },

  /** @override */
  ready: function() {
    this.addEventListener(settings.CertificateActionEvent, function(event) {
      this.dialogModel_ = event.detail.subnode;
      this.dialogModelCertificateType_ = event.detail.certificateType;

      if (event.detail.action == CertificateAction.IMPORT) {
        if (event.detail.certificateType == CertificateType.PERSONAL) {
          this.openDialog_(
              'settings-certificate-password-decryption-dialog',
              'showPasswordDecryptionDialog_');
        } else if (event.detail.certificateType ==
            CertificateType.CA) {
          this.openDialog_(
              'settings-ca-trust-edit-dialog', 'showCaTrustEditDialog_');
        }
      } else {
        if (event.detail.action == CertificateAction.EDIT) {
          this.openDialog_(
              'settings-ca-trust-edit-dialog', 'showCaTrustEditDialog_');
        } else if (event.detail.action == CertificateAction.DELETE) {
          this.openDialog_(
              'settings-certificate-delete-confirmation-dialog',
              'showDeleteConfirmationDialog_');
        } else if (event.detail.action ==
            CertificateAction.EXPORT_PERSONAL) {
          this.openDialog_(
              'settings-certificate-password-encryption-dialog',
              'showPasswordEncryptionDialog_');
        }
      }

      event.stopPropagation();
    }.bind(this));

    this.addEventListener('certificates-error', function(event) {
      this.errorDialogModel_ = event.detail;
      this.openDialog_(
          'settings-certificates-error-dialog',
          'showErrorDialog_');
      event.stopPropagation();
    }.bind(this));
  },

  /**
   * Opens a dialog and registers a listener for removing the dialog from the
   * DOM once is closed. The listener is destroyed when the dialog is removed
   * (because of 'restamp').
   *
   * @param {string} dialogTagName The tag name of the dialog to be shown.
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the dialog.
   * @private
   */
  openDialog_: function(dialogTagName, domIfBooleanName) {
    this.set(domIfBooleanName, true);
    this.async(function() {
      var dialog = this.$$(dialogTagName);
      dialog.addEventListener('close', function() {
        this.set(domIfBooleanName, false);
      }.bind(this));
    }.bind(this));
  },
});
