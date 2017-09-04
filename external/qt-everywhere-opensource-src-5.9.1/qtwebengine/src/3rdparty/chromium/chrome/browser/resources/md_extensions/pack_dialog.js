// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  function PackDialogDelegate() {}

  PackDialogDelegate.prototype = {
    /**
     * Opens a file browser for the user to select the root directory.
     * @return {Promise<string>} A promise that is resolved with the path the
     *     user selected.
     */
    choosePackRootDirectory: assertNotReached,

    /**
     * Opens a file browser for the user to select the private key file.
     * @return {Promise<string>} A promise that is resolved with the path the
     *     user selected.
     */
    choosePrivateKeyPath: assertNotReached,

    /**
     * Packs the extension into a .crx.
     * @param {string} rootPath
     * @param {string} keyPath
     */
    packExtension: assertNotReached,
  };

  var PackDialog = Polymer({
    is: 'extensions-pack-dialog',
    properties: {
      /** @type {extensions.PackDialogDelegate} */
      delegate: Object,

      /** @private */
      packDirectory_: String,

      /** @private */
      keyFile_: String,
    },

    show: function() {
      this.$$('dialog').showModal();
    },

    close: function() {
      this.$$('dialog').close();
    },

    /** @private */
    onRootBrowse_: function() {
      this.delegate.choosePackRootDirectory().then(function(path) {
        if (path)
          this.set('packDirectory_', path);
      }.bind(this));
    },

    /** @private */
    onKeyBrowse_: function() {
      this.delegate.choosePrivateKeyPath().then(function(path) {
        if (path)
          this.set('keyFile_', path);
      }.bind(this));
    },

    /** @private */
    onConfirmTap_: function() {
      this.delegate.packExtension(this.packDirectory_, this.keyFile_);
      this.close();
    },
  });

  return {PackDialog: PackDialog,
          PackDialogDelegate: PackDialogDelegate};
});
