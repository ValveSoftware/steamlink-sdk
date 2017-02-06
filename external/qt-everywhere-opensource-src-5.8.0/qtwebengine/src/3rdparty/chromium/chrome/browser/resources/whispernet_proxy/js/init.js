// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Global holding our NaclBridge.
var whispernetNacl = null;

// Encoders and decoders for each client.
var whisperEncoders = {};
var whisperDecoders = {};

/**
 * Initialize the whispernet encoder and decoder.
 * Call this before any other functions.
 * @param {string} clientId A string identifying the requester.
 * @param {Object} audioParams Audio parameters for token encoding and decoding.
 */
function audioConfig(clientId, audioParams) {
  if (!whispernetNacl) {
    chrome.copresencePrivate.sendInitialized(false);
    return;
  }

  console.log('Configuring encoder and decoder for client ' + clientId);
  whisperEncoders[clientId] =
      new WhisperEncoder(audioParams.paramData, whispernetNacl, clientId);
  whisperDecoders[clientId] =
      new WhisperDecoder(audioParams.paramData, whispernetNacl, clientId);
}

/**
 * Sends a request to whispernet to encode a token.
 * @param {string} clientId A string identifying the requester.
 * @param {Object} params Encode token parameters object.
 */
function encodeTokenRequest(clientId, params) {
  if (whisperEncoders[clientId]) {
    whisperEncoders[clientId].encode(params);
  } else {
    console.error('encodeTokenRequest: Whisper not initialized for client ' +
        clientId);
  }
}

/**
 * Sends a request to whispernet to decode samples.
 * @param {string} clientId A string identifying the requester.
 * @param {Object} params Process samples parameters object.
 */
function decodeSamplesRequest(clientId, params) {
  if (whisperDecoders[clientId]) {
    whisperDecoders[clientId].processSamples(params);
  } else {
    console.error('decodeSamplesRequest: Whisper not initialized for client ' +
        clientId);
  }
}

/**
 * Initialize our listeners and signal that the extension is loaded.
 */
function onWhispernetLoaded() {
  console.log('init: Nacl ready!');

  // Setup all the listeners for the private API.
  chrome.copresencePrivate.onConfigAudio.addListener(audioConfig);
  chrome.copresencePrivate.onEncodeTokenRequest.addListener(encodeTokenRequest);
  chrome.copresencePrivate.onDecodeSamplesRequest.addListener(
      decodeSamplesRequest);

  // This first initialized is sent to indicate that the library is loaded.
  // Every other time, it will be sent only when Chrome wants to reinitialize
  // the encoder and decoder.
  chrome.copresencePrivate.sendInitialized(true);
}

/**
 * Initialize the whispernet Nacl bridge.
 */
function initWhispernet() {
  console.log('init: Starting Nacl bridge.');
  // TODO(rkc): Figure out how to embed the .nmf and the .pexe into component
  // resources without having to rename them to .js.
  whispernetNacl = new NaclBridge('whispernet_proxy.nmf.png',
                                  onWhispernetLoaded);
}

window.addEventListener('DOMContentLoaded', initWhispernet);
