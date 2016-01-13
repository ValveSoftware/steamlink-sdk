// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The class to Manage both offline / online speech recognition.
 */

<include src="plugin_manager.js"/>
<include src="audio_manager.js"/>
<include src="speech_recognition_manager.js"/>

cr.define('speech', function() {
  'use strict';

  /**
   * The state of speech recognition.
   *
   * @enum {string}
   */
  var SpeechState = {
    READY: 'READY',
    HOTWORD_RECOGNIZING: 'HOTWORD_RECOGNIZING',
    RECOGNIZING: 'RECOGNIZING',
    IN_SPEECH: 'IN_SPEECH',
    STOPPING: 'STOPPING',
    NETWORK_ERROR: 'NETWORK_ERROR'
  };

  /**
   * The time to show the network error message in seconds.
   *
   * @const {number}
   */
  var SPEECH_ERROR_TIMEOUT = 3;

  /**
   * Checks the prefix for the hotword module based on the language. This is
   * fragile if the file structure has changed.
   */
  function getHotwordPrefix() {
    var prefix = navigator.language.toLowerCase();
    if (prefix == 'en-gb')
      return prefix;
    var hyphen = prefix.indexOf('-');
    if (hyphen >= 0)
      prefix = prefix.substr(0, hyphen);
    if (prefix == 'en')
      prefix = '';
    return prefix;
  }

  /**
   * @constructor
   */
  function SpeechManager() {
    this.audioManager_ = new speech.AudioManager();
    this.audioManager_.addEventListener('audio', this.onAudioLevel_.bind(this));
    this.speechRecognitionManager_ = new speech.SpeechRecognitionManager(this);
    this.errorTimeoutId_ = null;
  }

  /**
   * Updates the state.
   *
   * @param {SpeechState} newState The new state.
   * @private
   */
  SpeechManager.prototype.setState_ = function(newState) {
    if (this.state == newState)
      return;

    this.state = newState;
    chrome.send('setSpeechRecognitionState', [this.state]);
  };

  /**
   * Called with the mean audio level when audio data arrives.
   *
   * @param {cr.event.Event} event The event object for the audio data.
   * @private
   */
  SpeechManager.prototype.onAudioLevel_ = function(event) {
    var data = event.data;
    var level = 0;
    for (var i = 0; i < data.length; ++i)
      level += Math.abs(data[i]);
    level /= data.length;
    chrome.send('speechSoundLevel', [level]);
  };

  /**
   * Called when the hotword recognizer is ready.
   *
   * @param {PluginManager} pluginManager The hotword plugin manager which gets
   *   ready.
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizerReady_ = function(pluginManager) {
    this.pluginManager_ = pluginManager;
    this.audioManager_.addEventListener(
        'audio', pluginManager.sendAudioData.bind(pluginManager));
    this.pluginManager_.startRecognizer();
    this.audioManager_.start();
    this.setState_(SpeechState.HOTWORD_RECOGNIZING);
  };

  /**
   * Called when an error happens for loading the hotword recognizer.
   *
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizerLoadError_ = function() {
    this.setHotwordEnabled(false);
    this.setState_(SpeechState.READY);
  };

  /**
   * Called when the hotword is recognized.
   *
   * @private
   */
  SpeechManager.prototype.onHotwordRecognized_ = function() {
    if (this.state != SpeechState.HOTWORD_RECOGNIZING)
      return;
    this.pluginManager_.stopRecognizer();
    this.speechRecognitionManager_.start();
  };

  /**
   * Called when the speech recognition has happened.
   *
   * @param {string} result The speech recognition result.
   * @param {boolean} isFinal Whether the result is final or not.
   */
  SpeechManager.prototype.onSpeechRecognized = function(result, isFinal) {
    chrome.send('speechResult', [result, isFinal]);
    if (isFinal)
      this.speechRecognitionManager_.stop();
  };

  /**
   * Called when the speech recognition has started.
   */
  SpeechManager.prototype.onSpeechRecognitionStarted = function() {
    this.setState_(SpeechState.RECOGNIZING);
  };

  /**
   * Called when the speech recognition has ended.
   */
  SpeechManager.prototype.onSpeechRecognitionEnded = function() {
    // Do not handle the speech recognition ends if it ends due to an error
    // because an error message should be shown for a while.
    // See onSpeechRecognitionError.
    if (this.state == SpeechState.NETWORK_ERROR)
      return;

    // Restarts the hotword recognition.
    if (this.state != SpeechState.STOPPING && this.pluginManager_) {
      this.pluginManager_.startRecognizer();
      this.audioManager_.start();
      this.setState_(SpeechState.HOTWORD_RECOGNIZING);
    } else {
      this.audioManager_.stop();
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Called when a speech has started.
   */
  SpeechManager.prototype.onSpeechStarted = function() {
    if (this.state == SpeechState.RECOGNIZING)
      this.setState_(SpeechState.IN_SPEECH);
  };

  /**
   * Called when a speech has ended.
   */
  SpeechManager.prototype.onSpeechEnded = function() {
    if (this.state == SpeechState.IN_SPEECH)
      this.setState_(SpeechState.RECOGNIZING);
  };

  /**
   * Called when the speech manager should recover from the error state.
   *
   * @private
   */
  SpeechManager.prototype.onSpeechRecognitionErrorTimeout_ = function() {
    this.errorTimeoutId_ = null;
    this.setState_(SpeechState.READY);
    this.onSpeechRecognitionEnded();
  };

  /**
   * Called when an error happened during the speech recognition.
   *
   * @param {SpeechRecognitionError} e The error object.
   */
  SpeechManager.prototype.onSpeechRecognitionError = function(e) {
    if (e.error == 'network') {
      this.setState_(SpeechState.NETWORK_ERROR);
      this.errorTimeoutId_ = window.setTimeout(
          this.onSpeechRecognitionErrorTimeout_.bind(this),
          SPEECH_ERROR_TIMEOUT * 1000);
    } else {
      if (this.state != SpeechState.STOPPING)
        this.setState_(SpeechState.READY);
    }
  };

  /**
   * Changes the availability of the hotword plugin.
   *
   * @param {boolean} enabled Whether enabled or not.
   */
  SpeechManager.prototype.setHotwordEnabled = function(enabled) {
    var recognizer = $('recognizer');
    if (enabled) {
      if (recognizer)
        return;
      if (!this.naclArch)
        return;

      var prefix = getHotwordPrefix();
      var pluginManager = new speech.PluginManager(
          prefix,
          this.onHotwordRecognizerReady_.bind(this),
          this.onHotwordRecognized_.bind(this),
          this.onHotwordRecognizerLoadError_.bind(this));
      var modelUrl = 'chrome://app-list/_platform_specific/' + this.naclArch +
          '_' + prefix + '/hotword.data';
      pluginManager.scheduleInitialize(this.audioManager_.sampleRate, modelUrl);
    } else {
      if (!recognizer)
        return;
      document.body.removeChild(recognizer);
      this.pluginManager_ = null;
      if (this.state == SpeechState.HOTWORD_RECOGNIZING) {
        this.audioManager_.stop();
        this.setState_(SpeechState.READY);
      }
    }
  };

  /**
   * Sets the NaCl architecture for the hotword module.
   *
   * @param {string} arch The architecture.
   */
  SpeechManager.prototype.setNaclArch = function(arch) {
    this.naclArch = arch;
  };

  /**
   * Called when the app-list bubble is shown.
   *
   * @param {boolean} hotwordEnabled Whether the hotword is enabled or not.
   */
  SpeechManager.prototype.onShown = function(hotwordEnabled) {
    this.setHotwordEnabled(hotwordEnabled);

    // No one sets the state if the content is initialized on shown but hotword
    // is not enabled. Sets the state in such case.
    if (!this.state && !hotwordEnabled)
      this.setState_(SpeechState.READY);
  };

  /**
   * Called when the app-list bubble is hidden.
   */
  SpeechManager.prototype.onHidden = function() {
    this.setHotwordEnabled(false);

    // SpeechRecognition is asynchronous.
    this.audioManager_.stop();
    if (this.state == SpeechState.RECOGNIZING ||
        this.state == SpeechState.IN_SPEECH) {
      this.setState_(SpeechState.STOPPING);
      this.speechRecognitionManager_.stop();
    } else {
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Toggles the current state of speech recognition.
   */
  SpeechManager.prototype.toggleSpeechRecognition = function() {
    if (this.state == SpeechState.NETWORK_ERROR) {
      if (this.errorTimeoutId_)
        window.clearTimeout(this.errorTimeoutId_);
      this.onSpeechRecognitionErrorTimeout_();
    } else if (this.state == SpeechState.RECOGNIZING ||
        this.state == SpeechState.IN_SPEECH) {
      this.audioManager_.stop();
      this.speechRecognitionManager_.stop();
    } else {
      if (this.pluginManager_)
        this.pluginManager_.stopRecognizer();
      if (this.audioManager_.state == speech.AudioState.STOPPED)
        this.audioManager_.start();
      this.speechRecognitionManager_.start();
    }
  };

  return {
    SpeechManager: SpeechManager
  };
});
