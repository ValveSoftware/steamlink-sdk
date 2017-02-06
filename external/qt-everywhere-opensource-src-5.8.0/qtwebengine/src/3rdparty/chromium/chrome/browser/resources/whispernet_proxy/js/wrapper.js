// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Function to convert an array of bytes to a base64 string
 * TODO(rkc): Change this to use a Uint8array instead of a string.
 * @param {string} bytes String containing the bytes we want to convert.
 * @return {string} String containing the base64 representation.
 */
function bytesToBase64(bytes) {
  var bstr = '';
  for (var i = 0; i < bytes.length; ++i)
    bstr += String.fromCharCode(bytes[i]);
  return btoa(bstr).replace(/=/g, '');
}

/**
 * Function to convert a string to an array of bytes.
 * @param {string} str String to convert.
 * @return {Array} Array containing the string.
 */
function stringToArray(str) {
  var buffer = [];
  for (var i = 0; i < str.length; ++i)
    buffer[i] = str.charCodeAt(i);
  return buffer;
}

/**
 * Creates a whispernet encoder.
 * @constructor
 * @param {Object} params Audio parameters for the whispernet encoder.
 * @param {Object} whisperNacl The NaclBridge object, used to communicate with
 *     the whispernet wrapper.
 * @param {string} clientId A string identifying the requester.
 */
function WhisperEncoder(params, whisperNacl, clientId) {
  this.whisperNacl_ = whisperNacl;
  this.whisperNacl_.addListener(this.onNaclMessage_.bind(this));
  this.clientId_ = clientId;

  var msg = {
    type: 'initialize_encoder',
    client_id: clientId,
    params: params
  };

  this.whisperNacl_.send(msg);
}

/**
 * Method to encode a token.
 * @param {Object} params Encode token parameters object.
 */
WhisperEncoder.prototype.encode = function(params) {
  // Pad the token before decoding it.
  var token = params.token.token;
  while (token.length % 4 > 0)
    token += '=';

  var msg = {
    type: 'encode_token',
    client_id: this.clientId_,
    // Trying to send the token in binary form to Nacl doesn't work correctly.
    // We end up with the correct string + a bunch of extra characters. This is
    // true of returning a binary string too; hence we communicate back and
    // forth by converting the bytes into an array of integers.
    token: stringToArray(atob(token)),
    repetitions: params.repetitions,
    use_dtmf: params.token.audible,
    use_crc: params.tokenParams.crc,
    use_parity: params.tokenParams.parity
  };

  this.whisperNacl_.send(msg);
};

/**
 * Method to handle messages from the whispernet NaCl wrapper.
 * @param {Event} e Event from the whispernet wrapper.
 * @private
 */
WhisperEncoder.prototype.onNaclMessage_ = function(e) {
  var msg = e.data;
  if (msg.type == 'encode_token_response' && msg.client_id == this.clientId_) {
    chrome.copresencePrivate.sendSamples(this.clientId_,
        { token: bytesToBase64(msg.token), audible: msg.audible }, msg.samples);
  }
};

/**
 * Creates a whispernet decoder.
 * @constructor
 * @param {Object} params Audio parameters for the whispernet decoder.
 * @param {Object} whisperNacl The NaclBridge object, used to communicate with
 *     the whispernet wrapper.
 * @param {string} clientId A string identifying the requester.
 */
function WhisperDecoder(params, whisperNacl, clientId) {
  this.whisperNacl_ = whisperNacl;
  this.whisperNacl_.addListener(this.onNaclMessage_.bind(this));
  this.clientId_ = clientId;

  var msg = {
    type: 'initialize_decoder',
    client_id: clientId,
    params: params
  };
  this.whisperNacl_.send(msg);
}

/**
 * Method to request the decoder to process samples.
 * @param {Object} params Process samples parameters object.
 */
WhisperDecoder.prototype.processSamples = function(params) {
  var msg = {
    type: 'decode_tokens',
    client_id: this.clientId_,
    data: params.samples,

    decode_audible: params.decodeAudible,
    token_length_dtmf: params.audibleTokenParams.length,
    crc_dtmf: params.audibleTokenParams.crc,
    parity_dtmf: params.audibleTokenParams.parity,

    decode_inaudible: params.decodeInaudible,
    token_length_dsss: params.inaudibleTokenParams.length,
    crc_dsss: params.inaudibleTokenParams.crc,
    parity_dsss: params.inaudibleTokenParams.parity,
  };

  this.whisperNacl_.send(msg);
};

/**
 * Method to handle messages from the whispernet NaCl wrapper.
 * @param {Event} e Event from the whispernet wrapper.
 * @private
 */
WhisperDecoder.prototype.onNaclMessage_ = function(e) {
  var msg = e.data;
  if (msg.type == 'decode_tokens_response' && msg.client_id == this.clientId_) {
    this.handleCandidates_(msg.tokens, msg.audible);
  }
};

/**
 * Method to receive tokens from the decoder and process and forward them to the
 * token callback registered with us.
 * @param {!Array.string} candidates Array of token candidates.
 * @param {boolean} audible Whether the received candidates are from the audible
 *     decoder or not.
 * @private
 */
WhisperDecoder.prototype.handleCandidates_ = function(candidates, audible) {
  if (!candidates || candidates.length == 0)
    return;

  var returnCandidates = [];
  for (var i = 0; i < candidates.length; ++i) {
    returnCandidates[i] = { token: bytesToBase64(candidates[i]),
                            audible: audible };
  }
  chrome.copresencePrivate.sendFound(this.clientId_, returnCandidates);
};
