// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, WiMAX, and VPN.
 */

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
var DeviceStateProperties;

/**
 * @typedef {{
 *   Ethernet: (DeviceStateProperties|undefined),
 *   WiFi: (DeviceStateProperties|undefined),
 *   Cellular: (DeviceStateProperties|undefined),
 *   WiMAX: (DeviceStateProperties|undefined),
 *   VPN: (DeviceStateProperties|undefined)
 * }}
 */
var DeviceStateObject;

/**
 * @typedef {{
 *   Ethernet: (!CrOnc.NetworkStateProperties|undefined),
 *   WiFi: (!CrOnc.NetworkStateProperties|undefined),
 *   Cellular: (!CrOnc.NetworkStateProperties|undefined),
 *   WiMAX: (!CrOnc.NetworkStateProperties|undefined),
 *   VPN: (!CrOnc.NetworkStateProperties|undefined)
 * }}
 */
var NetworkStateObject;

/**
 * @typedef {{
 *   Ethernet: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   WiFi: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   Cellular: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   WiMAX: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   VPN: (Array<CrOnc.NetworkStateProperties>|undefined)
 * }}
 */
var NetworkStateListObject;

(function() {

/** @const {!Array<chrome.networkingPrivate.NetworkType>} */
var NETWORK_TYPES = [
  CrOnc.Type.ETHERNET,
  CrOnc.Type.WI_FI,
  CrOnc.Type.CELLULAR,
  CrOnc.Type.WI_MAX,
  CrOnc.Type.VPN
];

Polymer({
  is: 'network-summary',

  properties: {
    /**
     * Highest priority connected network or null.
     * @type {?CrOnc.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
      notify: true
    },

    /**
     * The device state for each network device type.
     * @type {DeviceStateObject}
     */
    deviceStates: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * Network state data for each network type.
     * @type {NetworkStateObject}
     */
    networkStates: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * List of network state data for each network type.
     * @type {NetworkStateListObject}
     */
    networkStateLists: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
    }
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

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: function() {},

  /**
   * Dictionary of GUIDs identifying primary (active) networks for each type.
   * @type {?Object}
   * @private
   */
  networkIds_: null,

  /** @override */
  attached: function() {
    this.networkIds_ = {};

    this.getNetworkLists_();

    this.networkListChangedListener_ =
        this.onNetworkListChangedEvent_.bind(this);
    this.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ =
        this.onDeviceStateListChangedEvent_.bind(this);
    this.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    this.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    this.networkingPrivate.onNetworkListChanged.removeListener(
        this.networkListChangedListener_);

    this.networkingPrivate.onDeviceStateListChanged.removeListener(
        this.deviceStateListChangedListener_);

    this.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * Event triggered when the WiFi network-summary-item is expanded.
   * @param {!{detail: {expanded: boolean, type: string}}} event
   * @private
   */
  onWiFiExpanded_: function(event) {
    if (!event.detail.expanded)
      return;
    // Get the latest network states (only).
    this.getNetworkStates_();
    this.networkingPrivate.requestNetworkScan();
  },

  /**
   * Event triggered when a network-summary-item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onSelected_: function(event) {
    var state = event.detail;
    if (this.canConnect_(state)) {
      this.connectToNetwork_(state);
      return;
    }
    this.fire('show-detail', state);
  },

  /**
   * Event triggered when the enabled state of a network-summary-item is
   * toggled.
   * @param {!{detail: {enabled: boolean,
   *                    type: chrome.networkingPrivate.NetworkType}}} event
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    if (event.detail.enabled)
      this.networkingPrivate.enableNetworkType(event.detail.type);
    else
      this.networkingPrivate.disableNetworkType(event.detail.type);
  },

  /**
   * networkingPrivate.onNetworkListChanged event callback.
   * @private
   */
  onNetworkListChangedEvent_: function() { this.getNetworkLists_(); },

  /**
   * networkingPrivate.onDeviceStateListChanged event callback.
   * @private
   */
  onDeviceStateListChangedEvent_: function() { this.getNetworkLists_(); },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    networkIds.forEach(function(id) {
      if (id in this.networkIds_) {
        this.networkingPrivate.getState(
            id, this.getStateCallback_.bind(this, id));
      }
    }, this);
  },

  /**
   * Determines whether or not a network state can be connected to.
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @private
   */
  canConnect_: function(state) {
    if (state.Type == CrOnc.Type.ETHERNET ||
        state.Type == CrOnc.Type.VPN && !this.defaultNetwork) {
      return false;
    }
    return state.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * networkingPrivate.getState event callback.
   * @param {string} id The id of the requested state.
   * @param {!chrome.networkingPrivate.NetworkStateProperties} state
   * @private
   */
  getStateCallback_: function(id, state) {
    if (chrome.runtime.lastError) {
      var message = chrome.runtime.lastError.message;
      if (message != 'Error.NetworkUnavailable') {
        console.error(
            'Unexpected networkingPrivate.getState error: ' + message +
            ' For: ' + id);
      }
      return;
    }
    // Async call, ensure id still exists.
    if (!this.networkIds_[id])
      return;
    if (!state) {
      this.networkIds_[id] = undefined;
      return;
    }
    this.updateNetworkState_(state.Type, state);
  },

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @private
   */
  connectToNetwork_: function(state) {
    this.networkingPrivate.startConnect(state.GUID, function() {
      if (chrome.runtime.lastError) {
        var message = chrome.runtime.lastError.message;
        if (message != 'connecting') {
          console.error(
              'Unexpected networkingPrivate.startConnect error: ' + message +
              'For: ' + state.GUID);
        }
      }
    });
  },

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, networkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_: function() {
    // First get the device states.
    this.networkingPrivate.getDeviceStates(
      function(deviceStates) {
        // Second get the network states.
        this.getNetworkStates_(deviceStates);
      }.bind(this));
  },

  /**
   * Requests the list of network states from Chrome. Updates networkStates and
   * networkStateLists once the results are returned from Chrome.
   * @param {!Array<!DeviceStateProperties>=} opt_deviceStates
   *     Optional list of state properties for all available devices.
   * @private
   */
  getNetworkStates_: function(opt_deviceStates) {
    var filter = {
      networkType: chrome.networkingPrivate.NetworkType.ALL,
      visible: true,
      configured: false
    };
    this.networkingPrivate.getNetworks(filter, function(networkStates) {
      this.updateNetworkStates_(networkStates, opt_deviceStates);
    }.bind(this));
  },

  /**
   * Called after network states are received from getNetworks.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates The state
   *     properties for all visible networks.
   * @param {!Array<!DeviceStateProperties>=} opt_deviceStates
   *     Optional list of state properties for all available devices. If not
   *     defined the existing list of device states will be used.
   * @private
   */
  updateNetworkStates_: function(networkStates, opt_deviceStates) {
    var newDeviceStates;
    if (opt_deviceStates) {
      newDeviceStates = /** @type {!DeviceStateObject} */({});
      opt_deviceStates.forEach(function(state) {
        newDeviceStates[state.Type] = state;
      });
    } else {
      newDeviceStates = this.deviceStates;
    }

    // Clear any current networks.
    this.networkIds_ = {};

    // Track the first (active) state for each type.
    var foundTypes = {};

    // Complete list of states by type.
    /** @type {!NetworkStateListObject} */ var networkStateLists = {
      Ethernet: [],
      WiFi: [],
      Cellular: [],
      WiMAX: [],
      VPN: []
    };

    var firstConnectedNetwork = null;
    networkStates.forEach(function(state) {
      var type = state.Type;
      if (!foundTypes[type]) {
        foundTypes[type] = true;
        this.updateNetworkState_(type, state);
        if (!firstConnectedNetwork && state.Type != CrOnc.Type.VPN &&
            state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
          firstConnectedNetwork = state;
        }
      }
      networkStateLists[type].push(state);
    }, this);

    this.defaultNetwork = firstConnectedNetwork;

    // Set any types with a deviceState and no network to a default state,
    // and any types not found to undefined.
    NETWORK_TYPES.forEach(function(type) {
      if (!foundTypes[type]) {
        var defaultState = undefined;
        if (newDeviceStates[type])
          defaultState = {GUID: '', Type: type};
        this.updateNetworkState_(type, defaultState);
      }
    }, this);

    this.networkStateLists = networkStateLists;

    // Create a VPN entry in deviceStates if there are any VPN networks.
    if (networkStateLists.VPN && networkStateLists.VPN.length > 0) {
      newDeviceStates.VPN = /** @type {DeviceStateProperties} */ ({
        Type: CrOnc.Type.VPN,
        State: chrome.networkingPrivate.DeviceStateType.ENABLED
      });
    }

    this.deviceStates = newDeviceStates;
  },

  /**
   * Sets 'networkStates[type]' which will update the
   * cr-network-list-network-item associated with 'type'.
   * @param {string} type The network type.
   * @param {!CrOnc.NetworkStateProperties|undefined} state The state properties
   *     for the network to associate with |type|. May be undefined if there are
   *     no networks matching |type|.
   * @private
   */
  updateNetworkState_: function(type, state) {
    this.set('networkStates.' + type, state);
    if (state)
      this.networkIds_[state.GUID] = true;
  },
});
})();
