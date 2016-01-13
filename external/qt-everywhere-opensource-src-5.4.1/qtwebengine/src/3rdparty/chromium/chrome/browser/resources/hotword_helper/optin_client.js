// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This is the content script injected by the opt in helper.
 *   It is injected into the newtab page and also google.com and notifies the
 *   page to show an opt-in message for the user to enable the built-in hotword
 *   extension.
 */



/** @constructor */
var OptInClient = function() {};


/**
 * Commands sent from this injected content scripts to the page.
 * @enum {string}
 */
OptInClient.CommandToPage = {
  // Allow hotword opt-in message bubble to be displayed.
  ALLOW_OPTIN_MESSAGE: 'chwom'
};


/**
 * Commands sent from the page to this content script.
 * @enum {string}
 */
OptInClient.CommandFromPage = {
  // User has explicitly clicked 'no'.
  CLICKED_NO_OPTIN: 'hcno',
  // User has opted in.
  CLICKED_OPTIN: 'hco',
  // Audio logging is opted in.
  AUDIO_LOGGING_ON: 'alon',
  // Audio logging is opted out.
  AUDIO_LOGGING_OFF: 'aloff',
};


/**
 * Used to determine if this content script has already been injected.
 * @const {string}
 * @private
 */
OptInClient.EXISTS_ = 'chwoihe';


/**
 * Handles the messages posted to the window, mainly listening for
 * the optin and no optin clicks. Also listening for preference on
 * audio logging.
 * @param {!MessageEvent} messageEvent Message event from the window.
 * @private
 */
OptInClient.prototype.handleCommandFromPage_ = function(messageEvent) {
  if (messageEvent.source === window && messageEvent.data.type) {
    var command = messageEvent.data.type;
    chrome.runtime.sendMessage({'type': command});
  }
};


/**
 * Sends a command to the page allowing it to display the hotword opt-in
 * message. It is up to the page whether it is actually displayed.
 * @private
 */
OptInClient.prototype.notifyPageAllowOptIn_ = function() {
  if (document.readyState === 'complete') {
    window.postMessage(
        {'type': OptInClient.CommandToPage.ALLOW_OPTIN_MESSAGE}, '*');
  }
};


/**
 * Initializes the content script.
 */
OptInClient.prototype.initialize = function() {
  if (OptInClient.EXISTS_ in window)
    return;
  window[OptInClient.EXISTS_] = true;
  window.addEventListener(
      'message', this.handleCommandFromPage_.bind(this), false);

  if (document.readyState === 'complete')
    this.notifyPageAllowOptIn_();
  else
    document.onreadystatechange = this.notifyPageAllowOptIn_;
};


new OptInClient().initialize();
