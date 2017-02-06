// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Constructor for the Nacl bridge to the whispernet wrapper.
 * @param {string} nmf The relative path to the nmf containing the location of
 * the whispernet Nacl wrapper.
 * @param {function()} readyCallback Callback to be called once we've loaded the
 * whispernet wrapper.
 */
function NaclBridge(nmf, readyCallback) {
  this.readyCallback_ = readyCallback;
  this.callbacks_ = [];
  this.isEnabled_ = false;
  this.naclId_ = this.loadNacl_(nmf);
}

/**
 * Method to send generic byte data to the whispernet wrapper.
 * @param {Object} data Raw data to send to the whispernet wrapper.
 */
NaclBridge.prototype.send = function(data) {
  if (this.isEnabled_) {
    this.embed_.postMessage(data);
  } else {
    console.error('Whisper Nacl Bridge not initialized!');
  }
};

/**
 * Method to add a listener to Nacl messages received by this bridge.
 * @param {function(Event)} messageCallback Callback to receive the messsage.
 */
NaclBridge.prototype.addListener = function(messageCallback) {
  this.callbacks_.push(messageCallback);
};

/**
 * Method that receives Nacl messages and forwards them to registered
 * callbacks.
 * @param {Event} e Event from the whispernet wrapper.
 * @private
 */
NaclBridge.prototype.onMessage_ = function(e) {
  if (this.isEnabled_) {
    this.callbacks_.forEach(function(callback) {
      callback(e);
    });
  }
};

/**
 * Injects the <embed> for this nacl manifest URL, generating a unique ID.
 * @param {string} manifestUrl Url to the nacl manifest to load.
 * @return {number} generated ID.
 * @private
 */
NaclBridge.prototype.loadNacl_ = function(manifestUrl) {
  var id = 'nacl-' + Math.floor(Math.random() * 10000);
  this.embed_ = document.createElement('embed');
  this.embed_.name = 'nacl_module';
  this.embed_.width = 1;
  this.embed_.height = 1;
  this.embed_.src = manifestUrl;
  this.embed_.id = id;
  this.embed_.type = 'application/x-pnacl';

  // Wait for the element to load and callback.
  this.embed_.addEventListener('load', this.onNaclReady_.bind(this));
  this.embed_.addEventListener('error', this.onNaclError_.bind(this));

  // Inject the embed string into the page.
  document.body.appendChild(this.embed_);

  // Listen for messages from the NaCl module.
  window.addEventListener('message', this.onMessage_.bind(this), true);
  return id;
};

/**
 * Called when the Whispernet wrapper is loaded.
 * @private
 */
NaclBridge.prototype.onNaclReady_ = function() {
  this.isEnabled_ = true;
  if (this.readyCallback_)
    this.readyCallback_();
};

/**
 * Callback that handles Nacl errors.
 * @param {string} msg Error string.
 * @private
 */
NaclBridge.prototype.onNaclError_ = function(msg) {
  // TODO(rkc): Handle error from NaCl better.
  console.error('NaCl error', msg);
};
