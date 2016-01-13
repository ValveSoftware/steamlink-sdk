// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The manager of audio streams.
 */

cr.define('speech', function() {
  'use strict';

  /**
   * The enum of the status of hotword audio recognition.
   *
   * @enum {number}
   */
  var AudioState = {
    STOPPED: 0,
    LISTENING: 1
  };

  /**
   * @constructor
   * @extends {cr.EventTarget}
   */
  function AudioManager() {
    var audioContext = new window.AudioContext();
    this.sampleRate = audioContext.sampleRate;
    this.audioProc_ = null;
    this.audioIn_ = null;
    this.stream_ = null;
    this.state = AudioState.STOPPED;
  };

  AudioManager.prototype.__proto__ = cr.EventTarget.prototype;

  /**
   * Called when the audio data arrives.
   *
   * @param {Event} audioEvent The audio event.
   * @private
   */
  AudioManager.prototype.onAudioProcess_ = function(audioEvent) {
    var data = audioEvent.inputBuffer.getChannelData(0);
    var intData = new Int16Array(data.length);
    for (var i = 0; i < data.length; ++i)
      intData[i] = Math.round(data[i] * 32767);
    var event = new Event('audio');
    event.data = intData;
    this.dispatchEvent(event);
  };

  /**
   * Called when the audio stream is ready.
   *
   * @param {MediaStream} stream The media stream which is now available.
   * @private
   */
  AudioManager.prototype.onAudioReady_ = function(stream) {
    var audioContext = new window.AudioContext();
    this.stream_ = stream;
    this.audioIn_ = audioContext.createMediaStreamSource(stream);
    this.audioProc_ = audioContext.createScriptProcessor(
        4096 /* buffer size */, 1 /* channels */, 1 /* channels */);
    this.audioProc_.onaudioprocess = this.onAudioProcess_.bind(this);

    this.audioIn_.connect(this.audioProc_);
    this.audioProc_.connect(audioContext.destination);
    this.state = AudioState.LISTENING;
  };

  /**
   * Starts the audio processing.
   */
  AudioManager.prototype.start = function() {
    if (this.state == AudioState.LISTENING)
      return;

    navigator.webkitGetUserMedia(
        {audio: true},
        this.onAudioReady_.bind(this),
        function(msg) { console.error('Failed to getUserMedia: ' + msg); });
  };

  /**
   * Stops the audio processing.
   */
  AudioManager.prototype.stop = function() {
    if (this.state != AudioState.LISTENING)
      return;
    this.audioProc_.disconnect();
    this.audioIn_.disconnect();
    var audioTracks = this.stream_.getAudioTracks();
    for (var i = 0; i < audioTracks.length; ++i) {
      audioTracks[i].stop();
    }
    this.audioProc_ = null;
    this.audioIn_ = null;
    this.stream_ = null;
    this.state = AudioState.STOPPED;
  };

  return {
    AudioManager: AudioManager,
    AudioState: AudioState
  };
});
