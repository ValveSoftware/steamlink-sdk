// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  // The UI to display and manage keyboard shortcuts set for extension commands.
  var ShortcutInput = Polymer({
    is: 'extensions-shortcut-input',

    behaviors: [I18nBehavior],

    properties: {
      item: {
        type: String,
        value: '',
      },
      commandName: {
        type: String,
        value: '',
      },
      shortcut: {
        type: String,
        value: '',
      },
      /** @private */
      capturing_: {
        type: Boolean,
        value: false,
      },
      /** @private */
      pendingShortcut_: {
        type: String,
        value: '',
      },
    },

    ready: function() {
      var node = this.$['input'];
      node.addEventListener('mouseup', this.startCapture_.bind(this));
      node.addEventListener('blur', this.endCapture_.bind(this));
      node.addEventListener('focus', this.startCapture_.bind(this));
      node.addEventListener('keydown', this.onKeyDown_.bind(this));
      node.addEventListener('keyup', this.onKeyUp_.bind(this));
    },

    /** @private */
    startCapture_: function() {
      if (this.capturing_)
        return;
      this.capturing_ = true;
      this.fire('shortcut-capture-started');
    },

    /** @private */
    endCapture_: function() {
      if (!this.capturing_)
        return;
      this.pendingShortcut_ = '';
      this.capturing_ = false;
      this.$['input'].blur();
      this.fire('shortcut-capture-ended');
    },

    /**
     * @param {!KeyboardEvent} e
     * @private
     */
    onKeyDown_: function(e) {
      if (e.keyCode == extensions.Key.Escape) {
        if (!this.capturing_) {
          // If we're not currently capturing, allow escape to propagate.
          return;
        }
        // Otherwise, escape cancels capturing.
        this.endCapture_();
        e.preventDefault();
        e.stopPropagation();
        return;
      }
      if (e.keyCode == extensions.Key.Tab) {
        // Allow tab propagation for keyboard navigation.
        return;
      }

      if (!this.capturing_)
        this.startCapture_();

      this.handleKey_(e);
    },

    /**
     * @param {!KeyboardEvent} e
     * @private
     */
    onKeyUp_: function(e) {
      if (e.keyCode == extensions.Key.Escape || e.keyCode == extensions.Key.Tab)
        return;

      this.handleKey_(e);
    },

    /**
     * @param {!KeyboardEvent} e
     * @private
     */
    handleKey_: function(e) {
      // While capturing, we prevent all events from bubbling, to prevent
      // shortcuts lacking the right modifier (F3 for example) from activating
      // and ending capture prematurely.
      e.preventDefault();
      e.stopPropagation();

      // We don't allow both Ctrl and Alt in the same keybinding.
      // TODO(devlin): This really should go in extensions.hasValidModifiers,
      // but that requires updating the existing page as well.
      if ((e.ctrlKey && e.altKey) || !extensions.hasValidModifiers(e)) {
        this.pendingShortcut_ = 'invalid';
        return;
      }

      this.pendingShortcut_ = extensions.keystrokeToString(e);

      if (extensions.isValidKeyCode(e.keyCode)) {
        this.commitPending_();
        this.endCapture_();
      }
    },

    /** @private */
    commitPending_: function() {
      this.shortcut = this.pendingShortcut_;
      this.fire('shortcut-updated', {keybinding: this.shortcut,
                                     item: this.item,
                                     commandName: this.commandName});
    },

    /**
     * @return {string} The text to be displayed in the shortcut field.
     * @private
     */
    computeText_: function() {
      if (this.capturing_)
        return this.pendingShortcut_ || this.i18n('shortcutTypeAShortcut');
      return this.shortcut || this.i18n('shortcutNotSet');
    },

    /**
     * @return {boolean} Whether the clear button is hidden.
     * @private
     */
    computeClearHidden_: function() {
      // We don't want to show the clear button if the input is currently
      // capturing a new shortcut or if there is no shortcut to clear.
      return this.capturing_ || !this.shortcut;
    },

    /** @private */
    onClearTap_: function() {
      if (this.shortcut) {
        this.pendingShortcut_ = '';
        this.commitPending_();
      }
    },
  });

  return {ShortcutInput: ShortcutInput};
});
