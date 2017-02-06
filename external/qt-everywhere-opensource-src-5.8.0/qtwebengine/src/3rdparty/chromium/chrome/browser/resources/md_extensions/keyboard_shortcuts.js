// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  // The UI to display and manage keyboard shortcuts set for extension commands.
  var KeyboardShortcuts = Polymer({
    is: 'extensions-keyboard-shortcuts',

    behaviors: [Polymer.NeonAnimatableBehavior],

    properties: {
      /** @type {Array<!chrome.developerPrivate.ExtensionInfo>} */
      items: Array,
    },

    ready: function() {
      /** @type {!extensions.AnimationHelper} */
      this.animationHelper = new extensions.AnimationHelper(this, this.$.main);
      this.animationHelper.setEntryAnimation(extensions.Animation.FADE_IN);
      this.animationHelper.setExitAnimation(extensions.Animation.SCALE_DOWN);
      this.sharedElements = {hero: this.$.main};
    },

    /**
     * @return {!Array<!chrome.developerPrivate.ExtensionInfo>}
     * @private
     */
    calculateShownItems_: function() {
      return this.items.filter(function(item) {
        return item.commands.length > 0;
      });
    },

    /**
     * A polymer bug doesn't allow for databinding of a string property as a
     * boolean, but it is correctly interpreted from a function.
     * Bug: https://github.com/Polymer/polymer/issues/3669
     * @param {string} keybinding
     * @return {boolean}
     * @private
     */
    hasKeybinding_: function(keybinding) {
      return !!keybinding;
    },

    /**
     * Returns the scope index in the dropdown menu for the command's scope.
     * @param {chrome.developerPrivate.Command} command
     * @return {number}
     * @private
     */
    computeSelectedScope_: function(command) {
      // These numbers match the indexes in the dropdown menu in the html.
      switch (command.scope) {
        case chrome.developerPrivate.CommandScope.CHROME:
          return 0;
        case chrome.developerPrivate.CommandScope.GLOBAL:
          return 1;
      }
      assertNotReached();
    },

    /** @private */
    onCloseButtonClick_: function() {
      this.fire('close');
    },
  });

  return {KeyboardShortcuts: KeyboardShortcuts};
});
