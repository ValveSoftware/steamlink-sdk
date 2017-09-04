// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="shortcut_util.js">

cr.define('extensions', function() {
  'use strict';

  /**
   * Creates a new list of extension commands.
   * @param {HTMLDivElement} div
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionCommandList(div) {
    div.__proto__ = ExtensionCommandList.prototype;
    return div;
  }

  ExtensionCommandList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * While capturing, this records the current (last) keyboard event generated
     * by the user. Will be |null| after capture and during capture when no
     * keyboard event has been generated.
     * @type {KeyboardEvent}.
     * @private
     */
    currentKeyEvent_: null,

    /**
     * While capturing, this keeps track of the previous selection so we can
     * revert back to if no valid assignment is made during capture.
     * @type {string}.
     * @private
     */
    oldValue_: '',

    /**
     * While capturing, this keeps track of which element the user asked to
     * change.
     * @type {HTMLElement}.
     * @private
     */
    capturingElement_: null,

    /**
     * Updates the extensions data for the overlay.
     * @param {!Array<chrome.developerPrivate.ExtensionInfo>} data The extension
     *     data.
     */
    setData: function(data) {
      /** @private {!Array<chrome.developerPrivate.ExtensionInfo>} */
      this.data_ = data;

      this.textContent = '';

      // Iterate over the extension data and add each item to the list.
      this.data_.forEach(this.createNodeForExtension_.bind(this));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |extension|.
     * @param {chrome.developerPrivate.ExtensionInfo} extension A dictionary of
     *     extension metadata.
     * @private
     */
    createNodeForExtension_: function(extension) {
      if (extension.commands.length == 0 ||
          extension.state == chrome.developerPrivate.ExtensionState.DISABLED)
        return;

      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-extension-item-wrapper');
      var node = template.cloneNode(true);

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      this.appendChild(node);

      // Iterate over the commands data within the extension and add each item
      // to the list.
      extension.commands.forEach(
          this.createNodeForCommand_.bind(this, extension.id));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |command|.
     * @param {string} extensionId The associated extension's id.
     * @param {chrome.developerPrivate.Command} command A dictionary of
     *     extension command metadata.
     * @private
     */
    createNodeForCommand_: function(extensionId, command) {
      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-command-item-wrapper');
      var node = template.cloneNode(true);
      node.id = this.createElementId_('command', extensionId, command.name);

      var description = node.querySelector('.command-description');
      description.textContent = command.description;

      var shortcutNode = node.querySelector('.command-shortcut-text');
      shortcutNode.addEventListener('mouseup',
                                    this.startCapture_.bind(this));
      shortcutNode.addEventListener('focus', this.handleFocus_.bind(this));
      shortcutNode.addEventListener('blur', this.handleBlur_.bind(this));
      shortcutNode.addEventListener('keydown', this.handleKeyDown_.bind(this));
      shortcutNode.addEventListener('keyup', this.handleKeyUp_.bind(this));
      if (!command.isActive) {
        shortcutNode.textContent =
            loadTimeData.getString('extensionCommandsInactive');

        var commandShortcut = node.querySelector('.command-shortcut');
        commandShortcut.classList.add('inactive-keybinding');
      } else {
        shortcutNode.textContent = command.keybinding;
      }

      var commandClear = node.querySelector('.command-clear');
      commandClear.id = this.createElementId_(
          'clear', extensionId, command.name);
      commandClear.title = loadTimeData.getString('extensionCommandsDelete');
      commandClear.addEventListener('click', this.handleClear_.bind(this));

      var select = node.querySelector('.command-scope');
      select.id = this.createElementId_(
          'setCommandScope', extensionId, command.name);
      select.hidden = false;
      // Add the 'In Chrome' option.
      var option = document.createElement('option');
      option.textContent = loadTimeData.getString('extensionCommandsRegular');
      select.appendChild(option);
      if (command.isExtensionAction || !command.isActive) {
        // Extension actions cannot be global, so we might as well disable the
        // combo box, to signify that, and if the command is inactive, it
        // doesn't make sense to allow the user to adjust the scope.
        select.disabled = true;
      } else {
        // Add the 'Global' option.
        option = document.createElement('option');
        option.textContent = loadTimeData.getString('extensionCommandsGlobal');
        select.appendChild(option);
        select.selectedIndex =
            command.scope == chrome.developerPrivate.CommandScope.GLOBAL ?
                1 : 0;

        select.addEventListener(
            'change', this.handleSetCommandScope_.bind(this));
      }

      this.appendChild(node);
    },

    /**
     * Starts keystroke capture to determine which key to use for a particular
     * extension command.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    startCapture_: function(event) {
      if (this.capturingElement_)
        return;  // Already capturing.

      chrome.developerPrivate.setShortcutHandlingSuspended(true);

      var shortcutNode = event.target;
      this.oldValue_ = shortcutNode.textContent;
      shortcutNode.textContent =
          loadTimeData.getString('extensionCommandsStartTyping');
      shortcutNode.parentElement.classList.add('capturing');

      var commandClear =
          shortcutNode.parentElement.querySelector('.command-clear');
      commandClear.hidden = true;

      this.capturingElement_ = /** @type {HTMLElement} */(event.target);
    },

    /**
     * Ends keystroke capture and either restores the old value or (if valid
     * value) sets the new value as active..
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    endCapture_: function(event) {
      if (!this.capturingElement_)
        return;  // Not capturing.

      chrome.developerPrivate.setShortcutHandlingSuspended(false);

      var shortcutNode = this.capturingElement_;
      var commandShortcut = shortcutNode.parentElement;

      commandShortcut.classList.remove('capturing');
      commandShortcut.classList.remove('contains-chars');

      // When the capture ends, the user may have not given a complete and valid
      // input (or even no input at all). Only a valid key event followed by a
      // valid key combination will cause a shortcut selection to be activated.
      // If no valid selection was made, however, revert back to what the
      // textbox had before to indicate that the shortcut registration was
      // canceled.
      if (!this.currentKeyEvent_ ||
          !extensions.isValidKeyCode(this.currentKeyEvent_.keyCode))
        shortcutNode.textContent = this.oldValue_;

      var commandClear = commandShortcut.querySelector('.command-clear');
      if (this.oldValue_ == '') {
        commandShortcut.classList.remove('clearable');
        commandClear.hidden = true;
      } else {
        commandShortcut.classList.add('clearable');
        commandClear.hidden = false;
      }

      this.oldValue_ = '';
      this.capturingElement_ = null;
      this.currentKeyEvent_ = null;
    },

    /**
     * Handles focus event and adds visual indication for active shortcut.
     * @param {Event} event to consider.
     * @private
     */
    handleFocus_: function(event) {
      var commandShortcut = event.target.parentElement;
      commandShortcut.classList.add('focused');
    },

    /**
     * Handles lost focus event and removes visual indication of active shortcut
     * also stops capturing on focus lost.
     * @param {Event} event to consider.
     * @private
     */
    handleBlur_: function(event) {
      this.endCapture_(event);
      var commandShortcut = event.target.parentElement;
      commandShortcut.classList.remove('focused');
    },

    /**
     * The KeyDown handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyDown_: function(event) {
      event = /** @type {KeyboardEvent} */(event);
      if (event.keyCode == extensions.Key.Escape) {
        if (!this.capturingElement_) {
          // If we're not currently capturing, allow escape to propagate (so it
          // can close the overflow).
          return;
        }
        // Otherwise, escape cancels capturing.
        this.endCapture_(event);
        var parsed = this.parseElementId_('clear',
            event.target.parentElement.querySelector('.command-clear').id);
        chrome.developerPrivate.updateExtensionCommand({
          extensionId: parsed.extensionId,
          commandName: parsed.commandName,
          keybinding: ''
        });
        event.preventDefault();
        event.stopPropagation();
        return;
      }
      if (event.keyCode == extensions.Key.Tab) {
        // Allow tab propagation for keyboard navigation.
        return;
      }

      if (!this.capturingElement_)
        this.startCapture_(event);

      this.handleKey_(event);
    },

    /**
     * The KeyUp handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyUp_: function(event) {
      event = /** @type {KeyboardEvent} */(event);
      if (event.keyCode == extensions.Key.Tab ||
          event.keyCode == extensions.Key.Escape) {
        // We need to allow tab propagation for keyboard navigation, and escapes
        // are fully handled in handleKeyDown.
        return;
      }

      // We want to make it easy to change from Ctrl+Shift+ to just Ctrl+ by
      // releasing Shift, but we also don't want it to be easy to lose for
      // example Ctrl+Shift+F to Ctrl+ just because you didn't release Ctrl
      // as fast as the other two keys. Therefore, we process KeyUp until you
      // have a valid combination and then stop processing it (meaning that once
      // you have a valid combination, we won't change it until the next
      // KeyDown message arrives).
      if (!this.currentKeyEvent_ ||
          !extensions.isValidKeyCode(this.currentKeyEvent_.keyCode)) {
        if (!event.ctrlKey && !event.altKey ||
            ((cr.isMac || cr.isChromeOS) && !event.metaKey)) {
          // If neither Ctrl nor Alt is pressed then it is not a valid shortcut.
          // That means we're back at the starting point so we should restart
          // capture.
          this.endCapture_(event);
          this.startCapture_(event);
        } else {
          this.handleKey_(event);
        }
      }
    },

    /**
     * A general key handler (used for both KeyDown and KeyUp).
     * @param {KeyboardEvent} event The keyboard event to consider.
     * @private
     */
    handleKey_: function(event) {
      // While capturing, we prevent all events from bubbling, to prevent
      // shortcuts lacking the right modifier (F3 for example) from activating
      // and ending capture prematurely.
      event.preventDefault();
      event.stopPropagation();

      if (!extensions.hasValidModifiers(event))
        return;

      var shortcutNode = this.capturingElement_;
      var keystroke = extensions.keystrokeToString(event);
      shortcutNode.textContent = keystroke;
      event.target.classList.add('contains-chars');
      this.currentKeyEvent_ = event;

      if (extensions.isValidKeyCode(event.keyCode)) {
        var node = event.target;
        while (node && !node.id)
          node = node.parentElement;

        this.oldValue_ = keystroke;  // Forget what the old value was.
        var parsed = this.parseElementId_('command', node.id);

        // Ending the capture must occur before calling
        // setExtensionCommandShortcut to ensure the shortcut is set.
        this.endCapture_(event);
        chrome.developerPrivate.updateExtensionCommand(
            {extensionId: parsed.extensionId,
             commandName: parsed.commandName,
             keybinding: keystroke});
      }
    },

    /**
     * A handler for the delete command button.
     * @param {Event} event The mouse event to consider.
     * @private
     */
    handleClear_: function(event) {
      var parsed = this.parseElementId_('clear', event.target.id);
      chrome.developerPrivate.updateExtensionCommand(
          {extensionId: parsed.extensionId,
           commandName: parsed.commandName,
           keybinding: ''});
    },

    /**
     * A handler for the setting the scope of the command.
     * @param {Event} event The mouse event to consider.
     * @private
     */
    handleSetCommandScope_: function(event) {
      var parsed = this.parseElementId_('setCommandScope', event.target.id);
      var element = document.getElementById(
          'setCommandScope-' + parsed.extensionId + '-' + parsed.commandName);
      var scope = element.selectedIndex == 1 ?
          chrome.developerPrivate.CommandScope.GLOBAL :
          chrome.developerPrivate.CommandScope.CHROME;
      chrome.developerPrivate.updateExtensionCommand(
          {extensionId: parsed.extensionId,
           commandName: parsed.commandName,
           scope: scope});
    },

    /**
     * A utility function to create a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} extensionId The extension ID to use in the id.
     * @param {string} commandName The command name to append the id with.
     * @private
     */
    createElementId_: function(namespace, extensionId, commandName) {
      return namespace + '-' + extensionId + '-' + commandName;
    },

    /**
     * A utility function to parse a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} id          The id to parse.
     * @return {{extensionId: string, commandName: string}} The parsed id.
     * @private
     */
    parseElementId_: function(namespace, id) {
      var kExtensionIdLength = 32;
      return {
        extensionId: id.substring(namespace.length + 1,
                                  namespace.length + 1 + kExtensionIdLength),
        commandName: id.substring(namespace.length + 1 + kExtensionIdLength + 1)
      };
    },
  };

  return {
    ExtensionCommandList: ExtensionCommandList
  };
});
