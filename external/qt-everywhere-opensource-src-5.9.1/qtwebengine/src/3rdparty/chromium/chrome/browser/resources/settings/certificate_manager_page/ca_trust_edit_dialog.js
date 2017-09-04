// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ca-trust-edit-dialog' allows the user to
 *  - specify the trust level of a certificate authority that is being
 *    imported.
 *  - edit the trust level of an already existing certificate authority.
 */
Polymer({
  is: 'settings-ca-trust-edit-dialog',

  properties: {
    /** @private {!settings.CertificatesBrowserProxy} */
    browserProxy_: Object,

    /** @type {!CertificateSubnode|!NewCertificateSubNode} */
    model: Object,

    /** @private {?CaTrustInfo} */
    trustInfo_: Object,

    /** @private {string} */
    explanationText_: String,
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation',
        this.model.name);

    // A non existing |model.id| indicates that a new certificate is being
    // imported, otherwise an existing certificate is being edited.
    if (this.model.id) {
      this.browserProxy_.getCaCertificateTrust(this.model.id).then(
          /** @param {!CaTrustInfo} trustInfo */
          function(trustInfo) {
            this.trustInfo_ = trustInfo;
            this.$.dialog.showModal();
          }.bind(this));
    } else {
      /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
    }
  },

  /** @private */
  onCancelTap_: function() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /** @private */
  onOkTap_: function() {
    this.$.spinner.active = true;

    var whenDone = this.model.id ?
        this.browserProxy_.editCaCertificateTrust(
            this.model.id, this.$.ssl.checked,
            this.$.email.checked, this.$.objSign.checked) :
        this.browserProxy_.importCaCertificateTrustSelected(
            this.$.ssl.checked, this.$.email.checked, this.$.objSign.checked);

    whenDone.then(function() {
      this.$.spinner.active = false;
      /** @type {!CrDialogElement} */ (this.$.dialog).close();
    }.bind(this),
    /** @param {!CertificatesError} error */
    function(error) {
      /** @type {!CrDialogElement} */ (this.$.dialog).close();
      this.fire('certificates-error', error);
    }.bind(this));
  },
});
