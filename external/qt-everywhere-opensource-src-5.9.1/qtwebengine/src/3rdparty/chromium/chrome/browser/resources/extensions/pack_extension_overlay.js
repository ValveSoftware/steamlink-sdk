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

      $('pack-extension-dismiss').addEventListener('click', function() {
        cr.dispatchSimpleEvent(overlay, 'cancelOverlay');
      });
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
      chrome.developerPrivate.packDirectory(
          extensionPath, privateKeyPath, 0, this.onPackResponse_.bind(this));
    },

    /**
     * Utility function which asks the C++ to show a platform-specific file
     * select dialog, and set the value property of |node| to the selected path.
     * @param {chrome.developerPrivate.SelectType} selectType
     *     The type of selection to use.
     * @param {chrome.developerPrivate.FileType} fileType
     *     The type of file to select.
     * @param {HTMLInputElement} node The node to set the value of.
     * @private
     */
    showFileDialog_: function(selectType, fileType, node) {
      chrome.developerPrivate.choosePath(selectType, fileType, function(path) {
        // Last error is set if the user canceled the dialog.
        if (!chrome.runtime.lastError && path)
          node.value = path;
      });
    },

    /**
     * Handles the showing of the extension directory browser.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowseExtensionDir_: function(e) {
      this.showFileDialog_(
          chrome.developerPrivate.SelectType.FOLDER,
          chrome.developerPrivate.FileType.LOAD,
          /** @type {HTMLInputElement} */ ($('extension-root-dir')));
    },

    /**
     * Handles the showing of the extension private key file.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowsePrivateKey_: function(e) {
      this.showFileDialog_(
          chrome.developerPrivate.SelectType.FILE,
          chrome.developerPrivate.FileType.PEM,
          /** @type {HTMLInputElement} */ ($('extension-private-key')));
    },

    /**
     * Handles a response from a packDirectory call.
     * @param {chrome.developerPrivate.PackDirectoryResponse} response The
     *     response of the pack call.
     * @private
     */
    onPackResponse_: function(response) {
      /** @type {string} */
      var alertTitle;
      /** @type {string} */
      var alertOk;
      /** @type {string} */
      var alertCancel;
      /** @type {function()} */
      var alertOkCallback;
      /** @type {function()} */
      var alertCancelCallback;

      var closeAlert = function() {
        extensions.ExtensionSettings.showOverlay(null);
      };

      switch (response.status) {
        case chrome.developerPrivate.PackStatus.SUCCESS:
          alertTitle = loadTimeData.getString('packExtensionOverlay');
          alertOk = loadTimeData.getString('ok');
          alertOkCallback = closeAlert;
          // No 'Cancel' option.
          break;
        case chrome.developerPrivate.PackStatus.WARNING:
          alertTitle = loadTimeData.getString('packExtensionWarningTitle');
          alertOk = loadTimeData.getString('packExtensionProceedAnyway');
          alertCancel = loadTimeData.getString('cancel');
          alertOkCallback = function() {
            chrome.developerPrivate.packDirectory(
                response.item_path,
                response.pem_path,
                response.override_flags,
                this.onPackResponse_.bind(this));
            closeAlert();
          }.bind(this);
          alertCancelCallback = closeAlert;
          break;
        case chrome.developerPrivate.PackStatus.ERROR:
          alertTitle = loadTimeData.getString('packExtensionErrorTitle');
          alertOk = loadTimeData.getString('ok');
          alertOkCallback = function() {
            extensions.ExtensionSettings.showOverlay(
                $('pack-extension-overlay'));
          };
          // No 'Cancel' option.
          break;
        default:
          assertNotReached();
          return;
      }

      alertOverlay.setValues(alertTitle,
                             response.message,
                             alertOk,
                             alertCancel,
                             alertOkCallback,
                             alertCancelCallback);
      extensions.ExtensionSettings.showOverlay($('alertOverlay'));
    },
  };

  // Export
  return {
    PackExtensionOverlay: PackExtensionOverlay
  };
});
