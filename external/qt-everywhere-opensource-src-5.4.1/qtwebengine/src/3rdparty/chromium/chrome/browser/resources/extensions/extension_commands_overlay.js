// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_command_list.js"></include>

cr.define('extensions', function() {
  'use strict';

  // The Extension Commands list object that will be used to show the commands
  // on the page.
  var ExtensionCommandList = options.ExtensionCommandList;

  /**
   * ExtensionCommandsOverlay class
   * Encapsulated handling of the 'Extension Commands' overlay page.
   * @constructor
   */
  function ExtensionCommandsOverlay() {
  }

  cr.addSingletonGetter(ExtensionCommandsOverlay);

  ExtensionCommandsOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('extension-commands-dismiss').addEventListener('click',
          this.handleDismiss_.bind(this));

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionCommandsRequestExtensionsData');
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     */
    handleDismiss_: function(e) {
      extensions.ExtensionSettings.showOverlay(null);
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of extension commands.
   */
  ExtensionCommandsOverlay.returnExtensionsData = function(extensionsData) {
    ExtensionCommandList.prototype.data_ = extensionsData;
    var extensionCommandList = $('extension-command-list');
    ExtensionCommandList.decorate(extensionCommandList);

    // Make sure the config link is visible, since there are commands to show
    // and potentially configure.
    document.querySelector('.extension-commands-config').hidden =
        extensionsData.commands.length == 0;

    $('no-commands').hidden = extensionsData.commands.length > 0;
    var list = $('extension-command-list');
    if (extensionsData.commands.length == 0)
      list.classList.add('empty-extension-commands-list');
    else
      list.classList.remove('empty-extension-commands-list');
  }

  // Export
  return {
    ExtensionCommandsOverlay: ExtensionCommandsOverlay
  };
});
