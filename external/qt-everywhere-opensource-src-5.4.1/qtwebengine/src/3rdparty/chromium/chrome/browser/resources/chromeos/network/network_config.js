// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This object provides a similar API to chrome.networkingPrivate.
 * It simulates the extension callback model by storing callbacks in a member
 * object and invoking them when the corresponding method is called by Chrome in
 * response to a chrome.send call.
 */

var networkConfig = {
  /**
   * Return the property associated with a key (which may reference a
   * sub-object).
   *
   * @param {Object} properties The object containing the network properties.
   * @param {string} key The ONC key for the property. May refer to a nested
   *     propety, e.g. 'WiFi.Security'.
   * @return {*} The value associated with the property.
   */
  getValueFromProperties: function(properties, key) {
    if (!key) {
      console.error('Empty key');
      return undefined;
    }
    var dot = key.indexOf('.');
    if (dot > 0) {
      var key1 = key.substring(0, dot);
      var key2 = key.substring(dot + 1);
      var subobject = properties[key1];
      if (subobject)
        return this.getValueFromProperties(subobject, key2);
    }
    return properties[key];
  },

  /**
   * Generate a unique id for 'callback' and store it for future retrieval.
   *
   * @param {function} callback The associated callback.
   * @return {integer} The id of the callback.
   * @private
   */
  callbackId: 1,
  callbackMap: {},
  storeCallback_: function(callback) {
    var id = this.callbackId++;
    this.callbackMap[id] = callback;
    return id;
  },

  /**
   * Retrieve the callback associated with |id| and remove it from the map.
   *
   * @param {integer} id The id of the callback.
   * @return {function} The associated callback.
   * @private
   */
  retrieveCallback_: function(id) {
    var callback = this.callbackMap[id];
    delete this.callbackMap[id];
    return callback;
  },

  /**
   * Callback invoked by Chrome.
   *
   * @param {Array} args A list of arguments passed to the callback. The first
   *   entry must be the callbackId passed to chrome.send.
   */
  chromeCallbackSuccess: function(args) {
    var callbackId = args.shift();
    var callback = this.retrieveCallback_(callbackId);
    this.lastError = '';
    if (callback)
      callback.apply(null, args);
    else
      console.error('Callback not found for id: ' + callbackId);
  },

  /**
   * Error Callback invoked by Chrome. Sets lastError and logs to the console.
   *
   * @param {Args} args A list of arguments. The first entry must be the
   *   callbackId passed to chrome.send.
   */
  lastError: '',
  chromeCallbackError: function(args) {
    var callbackId = args.shift();
    this.lastError = args.shift();
    console.error('Callback error: "' + this.lastError);
    // We still invoke the callback, but with null args. The callback should
    // check this.lastError and handle that.
    var callback = this.retrieveCallback_(callbackId);
    if (callback)
      callback.apply(null, null);
  },

  /**
   * Implement networkingPrivate.getProperties. See networking_private.json.
   *
   * @param {string} guid The guid identifying the network.
   * @param {function()} callback The callback to call on completion.
   */
  getProperties: function(guid, callback) {
    var callbackId = this.storeCallback_(callback);
    chrome.send('networkConfig.getProperties', [callbackId, guid]);
  },

  /**
   * Implement networkingPrivate.getNetworks. See networking_private.json.
   *
   * @param {string} guid The guid identifying the network.
   * @param {function()} callback The callback to call on completion.
   */
  getNetworks: function(filter, callback) {
    var callbackId = this.storeCallback_(callback);
    chrome.send('networkConfig.getNetworks', [callbackId, filter]);
  }
};
