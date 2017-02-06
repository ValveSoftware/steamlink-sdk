// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user for a decryption password such that
 * a previously exported personal certificate can be imported.
 */
Polymer({
  is: 'settings-certificate-password-decryption-dialog',

  properties: {
    /** @private {!settings.CertificatesBrowserProxy} */
    browserProxy_: Object,

    /** @private */
    password_: {
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
    this.browserProxy_.importPersonalCertificatePasswordSelected(
        this.password_).then(
            function() {
              /** @type {!CrDialogElement} */ (this.$.dialog).close();
            }.bind(this),
            /** @param {!CertificatesError} error */
            function(error) {
              /** @type {!CrDialogElement} */ (this.$.dialog).close();
              this.fire('certificates-error', error);
            }.bind(this));
  },
});
