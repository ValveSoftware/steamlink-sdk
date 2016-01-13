// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The manager of offline hotword speech recognizer plugin.
 */

cr.define('speech', function() {
  'use strict';

  /** The timeout milliseconds to load the model file. */
  var MODEL_LOAD_TIMEOUT = 2000;

  /**
   * The type of the plugin state.
   ** @enum {number}
   */
  var PluginState = {
    UNINITIALIZED: 0,
    LOADED: 1,
    READY: 2,
    RECOGNIZING: 3
  };

  /**
   * The command names of the plugin.
   * @enum {string}
   */
  var pluginCommands = {
    SET_SAMPLING_RATE: 'h',
    SET_CONFIG: 'm',
    START_RECOGNIZING: 'r',
    STOP_RECOGNIZING: 's'
  };

  /**
   * The regexp pattern of the hotword recognition result.
   */
  var recognitionPattern = /^(HotwordFiredEvent:|hotword)/;

  /**
   * @constructor
   */
  function PluginManager(prefix, onReady, onRecognized, onError) {
    this.state = PluginState.UNINITIALIZED;
    this.onReady_ = onReady;
    this.onRecognized_ = onRecognized;
    this.onError_ = onError;
    this.samplingRate_ = null;
    this.config_ = null;
    this.modelLoadTimeoutId_ = null;
    var recognizer = $('recognizer');
    if (!recognizer) {
      recognizer = document.createElement('EMBED');
      recognizer.id = 'recognizer';
      recognizer.type = 'application/x-nacl';
      recognizer.src = 'chrome://app-list/hotword_' + prefix + '.nmf';
      recognizer.width = '1';
      recognizer.height = '1';
      document.body.appendChild(recognizer);
    }
    recognizer.addEventListener('error', onError);
    recognizer.addEventListener('message', this.onMessage_.bind(this));
    recognizer.addEventListener('load', this.onLoad_.bind(this));
  };

  /**
   * The event handler of the plugin status.
   *
   * @param {Event} messageEvent the event object from the plugin.
   * @private
   */
  PluginManager.prototype.onMessage_ = function(messageEvent) {
    if (messageEvent.data == 'audio') {
      var wasNotReady = this.state < PluginState.READY;
      this.state = PluginState.RECOGNIZING;
      if (wasNotReady) {
        window.clearTimeout(this.modelLoadTimeoutId_);
        this.modelLoadTimeoutId_ = null;
        this.onReady_(this);
      }
    } else if (messageEvent.data == 'stopped' &&
               this.state == PluginState.RECOGNIZING) {
      this.state = PluginState.READY;
    } else if (recognitionPattern.exec(messageEvent.data)) {
      this.onRecognized_();
    }
  };

  /**
   * The event handler when the plugin is loaded.
   *
   * @private
   */
  PluginManager.prototype.onLoad_ = function() {
    if (this.state == PluginState.UNINITIALIZED) {
      this.state = PluginState.LOADED;
      if (this.samplingRate_ && this.config_)
        this.initialize_(this.samplingRate_, this.config_);
      // Sets the timeout for initialization in case that NaCl module failed to
      // respond during the initialization.
      this.modelLoadTimeoutId_ = window.setTimeout(
          this.onError_, MODEL_LOAD_TIMEOUT);
    }
  };

  /**
   * Sends the initialization messages to the plugin. This method is private.
   * The initialization will happen from onLoad_ or scheduleInitialize.
   *
   * @param {number} samplingRate the sampling rate the plugin accepts.
   * @param {string} config the url of the config file.
   * @private
   */
  PluginManager.prototype.initialize_ = function(samplingRate, config) {
    $('recognizer').postMessage(
        pluginCommands.SET_SAMPLING_RATE + samplingRate);
    $('recognizer').postMessage(pluginCommands.SET_CONFIG + config);
  };

  /**
   * Initializes the plugin with the specified parameter, or schedules the
   * initialization if the plugin is not ready.
   *
   * @param {number} samplingRate the sampling rate the plugin accepts.
   * @param {string} config the url of the config file.
   */
  PluginManager.prototype.scheduleInitialize = function(samplingRate, config) {
    if (this.state == PluginState.UNINITIALIZED) {
      this.samplingRate_ = samplingRate;
      this.config_ = config;
    } else {
      this.initialize_(samplingRate, config);
    }
  };

  /**
   * Asks the plugin to start recognizing the hotword.
   */
  PluginManager.prototype.startRecognizer = function() {
    if (this.state == PluginState.READY)
      $('recognizer').postMessage(pluginCommands.START_RECOGNIZING);
  };

  /**
   * Asks the plugin to stop recognizing the hotword.
   */
  PluginManager.prototype.stopRecognizer = function() {
    if (this.state == PluginState.RECOGNIZING)
      $('recognizer').postMessage(pluginCommands.STOP_RECOGNIZING);
  };

  /**
   * Sends the actual audio wave data.
   *
   * @param {cr.event.Event} event The event for the audio data.
   */
  PluginManager.prototype.sendAudioData = function(event) {
    if (this.state == PluginState.RECOGNIZING)
      $('recognizer').postMessage(event.data.buffer);
  };

  return {
    PluginManager: PluginManager,
    PluginState: PluginState,
  };
});
