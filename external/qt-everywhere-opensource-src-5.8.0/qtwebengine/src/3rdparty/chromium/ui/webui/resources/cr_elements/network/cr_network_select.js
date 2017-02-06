// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping cr-network-list including the
 * networkingPrivate calls to populate it.
 */

Polymer({
  is: 'cr-network-select',

  properties: {
    /**
     * Network state for the active network.
     * @type {?CrOnc.NetworkStateProperties}
     */
    activeNetworkState: {
      type: Object,
      value: null
    },

    /**
     * If true, the element includes an 'expand' button that toggles the
     * expanded state of the network list.
     */
    expandable: {
      type: Boolean,
      value: false
    },

    /**
     * The maximum height in pixels for the list.
     */
    maxHeight: {
      type: Number,
      value: 1000
    },

    /**
     * If true, expand the network list.
     */
    networkListOpened: {
      type: Boolean,
      value: true,
      observer: "networkListOpenedChanged_"
    },

    /**
     * If true, show the active network state.
     */
    showActive: {
      type: Boolean,
      value: false
    },

    /**
     * List of all network state data for all visible networks.
     * See <cr-network-list-network-item/> for details.
     *
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * List of custom items to display at the end of networks list.
     * See <cr-network-list-custom-item/> for details.
     *
     * @type {!Array<Object>}
     */
    customItems: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Show all buttons in list items.
     */
    showButtons: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to handle "item-selected" for network items.
     * If this property is false, "network-item-selected" event is fired
     * carrying CrOnc.NetworkStateProperties as event detail.
     *
     * @type {Function}
     */
    handleNetworkItemSelected: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networkListChangedListener_: function() {},

  /**
   * Listener function for chrome.networkingPrivate.onDeviceStateListChanged
   * event.
   * @type {function(!Array<string>)}
   * @private
   */
  deviceStateListChangedListener_: function() {},

  /** @override */
  attached: function() {
    this.networkListChangedListener_ = this.refreshNetworks_.bind(this);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ = this.refreshNetworks_.bind(this);
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.refreshNetworks_();
    chrome.networkingPrivate.requestNetworkScan();
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworkListChanged.removeListener(
        this.networkListChangedListener_);
    chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
        this.deviceStateListChangedListener_);
  },

  /**
   * Polymer changed function.
   * @private
   */
  networkListOpenedChanged_: function() {
    if (this.networkListOpened)
      chrome.networkingPrivate.requestNetworkScan();
  },

  /**
   * Request the list of visible networks.
   * @private
   */
  refreshNetworks_: function() {
    var filter = {
      networkType: chrome.networkingPrivate.NetworkType.ALL,
      visible: true,
      configured: false
    };
    chrome.networkingPrivate.getNetworks(
        filter, this.getNetworksCallback_.bind(this));
  },

  /**
   * @param {!Array<!CrOnc.NetworkStateProperties>} states
   * @private
   */
  getNetworksCallback_: function(states) {
    this.activeNetworkState = states[0] || null;
    this.networkStateList = states;
  },

  /**
   * Event triggered when a cr-network-list-network-item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkListItemSelected_: function(event) {
    var state = event.detail;

    if (!this.handleNetworkItemSelected) {
      this.fire("network-item-selected", state);
      return;
    }

    if (state.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED)
      return;

    chrome.networkingPrivate.startConnect(state.GUID, function() {
      var lastError = chrome.runtime.lastError;
      if (lastError && lastError != 'connecting')
        console.error('networkingPrivate.startConnect error: ' + lastError);
    });
  },
});
