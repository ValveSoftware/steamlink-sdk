// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-known-networks' is the settings subpage listing the
 * known networks for a type (currently always WiFi).
 */
Polymer({
  is: 'settings-internet-known-networks-page',

  properties: {
    /**
     * The type of networks to list.
     * @type {chrome.networkingPrivate.NetworkType}
     */
    networkType: {
      type: String,
      observer: 'networkTypeChanged_',
    },

    /**
     * The maximum height in pixels for the list of networks.
     */
    maxHeight: {
      type: Number,
      value: 500
    },

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
    },
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: function() {},

  /** @override */
  attached: function() {
    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    this.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    this.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * Polymer type changed method.
   */
  networkTypeChanged_: function() {
    this.refreshNetworks_();
  },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    this.refreshNetworks_();
  },

  /**
   * Requests the list of network states from Chrome. Updates networkStates
   * once the results are returned from Chrome.
   * @private
   */
  refreshNetworks_: function() {
    if (!this.networkType)
      return;
    var filter = {
      networkType: this.networkType,
      visible: false,
      configured: true
    };
    this.networkingPrivate.getNetworks(
        filter,
        function(states) { this.networkStateList = states; }.bind(this));
  },

  /**
   * Event triggered when a cr-network-list item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onListItemSelected_: function(event) {
    this.fire('show-detail', event.detail);
  },

  /**
   * Event triggered when a cr-network-list item 'remove' button is pressed.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onRemove_: function(event) {
    var state = event.detail;
    if (!state.GUID)
      return;
    this.networkingPrivate.forgetNetwork(state.GUID);
  },

  /**
   * Event triggered when a cr-network-list item 'preferred' button is toggled.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onTogglePreferred_: function(event) {
    var state = event.detail;
    if (!state.GUID)
      return;
    var preferred = state.Priority > 0;
    var onc = {Priority: preferred ? 0 : 1};
    this.networkingPrivate.setProperties(state.GUID, onc);
  },
});
