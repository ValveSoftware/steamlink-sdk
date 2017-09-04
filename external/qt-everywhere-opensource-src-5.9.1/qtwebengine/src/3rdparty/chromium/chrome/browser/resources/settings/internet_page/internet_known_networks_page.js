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
      value: 500,
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * List of all network state data for the network type.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      }
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

  /** @private */
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
    this.networkingPrivate.getNetworks(filter, function(states) {
      this.networkStateList_ = states;
    }.bind(this));
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} state
   * @return {boolean}
   * @private
   */
  networkIsPreferred_: function(state) {
    // Currently we treat NetworkStateProperties.Priority as a boolean.
    return state.Priority > 0;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsNotPreferred_: function(networkState) {
    return networkState.Priority == 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  havePreferred_: function() {
    return this.networkStateList_.find(function(state) {
      return this.networkIsPreferred_(state);
    }.bind(this)) !== undefined;
  },

  /**
   * @return {boolean}
   * @private
   */
  haveNotPreferred_: function() {
    return this.networkStateList_.find(function(state) {
      return this.networkIsNotPreferred_(state);
    }.bind(this)) !== undefined;
  },

  /**
   * @param {!{model: !{item: !CrOnc.NetworkStateProperties}}} e
   * @private
   */
  onRemoveTap_: function(e) {
    var state = e.model.item;
    this.networkingPrivate.setProperties(state.GUID, {Priority: 0});
  },

  /**
   * @param {!{model: !{item: !CrOnc.NetworkStateProperties}}} e
   * @private
   */
  onAddTap_: function(e) {
    var state = e.model.item;
    this.networkingPrivate.setProperties(state.GUID, {Priority: 1});
  },
});
