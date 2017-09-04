// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Trivial container class for session information.
   * @param {!hotword.constants.SessionSource} source Source of the hotword
   *     session.
   * @param {!function()} triggerCb Callback invoked when the hotword has
   *     triggered.
   * @param {!function()} startedCb Callback invoked when the session has
   *     been started successfully.
   * @param {function()=} opt_modelSavedCb Callback invoked when the speaker
   *     model has been saved successfully.
   * @constructor
   * @struct
   * @private
   */
  function Session_(source, triggerCb, startedCb, opt_modelSavedCb) {
    /**
     * Source of the hotword session request.
     * @private {!hotword.constants.SessionSource}
     */
    this.source_ = source;

     /**
      * Callback invoked when the hotword has triggered.
      * @private {!function()}
      */
    this.triggerCb_ = triggerCb;

    /**
     * Callback invoked when the session has been started successfully.
     * @private {?function()}
     */
    this.startedCb_ = startedCb;

    /**
     * Callback invoked when the session has been started successfully.
     * @private {?function()}
     */
    this.speakerModelSavedCb_ = opt_modelSavedCb;
  }

  /**
   * Class to manage hotwording state. Starts/stops the hotword detector based
   * on user settings, session requests, and any other factors that play into
   * whether or not hotwording should be running.
   * @constructor
   */
  function StateManager() {
    /**
     * Current state.
     * @private {hotword.StateManager.State_}
     */
    this.state_ = State_.STOPPED;

    /**
     * Current hotwording status.
     * @private {?chrome.hotwordPrivate.StatusDetails}
     */
    this.hotwordStatus_ = null;

    /**
     * NaCl plugin manager.
     * @private {?hotword.NaClManager}
     */
    this.pluginManager_ = null;

    /**
     * Currently active hotwording sessions.
     * @private {!Array<Session_>}
     */
    this.sessions_ = [];

    /**
     * The mode to start the recognizer in.
     * @private {!hotword.constants.RecognizerStartMode}
     */
    this.startMode_ = hotword.constants.RecognizerStartMode.NORMAL;

    /**
     * Event that fires when the hotwording status has changed.
     * @type {!ChromeEvent}
     */
    this.onStatusChanged = new chrome.Event();

    /**
     * Hotword trigger audio notification... a.k.a The Chime (tm).
     * @private {!HTMLAudioElement}
     */
    this.chime_ =
        /** @type {!HTMLAudioElement} */(document.createElement('audio'));

    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     * @private
     */
    this.idleStateChangedListener_ = this.handleIdleStateChanged_.bind(this);
    this.startupListener_ = this.handleStartup_.bind(this);

    /**
     * Whether this user is locked.
     * @private {boolean}
     */
    this.isLocked_ = false;

    /**
     * Current state of audio logging.
     * This is tracked separately from hotwordStatus_ because we need to restart
     * the hotword detector when this value changes.
     * @private {boolean}
     */
    this.loggingEnabled_ = false;

    /**
     * Current state of training.
     * This is tracked separately from |hotwordStatus_| because we need to
     * restart the hotword detector when this value changes.
     * @private {!boolean}
     */
    this.trainingEnabled_ = false;

    /**
     * Helper class to keep this extension alive while the hotword detector is
     * running in always-on mode.
     * @private {!hotword.KeepAlive}
     */
    this.keepAlive_ = new hotword.KeepAlive();

    // Get the initial status.
    chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));

    // Setup the chime and insert into the page.
    // Set preload=none to prevent an audio output stream from being created
    // when the extension loads.
    this.chime_.preload = 'none';
    this.chime_.src = chrome.extension.getURL(
        hotword.constants.SHARED_MODULE_ROOT + '/audio/chime.wav');
    document.body.appendChild(this.chime_);

    // In order to remove this listener, it must first be added. This handles
    // the case on first Chrome startup where this event is never registered,
    // so can't be removed when it's determined that hotwording is disabled.
    // Why not only remove the listener if it exists? Extension API events have
    // two parts to them, the Javascript listeners, and a browser-side component
    // that wakes up the extension if it's an event page. The browser-side
    // wake-up event is only removed when the number of javascript listeners
    // becomes 0. To clear the browser wake-up event, a listener first needs to
    // be added, then removed in order to drop the count to 0 and remove the
    // event.
    chrome.runtime.onStartup.addListener(this.startupListener_);
  }

  /**
   * @enum {number}
   * @private
   */
  StateManager.State_ = {
    STOPPED: 0,
    STARTING: 1,
    RUNNING: 2,
    ERROR: 3,
  };
  var State_ = StateManager.State_;

  var UmaMediaStreamOpenResults_ = {
    // These first error are defined by the MediaStream spec:
    // http://w3c.github.io/mediacapture-main/getusermedia.html#idl-def-MediaStreamError
    'NotSupportedError':
        hotword.constants.UmaMediaStreamOpenResult.NOT_SUPPORTED,
    'PermissionDeniedError':
        hotword.constants.UmaMediaStreamOpenResult.PERMISSION_DENIED,
    'ConstraintNotSatisfiedError':
        hotword.constants.UmaMediaStreamOpenResult.CONSTRAINT_NOT_SATISFIED,
    'OverconstrainedError':
        hotword.constants.UmaMediaStreamOpenResult.OVERCONSTRAINED,
    'NotFoundError': hotword.constants.UmaMediaStreamOpenResult.NOT_FOUND,
    'AbortError': hotword.constants.UmaMediaStreamOpenResult.ABORT,
    'SourceUnavailableError':
        hotword.constants.UmaMediaStreamOpenResult.SOURCE_UNAVAILABLE,
    // The next few errors are chrome-specific. See:
    // content/renderer/media/user_media_client_impl.cc
    // (UserMediaClientImpl::GetUserMediaRequestFailed)
    'PermissionDismissedError':
        hotword.constants.UmaMediaStreamOpenResult.PERMISSION_DISMISSED,
    'InvalidStateError':
        hotword.constants.UmaMediaStreamOpenResult.INVALID_STATE,
    'DevicesNotFoundError':
        hotword.constants.UmaMediaStreamOpenResult.DEVICES_NOT_FOUND,
    'InvalidSecurityOriginError':
        hotword.constants.UmaMediaStreamOpenResult.INVALID_SECURITY_ORIGIN
  };

  var UmaTriggerSources_ = {
    'launcher': hotword.constants.UmaTriggerSource.LAUNCHER,
    'ntp': hotword.constants.UmaTriggerSource.NTP_GOOGLE_COM,
    'always': hotword.constants.UmaTriggerSource.ALWAYS_ON,
    'training': hotword.constants.UmaTriggerSource.TRAINING
  };

  StateManager.prototype = {
    /**
     * Request status details update. Intended to be called from the
     * hotwordPrivate.onEnabledChanged() event.
     */
    updateStatus: function() {
      chrome.hotwordPrivate.getStatus(this.handleStatus_.bind(this));
    },

    /**
     * @return {boolean} True if google.com/NTP/launcher hotwording is enabled.
     */
    isSometimesOnEnabled: function() {
      assert(this.hotwordStatus_,
             'No hotwording status (isSometimesOnEnabled)');
      // Although the two settings are supposed to be mutually exclusive, it's
      // possible for both to be set. In that case, always-on takes precedence.
      return this.hotwordStatus_.enabled &&
          !this.hotwordStatus_.alwaysOnEnabled;
    },

    /**
     * @return {boolean} True if always-on hotwording is enabled.
     */
    isAlwaysOnEnabled: function() {
      assert(this.hotwordStatus_, 'No hotword status (isAlwaysOnEnabled)');
      return this.hotwordStatus_.alwaysOnEnabled &&
          !this.hotwordStatus_.trainingEnabled;
    },

    /**
     * @return {boolean} True if training is enabled.
     */
    isTrainingEnabled: function() {
      assert(this.hotwordStatus_, 'No hotword status (isTrainingEnabled)');
      return this.hotwordStatus_.trainingEnabled;
    },

    /**
     * Callback for hotwordPrivate.getStatus() function.
     * @param {chrome.hotwordPrivate.StatusDetails} status Current hotword
     *     status.
     * @private
     */
    handleStatus_: function(status) {
      hotword.debug('New hotword status', status);
      this.hotwordStatus_ = status;
      this.updateStateFromStatus_();

      this.onStatusChanged.dispatch();
    },

    /**
     * Updates state based on the current status.
     * @private
     */
    updateStateFromStatus_: function() {
      if (!this.hotwordStatus_)
        return;

      if (this.hotwordStatus_.enabled ||
          this.hotwordStatus_.alwaysOnEnabled ||
          this.hotwordStatus_.trainingEnabled) {
        // Detect changes to audio logging and kill the detector if that setting
        // has changed.
        if (this.hotwordStatus_.audioLoggingEnabled != this.loggingEnabled_)
          this.shutdownDetector_();
        this.loggingEnabled_ = this.hotwordStatus_.audioLoggingEnabled;

        // If the training state has changed, we need to first shut down the
        // detector so that we can restart in a different mode.
        if (this.hotwordStatus_.trainingEnabled != this.trainingEnabled_)
          this.shutdownDetector_();
        this.trainingEnabled_ = this.hotwordStatus_.trainingEnabled;

        // Start the detector if there's a session and the user is unlocked, and
        // stops it otherwise.
        if (this.sessions_.length && !this.isLocked_ &&
            this.hotwordStatus_.userIsActive) {
          this.startDetector_();
        } else {
          this.shutdownDetector_();
        }

        if (!chrome.idle.onStateChanged.hasListener(
                this.idleStateChangedListener_)) {
          chrome.idle.onStateChanged.addListener(
              this.idleStateChangedListener_);
        }
        if (!chrome.runtime.onStartup.hasListener(this.startupListener_))
          chrome.runtime.onStartup.addListener(this.startupListener_);
      } else {
        // Not enabled. Shut down if running.
        this.shutdownDetector_();

        chrome.idle.onStateChanged.removeListener(
            this.idleStateChangedListener_);
        // If hotwording isn't enabled, don't start this component extension on
        // Chrome startup. If a user enables hotwording, the status change
        // event will be fired and the onStartup event will be registered.
        chrome.runtime.onStartup.removeListener(this.startupListener_);
      }
    },

    /**
     * Starts the hotword detector.
     * @private
     */
    startDetector_: function() {
      // Last attempt to start detector resulted in an error.
      if (this.state_ == State_.ERROR) {
        // TODO(amistry): Do some error rate tracking here and disable the
        // extension if we error too often.
      }

      if (!this.pluginManager_) {
        this.state_ = State_.STARTING;
        var isHotwordStream = this.isAlwaysOnEnabled() &&
            this.hotwordStatus_.hotwordHardwareAvailable;
        this.pluginManager_ = new hotword.NaClManager(this.loggingEnabled_,
                                                      isHotwordStream);
        this.pluginManager_.addEventListener(hotword.constants.Event.READY,
                                             this.onReady_.bind(this));
        this.pluginManager_.addEventListener(hotword.constants.Event.ERROR,
                                             this.onError_.bind(this));
        this.pluginManager_.addEventListener(hotword.constants.Event.TRIGGER,
                                             this.onTrigger_.bind(this));
        this.pluginManager_.addEventListener(hotword.constants.Event.TIMEOUT,
                                             this.onTimeout_.bind(this));
        this.pluginManager_.addEventListener(
            hotword.constants.Event.SPEAKER_MODEL_SAVED,
            this.onSpeakerModelSaved_.bind(this));
        chrome.runtime.getPlatformInfo(function(platform) {
          var naclArch = platform.nacl_arch;

          // googDucking set to false so that audio output level from other tabs
          // is not affected when hotword is enabled. https://crbug.com/357773
          // content/common/media/media_stream_options.cc
          // When always-on is enabled, request the hotword stream.
          // Optional because we allow power users to bypass the hardware
          // detection via a flag, and hence the hotword stream may not be
          // available.
          var constraints = /** @type {googMediaStreamConstraints} */
              ({audio: {optional: [
                { googDucking: false },
                { googHotword: this.isAlwaysOnEnabled() }
              ]}});
          navigator.webkitGetUserMedia(
              /** @type {MediaStreamConstraints} */ (constraints),
              function(stream) {
                hotword.metrics.recordEnum(
                    hotword.constants.UmaMetrics.MEDIA_STREAM_RESULT,
                    hotword.constants.UmaMediaStreamOpenResult.SUCCESS,
                    hotword.constants.UmaMediaStreamOpenResult.MAX);
                // The detector could have been shut down before the stream
                // finishes opening.
                if (this.pluginManager_ == null) {
                  stream.getAudioTracks()[0].stop();
                  return;
                }

                if (this.isAlwaysOnEnabled())
                  this.keepAlive_.start();
                if (!this.pluginManager_.initialize(naclArch, stream)) {
                  this.state_ = State_.ERROR;
                  this.shutdownPluginManager_();
                }
              }.bind(this),
              function(error) {
                if (error.name in UmaMediaStreamOpenResults_) {
                  var metricValue = UmaMediaStreamOpenResults_[error.name];
                } else {
                  var metricValue =
                      hotword.constants.UmaMediaStreamOpenResult.UNKNOWN;
                }
                hotword.metrics.recordEnum(
                    hotword.constants.UmaMetrics.MEDIA_STREAM_RESULT,
                    metricValue,
                    hotword.constants.UmaMediaStreamOpenResult.MAX);
                this.state_ = State_.ERROR;
                this.pluginManager_ = null;
              }.bind(this));
        }.bind(this));
      } else if (this.state_ != State_.STARTING) {
        // Don't try to start a starting detector.
        this.startRecognizer_();
      }
    },

    /**
     * Start the recognizer plugin. Assumes the plugin has been loaded and is
     * ready to start.
     * @private
     */
    startRecognizer_: function() {
      assert(this.pluginManager_, 'No NaCl plugin loaded');
      if (this.state_ != State_.RUNNING) {
        this.state_ = State_.RUNNING;
        if (this.isAlwaysOnEnabled())
          this.keepAlive_.start();
        this.pluginManager_.startRecognizer(this.startMode_);
      }
      for (var i = 0; i < this.sessions_.length; i++) {
        var session = this.sessions_[i];
        if (session.startedCb_) {
          session.startedCb_();
          session.startedCb_ = null;
        }
      }
    },

    /**
     * Stops the hotword detector, if it's running.
     * @private
     */
    stopDetector_: function() {
      this.keepAlive_.stop();
      if (this.pluginManager_ && this.state_ == State_.RUNNING) {
        this.state_ = State_.STOPPED;
        this.pluginManager_.stopRecognizer();
      }
    },

    /**
     * Shuts down and removes the plugin manager, if it exists.
     * @private
     */
    shutdownPluginManager_: function() {
      this.keepAlive_.stop();
      if (this.pluginManager_) {
        this.pluginManager_.shutdown();
        this.pluginManager_ = null;
      }
    },

    /**
     * Shuts down the hotword detector.
     * @private
     */
    shutdownDetector_: function() {
      this.state_ = State_.STOPPED;
      this.shutdownPluginManager_();
    },

    /**
     * Finalizes the speaker model. Assumes the plugin has been loaded and
     * started.
     */
    finalizeSpeakerModel: function() {
      assert(this.pluginManager_,
             'Cannot finalize speaker model: No NaCl plugin loaded');
      if (this.state_ != State_.RUNNING) {
        hotword.debug('Cannot finalize speaker model: NaCl plugin not started');
        return;
      }
      this.pluginManager_.finalizeSpeakerModel();
    },

    /**
     * Handle the hotword plugin being ready to start.
     * @private
     */
    onReady_: function() {
      if (this.state_ != State_.STARTING) {
        // At this point, we should not be in the RUNNING state. Doing so would
        // imply the hotword detector was started without being ready.
        assert(this.state_ != State_.RUNNING, 'Unexpected RUNNING state');
        this.shutdownPluginManager_();
        return;
      }
      this.startRecognizer_();
    },

    /**
     * Handle an error from the hotword plugin.
     * @private
     */
    onError_: function() {
      this.state_ = State_.ERROR;
      this.shutdownPluginManager_();
    },

    /**
     * Handle hotword triggering.
     * @param {!Event} event Event containing audio log data.
     * @private
     */
    onTrigger_: function(event) {
      this.keepAlive_.stop();
      hotword.debug('Hotword triggered!');
      chrome.metricsPrivate.recordUserAction(
          hotword.constants.UmaMetrics.TRIGGER);
      assert(this.pluginManager_, 'No NaCl plugin loaded on trigger');
      // Detector implicitly stops when the hotword is detected.
      this.state_ = State_.STOPPED;

      // Play the chime.
      this.chime_.play();

      // Implicitly clear the top session. A session needs to be started in
      // order to restart the detector.
      if (this.sessions_.length) {
        var session = this.sessions_.pop();
        session.triggerCb_(event.log);

        hotword.metrics.recordEnum(
            hotword.constants.UmaMetrics.TRIGGER_SOURCE,
            UmaTriggerSources_[session.source_],
            hotword.constants.UmaTriggerSource.MAX);
      }

      // If we're in always-on mode, shut down the hotword detector. The hotword
      // stream requires that we close and re-open it after a trigger, and the
      // only way to accomplish this is to shut everything down.
      if (this.isAlwaysOnEnabled())
        this.shutdownDetector_();
    },

    /**
     * Handle hotword timeout.
     * @private
     */
    onTimeout_: function() {
      hotword.debug('Hotword timeout!');

      // We get this event when the hotword detector thinks there's a false
      // trigger. In this case, we need to shut down and restart the detector to
      // re-arm the DSP.
      this.shutdownDetector_();
      this.updateStateFromStatus_();
    },

    /**
     * Handle speaker model saved.
     * @private
     */
    onSpeakerModelSaved_: function() {
      hotword.debug('Speaker model saved!');

      if (this.sessions_.length) {
        // Only call the callback of the the top session.
        var session = this.sessions_[this.sessions_.length - 1];
        if (session.speakerModelSavedCb_)
          session.speakerModelSavedCb_();
      }
    },

    /**
     * Remove a hotwording session from the given source.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     * @private
     */
    removeSession_: function(source) {
      for (var i = 0; i < this.sessions_.length; i++) {
        if (this.sessions_[i].source_ == source) {
          this.sessions_.splice(i, 1);
          break;
        }
      }
    },

    /**
     * Start a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     * @param {!function()} startedCb Callback invoked when the session has
     *     been started successfully.
     * @param {!function()} triggerCb Callback invoked when the hotword has
     * @param {function()=} modelSavedCb Callback invoked when the speaker model
     *     has been saved.
     * @param {hotword.constants.RecognizerStartMode=} opt_mode The mode to
     *     start the recognizer in.
     */
    startSession: function(source, startedCb, triggerCb,
                           opt_modelSavedCb, opt_mode) {
      if (this.isTrainingEnabled() && opt_mode) {
        this.startMode_ = opt_mode;
      } else {
        this.startMode_ = hotword.constants.RecognizerStartMode.NORMAL;
      }
      hotword.debug('Starting session for source: ' + source);
      this.removeSession_(source);
      this.sessions_.push(new Session_(source, triggerCb, startedCb,
                                       opt_modelSavedCb));
      this.updateStateFromStatus_();
    },

    /**
     * Stops a hotwording session.
     * @param {!hotword.constants.SessionSource} source Source of the hotword
     *     session request.
     */
    stopSession: function(source) {
      hotword.debug('Stopping session for source: ' + source);
      this.removeSession_(source);
      // If this is a training session then switch the start mode back to
      // normal.
      if (source == hotword.constants.SessionSource.TRAINING)
        this.startMode_ = hotword.constants.RecognizerStartMode.NORMAL;
      this.updateStateFromStatus_();
    },

    /**
     * Handles a chrome.idle.onStateChanged event.
     * @param {!string} state State, one of "active", "idle", or "locked".
     * @private
     */
    handleIdleStateChanged_: function(state) {
      hotword.debug('Idle state changed: ' + state);
      var oldLocked = this.isLocked_;
      if (state == 'locked')
        this.isLocked_ = true;
      else
        this.isLocked_ = false;

      if (oldLocked != this.isLocked_)
        this.updateStateFromStatus_();
    },

    /**
     * Handles a chrome.runtime.onStartup event.
     * @private
     */
    handleStartup_: function() {
      // Nothing specific needs to be done here. This function exists solely to
      // be registered on the startup event.
    }
  };

  return {
    StateManager: StateManager
  };
});
