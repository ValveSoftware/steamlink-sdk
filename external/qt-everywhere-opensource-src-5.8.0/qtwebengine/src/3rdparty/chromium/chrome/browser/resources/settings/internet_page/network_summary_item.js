// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type.
 */

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
var DeviceStateProperties;

Polymer({
  is: 'network-summary-item',

  properties: {
    /**
     * True if the list is expanded.
     */
    expanded: {
      type: Boolean,
      value: false,
      observer: 'expandedChanged_'
    },

    /**
     * The maximum height in pixels for the list of networks.
     */
    maxHeight: {
      type: Number,
      value: 200
    },

    /**
     * True if this item should be hidden. We need this computed property so
     * that it can default to true, hiding this element, since no changed event
     * will be fired for deviceState if it is undefined (in NetworkSummary).
     */
    isHidden: {
      type: Boolean,
      value: true,
      computed: 'noDeviceState_(deviceState)'
    },

    /**
     * Device state for the network type.
     * @type {DeviceStateProperties|undefined}
     */
    deviceState: {
      type: Object,
      observer: 'deviceStateChanged_'
    },

    /**
     * Network state for the active network.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
    },

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() { return []; },
      observer: 'networkStateListChanged_'
    }
  },

  /**
   * Polymer expanded changed method.
   */
  expandedChanged_: function() {
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('expanded', {expanded: this.expanded, type: type});
  },

  /**
   * Polymer deviceState changed method.
   */
  deviceStateChanged_: function() {
    this.updateSelectable_();
    if (this.expanded && !this.deviceIsEnabled_(this.deviceState))
      this.expanded = false;
  },

  /**
   * Polymer networkStateList changed method.
   */
  networkStateListChanged_: function() {
    this.updateSelectable_();
  },

  /**
   * @param {DeviceStateProperties} deviceState
   * @return {boolean} True if the device state is not set.
   * @private
   */
  noDeviceState_: function(deviceState) {
    return !deviceState;
  },

  /**
   * @param {DeviceStateProperties} deviceState
   * @param {boolean} expanded The expanded state.
   * @return {boolean} Whether or not the scanning spinner should be shown.
   * @private
   */
  showScanning_: function(deviceState, expanded) {
    return !!expanded && !!deviceState.Scanning;
  },

  /**
   * @param {DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState && deviceState.State == 'Enabled';
  },

  /**
   * @param {DeviceStateProperties} deviceState
   * @return {string} The class value for the device enabled button.
   * @private
   */
  getDeviceEnabledButtonClass_: function(deviceState) {
    var visible = deviceState && deviceState.Type != CrOnc.Type.ETHERNET &&
                  deviceState.Type != CrOnc.Type.VPN;
    return visible ? '' : 'invisible';
  },

  /**
   * @param {DeviceStateProperties} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkList
   * @return {string} The class value for the expand button.
   * @private
   */
  getExpandButtonClass_: function(deviceState, networkList) {
    var visible = this.expandIsVisible_(deviceState, networkList);
    return visible ? '' : 'invisible';
  },

  /**
   * @param {DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkList
   * @return {boolean} Whether or not to show the UI to expand the list.
   * @private
   */
  expandIsVisible_: function(deviceState, networkList) {
    if (!this.deviceIsEnabled_(deviceState))
      return false;
    var minLength = (this.deviceState.Type == CrOnc.Type.WI_FI) ? 1 : 2;
    return networkList.length >= minLength;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} state
   * @param {boolean} expanded The expanded state.
   * @return {boolean} True if the 'Known networks' button should be shown.
   * @private
   */
  showKnownNetworks_: function(state, expanded) {
    return !!expanded && !!state && state.Type == CrOnc.Type.WI_FI;
  },

  /**
   * Event triggered when the details div is tapped.
   * @param {Event} event The enable button event.
   * @private
   */
  onDetailsTap_: function(event) {
    if ((event.target.id == 'expandListButton') ||
        (this.deviceState && !this.deviceIsEnabled_(this.deviceState))) {
      // Already handled or disabled, do nothing.
      return;
    }
    if (this.expandIsVisible_(this.deviceState, this.networkStateList)) {
      // Expandable, toggle expand.
      this.expanded = !this.expanded;
      return;
    }
    // Not expandable, fire 'selected' with |networkState|.
    this.fire('selected', this.networkState);
  },

  /**
   * Event triggered when the known networks button is tapped.
   * @private
   */
  onKnownNetworksTap_: function() {
    this.fire('show-known-networks', {type: CrOnc.Type.WI_FI});
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledTap_: function(event) {
    var deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('device-enabled-toggled',
              {enabled: !deviceIsEnabled, type: type});
    // Make sure this does not propagate to onDetailsTap_.
    event.stopPropagation();
  },

  /**
   * Called whenever the 'selectable' state might change.
   * @private
   */
  updateSelectable_: function() {
    var selectable = this.deviceIsEnabled_(this.deviceState);
    this.$.details.classList.toggle('selectable', selectable);
  },
});
