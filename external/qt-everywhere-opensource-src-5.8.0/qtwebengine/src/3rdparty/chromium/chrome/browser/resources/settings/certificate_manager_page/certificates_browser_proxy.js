// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage certificates" section
 * to interact with the browser.
 */

/**
 * @typedef {{
 *   extractable: boolean,
 *   id: string,
 *   name: string,
 *   policy: boolean,
 *   readonly: boolean,
 *   untrusted: boolean,
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
var CertificateSubnode;

/**
 * A data structure describing a certificate that is currently being imported,
 * therefore it has no ID yet, but it has a name. Used within JS only.
 * @typedef {{
 *   name: string,
 * }}
 */
var NewCertificateSubNode;

/**
 * @typedef {{
 *   id: string,
 *   name: string,
 *   subnodes: !Array<!CertificateSubnode>
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
var Certificate;

/**
 * @typedef {{
 *   ssl: boolean,
 *   email: boolean,
 *   objSign: boolean
 * }}
 */
var CaTrustInfo;

/**
 * Generic error returned from C++ via a Promise reject callback.
 * @typedef {{
 *   title: string,
 *   description: string
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
var CertificatesError;

/**
 * Enumeration of all possible certificate types.
 * @enum {string}
 */
var CertificateType = {
  CA: 'ca',
  OTHER: 'other',
  PERSONAL: 'personal',
  SERVER: 'server',
};


/**
 * Error returned from C++ via a Promise reject callback, when some certificates
 * fail to be imported.
 * @typedef {{
 *   title: string,
 *   description: string,
 *   certificateErrors: !Array<{name: string, error: string}>
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
var CertificatesImportError;

cr.define('settings', function() {

  /** @interface */
  function CertificatesBrowserProxy() {}

  CertificatesBrowserProxy.prototype = {
    /**
     * Triggers 5 events in the following order
     * 1x 'certificates-model-ready' event.
     * 4x 'certificates-changed' event, one for each certificate category.
     */
    refreshCertificates: function() {},

    /** @param {string} id */
    viewCertificate: function(id) {},

    /** @param {string} id */
    exportCertificate: function(id) {},

    /**
     * @param {string} id
     * @return {!Promise} A promise resolved when the certificate has been
     *     deleted successfully or rejected with a CertificatesError.
     */
    deleteCertificate: function(id) {},

    /**
     * @param {string} id
     * @return {!Promise<!CaTrustInfo>}
     */
    getCaCertificateTrust: function(id) {},

    /**
     * @param {string} id
     * @param {boolean} ssl
     * @param {boolean} email
     * @param {boolean} objSign
     * @return {!Promise}
     */
    editCaCertificateTrust: function(id, ssl, email, objSign) {},

    cancelImportExportCertificate: function() {},

    /**
     * @param {string} id
     * @return {!Promise} A promise firing once the user has selected
     *     the export location. A prompt should be shown to asking for a
     *     password to use for encrypting the file. The password should be
     *     passed back via a call to
     *     exportPersonalCertificatePasswordSelected().
     */
    exportPersonalCertificate: function(id) {},

    /**
     * @param {string} password
     * @return {!Promise}
     */
    exportPersonalCertificatePasswordSelected: function(password) {},

    /**
     * @param {boolean} useHardwareBacked
     * @return {!Promise<boolean>} A promise firing once the user has selected
     *     the file to be imported. If true a password prompt should be shown to
     *     the user, and the password should be passed back via a call to
     *     importPersonalCertificatePasswordSelected().
     */
    importPersonalCertificate: function(useHardwareBacked) {},

    /**
     * @param {string} password
     * @return {!Promise}
     */
    importPersonalCertificatePasswordSelected: function(password) {},

    /**
     * @return {!Promise} A promise firing once the user has selected
     *     the file to be imported, or failing with CertificatesError.
     *     Upon success, a prompt should be shown to the user to specify the
     *     trust levels, and that information should be passed back via a call
     *     to importCaCertificateTrustSelected().
     */
    importCaCertificate: function() {},

    /**
     * @param {boolean} ssl
     * @param {boolean} email
     * @param {boolean} objSign
     * @return {!Promise} A promise firing once the trust level for the imported
     *     certificate has been successfully set. The promise is rejected if an
     *     error occurred with either a CertificatesError or
     *     CertificatesImportError.
     */
    importCaCertificateTrustSelected: function(ssl, email, objSign) {},

    /**
     * @return {!Promise} A promise firing once the certificate has been
     *     imported. The promise is rejected if an error occurred, with either
     *     a CertificatesError or CertificatesImportError.
     */
    importServerCertificate: function() {},
  };

  /**
   * @constructor
   * @implements {settings.CertificatesBrowserProxy}
   */
  function CertificatesBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(CertificatesBrowserProxyImpl);

  CertificatesBrowserProxyImpl.prototype = {
    /** @override */
    refreshCertificates: function() {
      chrome.send('refreshCertificates');
    },

    /** @override */
    viewCertificate: function(id) {
      chrome.send('viewCertificate', [id]);
    },

    /** @override */
    exportCertificate: function(id) {
      chrome.send('exportCertificate', [id]);
    },

    /** @override */
    deleteCertificate: function(id) {
      return cr.sendWithPromise('deleteCertificate', id);
    },

    /** @override */
    exportPersonalCertificate: function(id) {
      return cr.sendWithPromise('exportPersonalCertificate', id);
    },

    /** @override */
    exportPersonalCertificatePasswordSelected: function(password) {
      return cr.sendWithPromise(
          'exportPersonalCertificatePasswordSelected', password);
    },

    /** @override */
    importPersonalCertificate: function(useHardwareBacked) {
      return cr.sendWithPromise('importPersonalCertificate', useHardwareBacked);
    },

    /** @override */
    importPersonalCertificatePasswordSelected: function(password) {
      return cr.sendWithPromise(
          'importPersonalCertificatePasswordSelected', password);
    },

    /** @override */
    getCaCertificateTrust: function(id) {
      return cr.sendWithPromise('getCaCertificateTrust', id);
    },

    /** @override */
    editCaCertificateTrust: function(id, ssl, email, objSign) {
      return cr.sendWithPromise(
          'editCaCertificateTrust', id, ssl, email, objSign);
    },

    /** @override */
    importCaCertificateTrustSelected: function(ssl, email, objSign) {
      return cr.sendWithPromise(
          'importCaCertificateTrustSelected', ssl, email, objSign);
    },

    /** @override */
    cancelImportExportCertificate: function() {
      chrome.send('cancelImportExportCertificate');
    },

    /** @override */
    importCaCertificate: function() {
      return cr.sendWithPromise('importCaCertificate');
    },

    /** @override */
    importServerCertificate: function() {
      return cr.sendWithPromise('importServerCertificate');
    },
  };

  return {
    CertificatesBrowserProxy: CertificatesBrowserProxy,
    CertificatesBrowserProxyImpl: CertificatesBrowserProxyImpl,
  };
});
