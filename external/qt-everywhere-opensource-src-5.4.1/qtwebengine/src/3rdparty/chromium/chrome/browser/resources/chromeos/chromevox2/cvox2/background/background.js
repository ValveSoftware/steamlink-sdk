// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('cvox2.Background');
goog.provide('cvox2.global');

/** Classic Chrome accessibility API. */
cvox2.global.accessibility =
    chrome.accessibilityPrivate || chrome.experimental.accessibility;

/**
 * ChromeVox2 background page.
 */
cvox2.Background = function() {
  // Only needed with unmerged ChromeVox classic loaded before.
  cvox2.global.accessibility.setAccessibilityEnabled(false);

  // Register listeners for ...
  // Desktop.
  chrome.automation.getDesktop(this.onGotTree.bind(this));

  // Tabs.
  chrome.tabs.onUpdated.addListener(this.onTabUpdated.bind(this));

  // Keyboard events (currently Messages from content script).
  chrome.extension.onConnect.addListener(this.onConnect.bind(this));
};

cvox2.Background.prototype = {
  /**
   * ID of the port used to communicate between content script and background
   * page.
   * @const {string}
   */
  PORT_ID: 'chromevox2',

  /**
   * Handles chrome.extension.onConnect.
   * @param {Object} port The port.
   */
  onConnect: function(port) {
    if (port.name != this.PORT_ID)
      return;
    port.onMessage.addListener(this.onMessage.bind(this));
  },

  /**
   * Dispatches messages to specific handlers.
   * @param {Object} message The message.
   */
  onMessage: function(message) {
    if (message.keyDown)
      this.onKeyDown(message);
  },

  /**
   * Handles key down messages from the content script.
   * @param {Object} message The key down message.
   */
  onKeyDown: function(message) {
    // TODO(dtseng): Implement.
  },

  /**
   * Handles chrome.tabs.onUpdate.
   * @param {number} tabId The tab id.
   * @param {Object.<string, (string|boolean)>} changeInfo Information about
   * the updated tab.
   */
  onTabUpdated: function(tabId, changeInfo) {
    chrome.automation.getTree(this.onGotTree.bind(this));
  },

  /**
   * Handles all setup once a new automation tree appears.
   * @param {AutomationTree} tree The new automation tree.
   */
  onGotTree: function(root) {
    // Register all automation event listeners.
    root.addEventListener(chrome.automation.EventType.focus,
                          this.onAutomationEvent.bind(this),
                          true);
  },

  /**
   * A generic handler for all desktop automation events.
   * @param {AutomationEvent} evt The event.
   */
  onAutomationEvent: function(evt) {
    var output = evt.target.attributes.name + ' ' + evt.target.role;
    cvox.ChromeVox.tts.speak(output);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(output));
  }
};

/** @type {cvox2.Background} */
cvox2.global.backgroundObj = new cvox2.Background();
