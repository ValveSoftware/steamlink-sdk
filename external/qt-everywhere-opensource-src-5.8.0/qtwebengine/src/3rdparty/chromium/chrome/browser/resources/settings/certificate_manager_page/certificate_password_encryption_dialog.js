// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user to encrypt a personal certificate
 * before it is exported to disk.
 */
Polymer({
  is: 'settings-certificate-password-encryption-dialog',

  properties: {
    /** @private {!settings.CertificatesBrowserProxy} */
    browserProxy_: Object,

    /** @type {!CertificateSubnode} */
    model: Object,

    /** @private */
    password_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmPassword_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    /** @type {!CrDialogElement} */ (this.$.dialog).open();
  },

  /** @private */
  onCancelTap_: function() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /** @private */
  onOkTap_: function() {
    this.browserProxy_.exportPersonalCertificatePasswordSelected(
        this.password_).then(
            function() {
              this.$.dialog.close();
            }.bind(this),
            /** @param {!CertificatesError} error */
            function(error) {
              this.$.dialog.close();
              this.fire('certificates-error', error);
            }.bind(this));
  },

  /** @private */
  validate_: function() {
    var isValid = this.password_ != '' &&
        this.password_ == this.confirmPassword_;
    this.$.ok.disabled = !isValid;
  },
});
