// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

Polymer({
  is: 'website-usage-private-api',

  properties: {
    /**
     * The amount of data used by the given website.
     */
    websiteDataUsage: {
      type: String,
      notify: true,
    },

    /**
     * The type of data used by the given website.
     */
    websiteStorageType: {
      type: Number,
      notify: true,
    },
  },

  attached: function() {
    settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance = this;
  },

  /** @param {string} host */
  fetchUsageTotal: function(host) {
    settings.WebsiteUsagePrivateApi.fetchUsageTotal(host);
  },

  /**
   * @param {string} origin
   * @param {number} type
   */
  clearUsage: function(origin, type) {
    settings.WebsiteUsagePrivateApi.clearUsage(origin, type);
  },

  /** @param {string} origin */
  notifyUsageDeleted: function(origin) {
    this.fire('usage-deleted', {origin: origin});
  },
});
})();

cr.define('settings.WebsiteUsagePrivateApi', function() {
  /**
   * @type {Object} An instance of the polymer object defined above.
   * All data will be set here.
   */
  var websiteUsagePolymerInstance = null;

  /**
   * @type {string} The host for which the usage total is being fetched.
   */
  var hostName_;

  /**
   * Encapsulates the calls between JS and C++ to fetch how much storage the
   * host is using.
   * Will update the data in |websiteUsagePolymerInstance|.
   */
  var fetchUsageTotal = function(host) {
    var instance = settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance != null)
      instance.websiteDataUsage = '';

    hostName_ = host;
    chrome.send('fetchUsageTotal', [host]);
  };

  /**
   * Callback for when the usage total is known.
   * @param {string} host The host that the usage was fetched for.
   * @param {string} usage The string showing how much data the given host
   *     is using.
   * @param {number} type The storage type.
   */
  var returnUsageTotal = function(host, usage, type) {
    var instance = settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance == null)
      return;

    if (hostName_ == host) {
      instance.websiteDataUsage = usage;
      instance.websiteStorageType = type;
    }
  };

  /**
   * Deletes the storage being used for a given origin.
   * @param {string} origin The origin to delete storage for.
   * @param {number} type The type of storage to delete.
   */
  var clearUsage = function(origin, type) {
    chrome.send('clearUsage', [origin, type]);
  };

  /**
   * Callback for when the usage has been cleared.
   * @param {string} origin The origin that the usage was fetched for.
   */
  var onUsageCleared = function(origin) {
    var instance = settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance == null)
      return;

    instance.notifyUsageDeleted(origin);
  };

  return { websiteUsagePolymerInstance: websiteUsagePolymerInstance,
           fetchUsageTotal: fetchUsageTotal,
           returnUsageTotal: returnUsageTotal,
           clearUsage: clearUsage,
           onUsageCleared: onUsageCleared, };
});
