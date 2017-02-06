// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

  // Correspond to steps in the hotword opt-in flow.
  /** @const */ var START = 'start-container';
  /** @const */ var AUDIO_HISTORY = 'audio-history-container';
  /** @const */ var SPEECH_TRAINING = 'speech-training-container';
  /** @const */ var FINISH = 'finish-container';

  /**
   * These flows correspond to the three LaunchModes as defined in
   * chrome/browser/search/hotword_service.h and should be kept in sync
   * with them.
   * @const
   */
  var FLOWS = [
    [START, SPEECH_TRAINING, FINISH],
    [START, AUDIO_HISTORY, SPEECH_TRAINING, FINISH],
    [SPEECH_TRAINING, FINISH]
  ];

  /**
   * The launch mode. This enum needs to be kept in sync with that of
   * the same name in hotword_service.h.
   * @enum {number}
   */
  var LaunchMode = {
    HOTWORD_ONLY: 0,
    HOTWORD_AND_AUDIO_HISTORY: 1,
    RETRAIN: 2
  };

  /**
   * The training state.
   * @enum {string}
   */
  var TrainingState = {
    RESET: 'reset',
    TIMEOUT: 'timeout',
    ERROR: 'error',
  };

  /**
   * Class to control the page flow of the always-on hotword and
   * Audio History opt-in process.
   * @constructor
   */
  function Flow() {
    this.currentStepIndex_ = -1;
    this.currentFlow_ = [];

    /**
     * The mode that this app was launched in.
     * @private {LaunchMode}
     */
    this.launchMode_ = LaunchMode.HOTWORD_AND_AUDIO_HISTORY;

    /**
     * Whether this flow is currently in the process of training a voice model.
     * @private {boolean}
     */
    this.training_ = false;

    /**
     * The current training state.
     * @private {?TrainingState}
     */
    this.trainingState_ = null;

    /**
     * Whether an expected hotword trigger has been received, indexed by
     * training step.
     * @private {boolean[]}
     */
    this.hotwordTriggerReceived_ = [];

    /**
     * Prefix of the element ids for the page that is currently training.
     * @private {string}
     */
    this.trainingPagePrefix_ = 'speech-training';

    /**
     * Whether the speaker model for this flow has been finalized.
     * @private {boolean}
     */
    this.speakerModelFinalized_ = false;

    /**
     * ID of the currently active timeout.
     * @private {?number}
     */
    this.timeoutId_ = null;

    /**
     * Listener for the speakerModelSaved event.
     * @private {Function}
     */
    this.speakerModelFinalizedListener_ =
        this.onSpeakerModelFinalized_.bind(this);

    /**
     * Listener for the hotword trigger event.
     * @private {Function}
     */
    this.hotwordTriggerListener_ =
          this.handleHotwordTrigger_.bind(this);

    // Listen for the user locking the screen.
    chrome.idle.onStateChanged.addListener(
        this.handleIdleStateChanged_.bind(this));

    // Listen for hotword settings changes. This used to detect when the user
    // switches to a different profile.
    if (chrome.hotwordPrivate.onEnabledChanged) {
      chrome.hotwordPrivate.onEnabledChanged.addListener(
          this.handleEnabledChanged_.bind(this));
    }
  }

  /**
   * Advances the current step. Begins training if the speech-training
   * page has been reached.
   */
  Flow.prototype.advanceStep = function() {
    this.currentStepIndex_++;
    if (this.currentStepIndex_ < this.currentFlow_.length) {
      if (this.currentFlow_[this.currentStepIndex_] == SPEECH_TRAINING)
        this.startTraining();
      this.showStep_.apply(this);
    }
  };

  /**
   * Gets the appropriate flow and displays its first page.
   */
  Flow.prototype.startFlow = function() {
    if (chrome.hotwordPrivate && chrome.hotwordPrivate.getLaunchState)
      chrome.hotwordPrivate.getLaunchState(this.startFlowForMode_.bind(this));
  };

  /**
   * Starts the training process.
   */
  Flow.prototype.startTraining = function() {
    // Don't start a training session if one already exists.
    if (this.training_)
      return;

    this.training_ = true;

    if (chrome.hotwordPrivate.onHotwordTriggered &&
        !chrome.hotwordPrivate.onHotwordTriggered.hasListener(
            this.hotwordTriggerListener_)) {
      chrome.hotwordPrivate.onHotwordTriggered.addListener(
          this.hotwordTriggerListener_);
    }

    this.waitForHotwordTrigger_(0);
    if (chrome.hotwordPrivate.startTraining)
      chrome.hotwordPrivate.startTraining();
  };

  /**
   * Stops the training process.
   */
  Flow.prototype.stopTraining = function() {
    if (!this.training_)
      return;

    this.training_ = false;
    if (chrome.hotwordPrivate.onHotwordTriggered) {
      chrome.hotwordPrivate.onHotwordTriggered.
          removeListener(this.hotwordTriggerListener_);
    }
    if (chrome.hotwordPrivate.stopTraining)
      chrome.hotwordPrivate.stopTraining();
  };

  /**
   * Attempts to enable audio history for the signed-in account.
   */
  Flow.prototype.enableAudioHistory = function() {
    // Update UI
    $('audio-history-agree').disabled = true;
    $('audio-history-cancel').disabled = true;

    $('audio-history-error').hidden = true;
    $('audio-history-wait').hidden = false;

    if (chrome.hotwordPrivate.setAudioHistoryEnabled) {
      chrome.hotwordPrivate.setAudioHistoryEnabled(
          true, this.onAudioHistoryRequestCompleted_.bind(this));
    }
  };

  // ---- private methods:

  /**
   * Shows an error if the audio history setting was not enabled successfully.
   * @private
   */
  Flow.prototype.handleAudioHistoryError_ = function() {
    $('audio-history-agree').disabled = false;
    $('audio-history-cancel').disabled = false;

    $('audio-history-wait').hidden = true;
    $('audio-history-error').hidden = false;

    // Set a timeout before focusing the Enable button so that screenreaders
    // have time to announce the error first.
    this.setTimeout_(function() {
        $('audio-history-agree').focus();
    }.bind(this), 50);
  };

  /**
   * Callback for when an audio history request completes.
   * @param {chrome.hotwordPrivate.AudioHistoryState} state The audio history
   *     request state.
   * @private
   */
  Flow.prototype.onAudioHistoryRequestCompleted_ = function(state) {
    if (!state.success || !state.enabled) {
      this.handleAudioHistoryError_();
      return;
    }

    this.advanceStep();
  };

  /**
   * Shows an error if the speaker model has not been finalized.
   * @private
   */
  Flow.prototype.handleSpeakerModelFinalizedError_ = function() {
    if (!this.training_)
      return;

    if (this.speakerModelFinalized_)
      return;

    this.updateTrainingState_(TrainingState.ERROR);
    this.stopTraining();
  };

  /**
   * Handles the speaker model finalized event.
   * @private
   */
  Flow.prototype.onSpeakerModelFinalized_ = function() {
    this.speakerModelFinalized_ = true;
    if (chrome.hotwordPrivate.onSpeakerModelSaved) {
      chrome.hotwordPrivate.onSpeakerModelSaved.removeListener(
          this.speakerModelFinalizedListener_);
    }
    this.stopTraining();
    this.setTimeout_(this.finishFlow_.bind(this), 2000);
  };

  /**
   * Completes the training process.
   * @private
   */
  Flow.prototype.finishFlow_ = function() {
    if (chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled) {
      chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled(true,
          this.advanceStep.bind(this));
    }
  };

  /**
   * Handles a user clicking on the retry button.
   */
  Flow.prototype.handleRetry = function() {
    if (!(this.trainingState_ == TrainingState.TIMEOUT ||
        this.trainingState_ == TrainingState.ERROR))
      return;

    this.startTraining();
    this.updateTrainingState_(TrainingState.RESET);
  };

  // ---- private methods:

  /**
   * Completes the training process.
   * @private
   */
  Flow.prototype.finalizeSpeakerModel_ = function() {
    if (!this.training_)
      return;

    // Listen for the success event from the NaCl module.
    if (chrome.hotwordPrivate.onSpeakerModelSaved &&
        !chrome.hotwordPrivate.onSpeakerModelSaved.hasListener(
            this.speakerModelFinalizedListener_)) {
      chrome.hotwordPrivate.onSpeakerModelSaved.addListener(
          this.speakerModelFinalizedListener_);
    }

    this.speakerModelFinalized_ = false;
    this.setTimeout_(this.handleSpeakerModelFinalizedError_.bind(this), 30000);
    if (chrome.hotwordPrivate.finalizeSpeakerModel)
      chrome.hotwordPrivate.finalizeSpeakerModel();
  };

  /**
   * Returns the current training step.
   * @param {string} curStepClassName The name of the class of the current
   *     training step.
   * @return {Object} The current training step, its index, and an array of
   *     all training steps. Any of these can be undefined.
   * @private
   */
  Flow.prototype.getCurrentTrainingStep_ = function(curStepClassName) {
    var steps =
        $(this.trainingPagePrefix_ + '-training').querySelectorAll('.train');
    var curStep =
        $(this.trainingPagePrefix_ + '-training').querySelector('.listening');

    return {current: curStep,
            index: Array.prototype.indexOf.call(steps, curStep),
            steps: steps};
  };

  /**
   * Updates the training state.
   * @param {TrainingState} state The training state.
   * @private
   */
  Flow.prototype.updateTrainingState_ = function(state) {
    this.trainingState_ = state;
    this.updateErrorUI_();
  };

  /**
   * Waits two minutes and then checks for a training error.
   * @param {number} index The index of the training step.
   * @private
   */
  Flow.prototype.waitForHotwordTrigger_ = function(index) {
    if (!this.training_)
      return;

    this.hotwordTriggerReceived_[index] = false;
    this.setTimeout_(this.handleTrainingTimeout_.bind(this, index), 120000);
  };

  /**
   * Checks for and handles a training error.
   * @param {number} index The index of the training step.
   * @private
   */
  Flow.prototype.handleTrainingTimeout_ = function(index) {
    if (this.hotwordTriggerReceived_[index])
      return;

    this.timeoutTraining_();
  };

  /**
   * Times out training and updates the UI to show a "retry" message, if
   * currently training.
   * @private
   */
  Flow.prototype.timeoutTraining_ = function() {
    if (!this.training_)
      return;

    this.clearTimeout_();
    this.updateTrainingState_(TrainingState.TIMEOUT);
    this.stopTraining();
  };

  /**
   * Sets a timeout. If any timeout is active, clear it.
   * @param {Function} func The function to invoke when the timeout occurs.
   * @param {number} delay Timeout delay in milliseconds.
   * @private
   */
  Flow.prototype.setTimeout_ = function(func, delay) {
    this.clearTimeout_();
    this.timeoutId_ = setTimeout(function() {
      this.timeoutId_ = null;
      func();
    }, delay);
  };

  /**
   * Clears any currently active timeout.
   * @private
   */
  Flow.prototype.clearTimeout_ = function() {
    if (this.timeoutId_ != null) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  };

  /**
   * Updates the training error UI.
   * @private
   */
  Flow.prototype.updateErrorUI_ = function() {
    if (!this.training_)
      return;

    var trainingSteps = this.getCurrentTrainingStep_('listening');
    var steps = trainingSteps.steps;

    $(this.trainingPagePrefix_ + '-toast').hidden =
        this.trainingState_ != TrainingState.TIMEOUT;
    if (this.trainingState_ == TrainingState.RESET) {
      // We reset the training to begin at the first step.
      // The first step is reset to 'listening', while the rest
      // are reset to 'not-started'.
      var prompt = loadTimeData.getString('trainingFirstPrompt');
      for (var i = 0; i < steps.length; ++i) {
        steps[i].classList.remove('recorded');
        if (i == 0) {
          steps[i].classList.remove('not-started');
          steps[i].classList.add('listening');
        } else {
          steps[i].classList.add('not-started');
          if (i == steps.length - 1)
            prompt = loadTimeData.getString('trainingLastPrompt');
          else
            prompt = loadTimeData.getString('trainingMiddlePrompt');
        }
        steps[i].querySelector('.text').textContent = prompt;
      }

      // Reset the buttonbar.
      $(this.trainingPagePrefix_ + '-processing').hidden = true;
      $(this.trainingPagePrefix_ + '-wait').hidden = false;
      $(this.trainingPagePrefix_ + '-error').hidden = true;
      $(this.trainingPagePrefix_ + '-retry').hidden = true;
    } else if (this.trainingState_ == TrainingState.TIMEOUT) {
      var curStep = trainingSteps.current;
      if (curStep) {
        curStep.classList.remove('listening');
        curStep.classList.add('not-started');
      }

      // Set a timeout before focusing the Retry button so that screenreaders
      // have time to announce the timeout first.
      this.setTimeout_(function() {
        $(this.trainingPagePrefix_ + '-toast').children[1].focus();
      }.bind(this), 50);
    } else if (this.trainingState_ == TrainingState.ERROR) {
      // Update the buttonbar.
      $(this.trainingPagePrefix_ + '-wait').hidden = true;
      $(this.trainingPagePrefix_ + '-error').hidden = false;
      $(this.trainingPagePrefix_ + '-retry').hidden = false;
      $(this.trainingPagePrefix_ + '-processing').hidden = false;

      // Set a timeout before focusing the Retry button so that screenreaders
      // have time to announce the error first.
      this.setTimeout_(function() {
        $(this.trainingPagePrefix_ + '-retry').children[0].focus();
      }.bind(this), 50);
    }
  };

  /**
   * Handles a hotword trigger event and updates the training UI.
   * @private
   */
  Flow.prototype.handleHotwordTrigger_ = function() {
    var trainingSteps = this.getCurrentTrainingStep_('listening');

    if (!trainingSteps.current)
      return;

    var index = trainingSteps.index;
    this.hotwordTriggerReceived_[index] = true;

    trainingSteps.current.querySelector('.text').textContent =
        loadTimeData.getString('trainingRecorded');
    trainingSteps.current.classList.remove('listening');
    trainingSteps.current.classList.add('recorded');

    if (trainingSteps.steps[index + 1]) {
      trainingSteps.steps[index + 1].classList.remove('not-started');
      trainingSteps.steps[index + 1].classList.add('listening');
      this.waitForHotwordTrigger_(index + 1);
      return;
    }

    // Only the last step makes it here.
    var buttonElem = $(this.trainingPagePrefix_ + '-processing').hidden = false;
    this.finalizeSpeakerModel_();
  };

  /**
   * Handles a chrome.idle.onStateChanged event and times out the training if
   * the state is "locked".
   * @param {!string} state State, one of "active", "idle", or "locked".
   * @private
   */
  Flow.prototype.handleIdleStateChanged_ = function(state) {
    if (state == 'locked')
      this.timeoutTraining_();
  };

  /**
   * Handles a chrome.hotwordPrivate.onEnabledChanged event and times out
   * training if the user is no longer the active user (user switches profiles).
   * @private
   */
  Flow.prototype.handleEnabledChanged_ = function() {
    if (chrome.hotwordPrivate.getStatus) {
      chrome.hotwordPrivate.getStatus(function(status) {
        if (status.userIsActive)
          return;

        this.timeoutTraining_();
      }.bind(this));
    }
  };

  /**
   * Gets and starts the appropriate flow for the launch mode.
   * @param {chrome.hotwordPrivate.LaunchState} state Launch state of the
   *     Hotword Audio Verification App.
   * @private
   */
  Flow.prototype.startFlowForMode_ = function(state) {
    this.launchMode_ = state.launchMode;
    assert(state.launchMode >= 0 && state.launchMode < FLOWS.length,
           'Invalid Launch Mode.');
    this.currentFlow_ = FLOWS[state.launchMode];
    if (state.launchMode == LaunchMode.HOTWORD_ONLY) {
      $('intro-description-audio-history-enabled').hidden = false;
    } else if (state.launchMode == LaunchMode.HOTWORD_AND_AUDIO_HISTORY) {
      $('intro-description').hidden = false;
    }

    this.advanceStep();
  };

  /**
   * Displays the current step. If the current step is not the first step,
   * also hides the previous step. Focuses the current step's first button.
   * @private
   */
  Flow.prototype.showStep_ = function() {
    var currentStepId = this.currentFlow_[this.currentStepIndex_];
    var currentStep = document.getElementById(currentStepId);
    currentStep.hidden = false;

    cr.ui.setInitialFocus(currentStep);

    var previousStep = null;
    if (this.currentStepIndex_ > 0)
      previousStep = this.currentFlow_[this.currentStepIndex_ - 1];

    if (previousStep)
      document.getElementById(previousStep).hidden = true;

    chrome.app.window.current().show();
  };

  window.Flow = Flow;
})();
