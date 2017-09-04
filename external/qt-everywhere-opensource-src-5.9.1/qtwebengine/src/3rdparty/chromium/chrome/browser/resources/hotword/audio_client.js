// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This is the audio client content script injected into eligible
 *  Google.com and New tab pages for interaction between the Webpage and the
 *  Hotword extension.
 */

(function() {
  /**
   * @constructor
   */
  var AudioClient = function() {
    /** @private {Element} */
    this.speechOverlay_ = null;

    /** @private {number} */
    this.checkSpeechUiRetries_ = 0;

    /**
     * Port used to communicate with the audio manager.
     * @private {?Port}
     */
    this.port_ = null;

    /**
     * Keeps track of the effects of different commands. Used to verify that
     * proper UIs are shown to the user.
     * @private {Object<AudioClient.CommandToPage, Object>}
     */
    this.uiStatus_ = null;

    /**
     * Bound function used to handle commands sent from the page to this script.
     * @private {Function}
     */
    this.handleCommandFromPageFunc_ = null;
  };

  /**
   * Messages sent to the page to control the voice search UI.
   * @enum {string}
   */
  AudioClient.CommandToPage = {
    HOTWORD_VOICE_TRIGGER: 'vt',
    HOTWORD_STARTED: 'hs',
    HOTWORD_ENDED: 'hd',
    HOTWORD_TIMEOUT: 'ht',
    HOTWORD_ERROR: 'he'
  };

  /**
   * Messages received from the page used to indicate voice search state.
   * @enum {string}
   */
  AudioClient.CommandFromPage = {
    SPEECH_START: 'ss',
    SPEECH_END: 'se',
    SPEECH_RESET: 'sr',
    SHOWING_HOTWORD_START: 'shs',
    SHOWING_ERROR_MESSAGE: 'sem',
    SHOWING_TIMEOUT_MESSAGE: 'stm',
    CLICKED_RESUME: 'hcc',
    CLICKED_RESTART: 'hcr',
    CLICKED_DEBUG: 'hcd'
  };

  /**
   * Errors that are sent to the hotword extension.
   * @enum {string}
   */
  AudioClient.Error = {
    NO_SPEECH_UI: 'ac1',
    NO_HOTWORD_STARTED_UI: 'ac2',
    NO_HOTWORD_TIMEOUT_UI: 'ac3',
    NO_HOTWORD_ERROR_UI: 'ac4'
  };

  /**
   * @const {string}
   * @private
   */
  AudioClient.HOTWORD_EXTENSION_ID_ = 'nbpagnldghgfoolbancepceaanlmhfmd';

  /**
   * Number of times to retry checking a transient error.
   * @const {number}
   * @private
   */
  AudioClient.MAX_RETRIES = 3;

  /**
   * Delay to wait in milliseconds before rechecking for any transient errors.
   * @const {number}
   * @private
   */
  AudioClient.RETRY_TIME_MS_ = 2000;

  /**
   * DOM ID for the speech UI overlay.
   * @const {string}
   * @private
   */
  AudioClient.SPEECH_UI_OVERLAY_ID_ = 'spch';

  /**
   * @const {string}
   * @private
   */
  AudioClient.HELP_CENTER_URL_ =
      'https://support.google.com/chrome/?p=ui_hotword_search';

  /**
   * @const {string}
   * @private
   */
  AudioClient.CLIENT_PORT_NAME_ = 'chwcpn';

  /**
   * Existence of the Audio Client.
   * @const {string}
   * @private
   */
  AudioClient.EXISTS_ = 'chwace';

  /**
   * Checks for the presence of speech overlay UI DOM elements.
   * @private
   */
  AudioClient.prototype.checkSpeechOverlayUi_ = function() {
    if (!this.speechOverlay_) {
      window.setTimeout(this.delayedCheckSpeechOverlayUi_.bind(this),
                        AudioClient.RETRY_TIME_MS_);
    } else {
      this.checkSpeechUiRetries_ = 0;
    }
  };

  /**
   * Function called to check for the speech UI overlay after some time has
   * passed since an initial check. Will either retry triggering the speech
   * or sends an error message depending on the number of retries.
   * @private
   */
  AudioClient.prototype.delayedCheckSpeechOverlayUi_ = function() {
    this.speechOverlay_ = document.getElementById(
        AudioClient.SPEECH_UI_OVERLAY_ID_);
    if (!this.speechOverlay_) {
      if (this.checkSpeechUiRetries_++ < AudioClient.MAX_RETRIES) {
        this.sendCommandToPage_(AudioClient.CommandToPage.VOICE_TRIGGER);
        this.checkSpeechOverlayUi_();
      } else {
        this.sendCommandToExtension_(AudioClient.Error.NO_SPEECH_UI);
      }
    } else {
      this.checkSpeechUiRetries_ = 0;
    }
  };

  /**
   * Checks that the triggered UI is actually displayed.
   * @param {AudioClient.CommandToPage} command Command that was send.
   * @private
   */
  AudioClient.prototype.checkUi_ = function(command) {
    this.uiStatus_[command].timeoutId =
        window.setTimeout(this.failedCheckUi_.bind(this, command),
                          AudioClient.RETRY_TIME_MS_);
  };

  /**
   * Function called when the UI verification is not called in time. Will either
   * retry the command or sends an error message, depending on the number of
   * retries for the command.
   * @param {AudioClient.CommandToPage} command Command that was sent.
   * @private
   */
  AudioClient.prototype.failedCheckUi_ = function(command) {
    if (this.uiStatus_[command].tries++ < AudioClient.MAX_RETRIES) {
      this.sendCommandToPage_(command);
      this.checkUi_(command);
    } else {
      this.sendCommandToExtension_(this.uiStatus_[command].error);
    }
  };

  /**
   * Confirm that an UI element has been shown.
   * @param {AudioClient.CommandToPage} command UI to confirm.
   * @private
   */
  AudioClient.prototype.verifyUi_ = function(command) {
    if (this.uiStatus_[command].timeoutId) {
      window.clearTimeout(this.uiStatus_[command].timeoutId);
      this.uiStatus_[command].timeoutId = null;
      this.uiStatus_[command].tries = 0;
    }
  };

  /**
   * Sends a command to the audio manager.
   * @param {string} commandStr command to send to plugin.
   * @private
   */
  AudioClient.prototype.sendCommandToExtension_ = function(commandStr) {
    if (this.port_)
      this.port_.postMessage({'cmd': commandStr});
  };

  /**
   * Handles a message from the audio manager.
   * @param {{cmd: string}} commandObj Command from the audio manager.
   * @private
   */
  AudioClient.prototype.handleCommandFromExtension_ = function(commandObj) {
    var command = commandObj['cmd'];
    if (command) {
      switch (command) {
        case AudioClient.CommandToPage.HOTWORD_VOICE_TRIGGER:
          this.sendCommandToPage_(command);
          this.checkSpeechOverlayUi_();
          break;
        case AudioClient.CommandToPage.HOTWORD_STARTED:
          this.sendCommandToPage_(command);
          this.checkUi_(command);
          break;
        case AudioClient.CommandToPage.HOTWORD_ENDED:
          this.sendCommandToPage_(command);
          break;
        case AudioClient.CommandToPage.HOTWORD_TIMEOUT:
          this.sendCommandToPage_(command);
          this.checkUi_(command);
          break;
        case AudioClient.CommandToPage.HOTWORD_ERROR:
          this.sendCommandToPage_(command);
          this.checkUi_(command);
          break;
      }
    }
  };

  /**
   * @param {AudioClient.CommandToPage} commandStr Command to send.
   * @private
   */
  AudioClient.prototype.sendCommandToPage_ = function(commandStr) {
    window.postMessage({'type': commandStr}, '*');
  };

  /**
   * Handles a message from the html window.
   * @param {!MessageEvent} messageEvent Message event from the window.
   * @private
   */
  AudioClient.prototype.handleCommandFromPage_ = function(messageEvent) {
    if (messageEvent.source == window && messageEvent.data.type) {
      var command = messageEvent.data.type;
      switch (command) {
        case AudioClient.CommandFromPage.SPEECH_START:
          this.speechActive_ = true;
          this.sendCommandToExtension_(command);
          break;
        case AudioClient.CommandFromPage.SPEECH_END:
          this.speechActive_ = false;
          this.sendCommandToExtension_(command);
          break;
        case AudioClient.CommandFromPage.SPEECH_RESET:
          this.speechActive_ = false;
          this.sendCommandToExtension_(command);
          break;
        case 'SPEECH_RESET':  // Legacy, for embedded NTP.
          this.speechActive_ = false;
          this.sendCommandToExtension_(AudioClient.CommandFromPage.SPEECH_END);
          break;
        case AudioClient.CommandFromPage.CLICKED_RESUME:
          this.sendCommandToExtension_(command);
          break;
        case AudioClient.CommandFromPage.CLICKED_RESTART:
          this.sendCommandToExtension_(command);
          break;
        case AudioClient.CommandFromPage.CLICKED_DEBUG:
          window.open(AudioClient.HELP_CENTER_URL_, '_blank');
          break;
        case AudioClient.CommandFromPage.SHOWING_HOTWORD_START:
          this.verifyUi_(AudioClient.CommandToPage.HOTWORD_STARTED);
          break;
        case AudioClient.CommandFromPage.SHOWING_ERROR_MESSAGE:
          this.verifyUi_(AudioClient.CommandToPage.HOTWORD_ERROR);
          break;
        case AudioClient.CommandFromPage.SHOWING_TIMEOUT_MESSAGE:
          this.verifyUi_(AudioClient.CommandToPage.HOTWORD_TIMEOUT);
          break;
      }
    }
  };

  /**
   * Initialize the content script.
   */
  AudioClient.prototype.initialize = function() {
    if (AudioClient.EXISTS_ in window)
      return;
    window[AudioClient.EXISTS_] = true;

    // UI verification object.
    this.uiStatus_ = {};
    this.uiStatus_[AudioClient.CommandToPage.HOTWORD_STARTED] = {
      timeoutId: null,
      tries: 0,
      error: AudioClient.Error.NO_HOTWORD_STARTED_UI
    };
    this.uiStatus_[AudioClient.CommandToPage.HOTWORD_TIMEOUT] = {
      timeoutId: null,
      tries: 0,
      error: AudioClient.Error.NO_HOTWORD_TIMEOUT_UI
    };
    this.uiStatus_[AudioClient.CommandToPage.HOTWORD_ERROR] = {
      timeoutId: null,
      tries: 0,
      error: AudioClient.Error.NO_HOTWORD_ERROR_UI
    };

    this.handleCommandFromPageFunc_ = this.handleCommandFromPage_.bind(this);
    window.addEventListener('message', this.handleCommandFromPageFunc_, false);
    this.initPort_();
  };

  /**
   * Initialize the communications port with the audio manager. This
   * function will be also be called again if the audio-manager
   * disconnects for some reason (such as the extension
   * background.html page being reloaded).
   * @private
   */
  AudioClient.prototype.initPort_ = function() {
    this.port_ = chrome.runtime.connect(
        AudioClient.HOTWORD_EXTENSION_ID_,
        {'name': AudioClient.CLIENT_PORT_NAME_});
    // Note that this listen may have to be destroyed manually if AudioClient
    // is ever destroyed on this tab.
    this.port_.onDisconnect.addListener(
        (function(e) {
          if (this.handleCommandFromPageFunc_) {
            window.removeEventListener(
                'message', this.handleCommandFromPageFunc_, false);
          }
          delete window[AudioClient.EXISTS_];
        }).bind(this));

    // See note above.
    this.port_.onMessage.addListener(
        this.handleCommandFromExtension_.bind(this));

    if (this.speechActive_) {
      this.sendCommandToExtension_(AudioClient.CommandFromPage.SPEECH_START);
    } else {
      // It's possible for this script to be injected into the page after it has
      // completed loaded (i.e. when prerendering). In this case, this script
      // won't receive a SPEECH_RESET from the page to forward onto the
      // extension. To make up for this, always send a SPEECH_RESET. This means
      // in most cases, the extension will receive SPEECH_RESET twice, one from
      // this sendCommandToExtension_ and the one forwarded from the page. But
      // that's OK and the extension can handle it.
      this.sendCommandToExtension_(AudioClient.CommandFromPage.SPEECH_RESET);
    }
  };

  // Initializes as soon as the code is ready, do not wait for the page.
  new AudioClient().initialize();
})();
