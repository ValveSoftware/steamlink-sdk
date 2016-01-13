// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * PackExtensionOverlay class
   * Encapsulated handling of the 'Pack Extension' overlay page.
   * @constructor
   */
  function PackExtensionOverlay() {
  }

  cr.addSingletonGetter(PackExtensionOverlay);

  PackExtensionOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('pack-extension-dismiss').addEventListener('click',
          this.handleDismiss_.bind(this));
      $('pack-extension-commit').addEventListener('click',
          this.handleCommit_.bind(this));
      $('browse-extension-dir').addEventListener('click',
          this.handleBrowseExtensionDir_.bind(this));
      $('browse-private-key').addEventListener('click',
          this.handleBrowsePrivateKey_.bind(this));
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     */
    handleDismiss_: function(e) {
      extensions.ExtensionSettings.showOverlay(null);
    },

    /**
     * Handles a click on the pack button.
     * @param {Event} e The click event.
     */
    handleCommit_: function(e) {
      var extensionPath = $('extension-root-dir').value;
      var privateKeyPath = $('extension-private-key').value;
      chrome.send('pack', [extensionPath, privateKeyPath, 0]);
    },

    /**
     * Utility function which asks the C++ to show a platform-specific file
     * select dialog, and fire |callback| with the |filePath| that resulted.
     * |selectType| can be either 'file' or 'folder'. |operation| can be 'load'
     * or 'pem' which are signals to the C++ to do some operation-specific
     * configuration.
     * @private
     */
    showFileDialog_: function(selectType, operation, callback) {
      handleFilePathSelected = function(filePath) {
        callback(filePath);
        handleFilePathSelected = function() {};
      };

      chrome.send('packExtensionSelectFilePath', [selectType, operation]);
    },

    /**
     * Handles the showing of the extension directory browser.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowseExtensionDir_: function(e) {
      this.showFileDialog_('folder', 'load', function(filePath) {
        $('extension-root-dir').value = filePath;
      });
    },

    /**
     * Handles the showing of the extension private key file.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowsePrivateKey_: function(e) {
      this.showFileDialog_('file', 'pem', function(filePath) {
        $('extension-private-key').value = filePath;
      });
    },
  };

  /**
   * Wrap up the pack process by showing the success |message| and closing
   * the overlay.
   * @param {string} message The message to show to the user.
   */
  PackExtensionOverlay.showSuccessMessage = function(message) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionOverlay'),
        message,
        loadTimeData.getString('ok'),
        '',
        function() {
          extensions.ExtensionSettings.showOverlay(null);
        },
        null);
    extensions.ExtensionSettings.showOverlay($('alertOverlay'));
  };

  /**
   * Post an alert overlay showing |message|, and upon acknowledgement, close
   * the alert overlay and return to showing the PackExtensionOverlay.
   */
  PackExtensionOverlay.showError = function(message) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionErrorTitle'),
        message,
        loadTimeData.getString('ok'),
        '',
        function() {
          extensions.ExtensionSettings.showOverlay($('pack-extension-overlay'));
        },
        null);
    extensions.ExtensionSettings.showOverlay($('alertOverlay'));
  };

  // Export
  return {
    PackExtensionOverlay: PackExtensionOverlay
  };
});
