// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Base class for managing hotwording sessions.
   * @param {!hotword.StateManager} stateManager Manager of global hotwording
   *     state.
   * @param {!hotword.constants.SessionSource} sessionSource Source of the
   *     hotword session request.
   * @constructor
   */
  function BaseSessionManager(stateManager, sessionSource) {
    /**
     * Manager of global hotwording state.
     * @protected {!hotword.StateManager}
     */
    this.stateManager = stateManager;

    /**
     * Source of the hotword session request.
     * @private {!hotword.constants.SessionSource}
     */
    this.sessionSource_ = sessionSource;

    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     * @private
     */
    this.sessionRequestedListener_ = this.handleSessionRequested_.bind(this);
    this.sessionStoppedListener_ = this.handleSessionStopped_.bind(this);

    // Need to setup listeners on startup, otherwise events that caused the
    // event page to start up, will be lost.
    this.setupListeners_();

    this.stateManager.onStatusChanged.addListener(function() {
      hotword.debug('onStatusChanged');
      this.updateListeners();
    }.bind(this));
  }

  BaseSessionManager.prototype = {
    /**
     * Return whether or not this session type is enabled.
     * @protected
     * @return {boolean}
     */
    enabled: assertNotReached,

    /**
     * Called when the hotwording session is stopped.
     * @protected
     */
    onSessionStop: function() {
    },

    /**
     * Starts a launcher hotwording session.
     * @param {hotword.constants.TrainingMode=} opt_mode The mode to start the
     *     recognizer in.
     */
    startSession: function(opt_mode) {
      this.stateManager.startSession(
          this.sessionSource_,
          function() {
            chrome.hotwordPrivate.setHotwordSessionState(true, function() {});
          },
          this.handleHotwordTrigger.bind(this),
          opt_mode);
    },

    /**
     * Stops a launcher hotwording session.
     * @private
     */
    stopSession_: function() {
      this.stateManager.stopSession(this.sessionSource_);
      this.onSessionStop();
    },

    /**
     * Handles a hotword triggered event.
     * @param {?Object} log Audio log data, if audio logging is enabled.
     * @protected
     */
    handleHotwordTrigger: function(log) {
      hotword.debug('Hotword triggered: ' + this.sessionSource_, log);
      chrome.hotwordPrivate.notifyHotwordRecognition('search',
                                                     log,
                                                     function() {});
    },

    /**
     * Handles a hotwordPrivate.onHotwordSessionRequested event.
     * @private
     */
    handleSessionRequested_: function() {
      hotword.debug('handleSessionRequested_: ' + this.sessionSource_);
      this.startSession();
    },

    /**
     * Handles a hotwordPrivate.onHotwordSessionStopped event.
     * @private
     */
    handleSessionStopped_: function() {
      hotword.debug('handleSessionStopped_: ' + this.sessionSource_);
      this.stopSession_();
    },

    /**
     * Set up event listeners.
     * @private
     */
    setupListeners_: function() {
      if (chrome.hotwordPrivate.onHotwordSessionRequested.hasListener(
              this.sessionRequestedListener_)) {
        return;
      }

      chrome.hotwordPrivate.onHotwordSessionRequested.addListener(
          this.sessionRequestedListener_);
      chrome.hotwordPrivate.onHotwordSessionStopped.addListener(
          this.sessionStoppedListener_);
    },

    /**
     * Remove event listeners.
     * @private
     */
    removeListeners_: function() {
      chrome.hotwordPrivate.onHotwordSessionRequested.removeListener(
          this.sessionRequestedListener_);
      chrome.hotwordPrivate.onHotwordSessionStopped.removeListener(
          this.sessionStoppedListener_);
    },

    /**
     * Update event listeners based on the current hotwording state.
     * @protected
     */
    updateListeners: function() {
      if (this.enabled()) {
        this.setupListeners_();
      } else {
        this.removeListeners_();
        this.stopSession_();
      }
    }
  };

  return {
    BaseSessionManager: BaseSessionManager
  };
});
