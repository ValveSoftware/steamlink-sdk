// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_command_list.js">

cr.define('extensions', function() {
  'use strict';

  // The Extension Commands list object that will be used to show the commands
  // on the page.
  var ExtensionCommandList = extensions.ExtensionCommandList;

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

      this.extensionCommandList_ = new ExtensionCommandList(
          /**@type {HTMLDivElement} */($('extension-command-list')));

      $('extension-commands-dismiss').addEventListener('click', function() {
        cr.dispatchSimpleEvent(overlay, 'cancelOverlay');
      });

      // The ExtensionList will update us with its data, so we don't need to
      // handle that here.
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
   * @param {!Array<chrome.developerPrivate.ExtensionInfo>} extensionsData
   */
  ExtensionCommandsOverlay.updateExtensionsData = function(extensionsData) {
    var overlay = ExtensionCommandsOverlay.getInstance();
    overlay.extensionCommandList_.setData(extensionsData);

    var hasAnyCommands = false;
    for (var i = 0; i < extensionsData.length; ++i) {
      if (extensionsData[i].commands.length > 0) {
        hasAnyCommands = true;
        break;
      }
    }

    // Make sure the config link is visible, since there are commands to show
    // and potentially configure.
    document.querySelector('.extension-commands-config').hidden =
        !hasAnyCommands;

    $('no-commands').hidden = hasAnyCommands;
    overlay.extensionCommandList_.classList.toggle(
        'empty-extension-commands-list', !hasAnyCommands);
  };

  // Export
  return {
    ExtensionCommandsOverlay: ExtensionCommandsOverlay
  };
});
