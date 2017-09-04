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

  behaviors: [Polymer.IronA11yKeysBehavior],

  properties: {
    /**
     * Device state for the network type.
     * @type {DeviceStateProperties|undefined}
     */
    deviceState: {
      type: Object,
      observer: 'deviceStateChanged_',
    },

    /**
     * Network state for the active network.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    activeNetworkState: Object,

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** The maximum height in pixels for the list of networks. */
    maxHeight: {
      type: Number,
      value: 200,
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * The expanded state of the list of networks.
     * @private
     */
    expanded_: {
      type: Boolean,
      value: false,
      observer: 'expandedChanged_',
    },

    /**
     * Whether the list has been expanded. This is used to ensure the
     * iron-collapse section animates correctly.
     * @private
     */
    wasExpanded_: {
      type: Boolean,
      value: false,
    },
  },

  keyBindings: {
    'enter': 'onDetailsTap_',
  },

  /** @private */
  expandedChanged_: function() {
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('expanded', {expanded: this.expanded_, type: type});
  },

  /** @private */
  deviceStateChanged_: function() {
    if (this.expanded_ && !this.deviceIsEnabled_())
      this.expanded_ = false;
  },

  /**
   * @return {boolean} Whether or not the scanning spinner should be visible.
   * @private
   */
  scanningIsVisible_: function() {
    return this.deviceState.Type == CrOnc.Type.WI_FI;
  },

  /**
   * @return {boolean} Whether or not the scanning spinner should be active.
   * @private
   */
  scanningIsActive_: function() {
    return !!this.expanded_ && !!this.deviceState.Scanning;
  },

  /**
   * Show the <network-siminfo> element if this is a disabled and locked
   * cellular device.
   * @return {boolean}
   * @private
   */
  showSimInfo_: function() {
    let device = this.deviceState;
    if (device.Type != CrOnc.Type.CELLULAR || this.deviceIsEnabled_())
      return false;
    return device.SimPresent === false ||
        device.SimLockType == CrOnc.LockType.PIN ||
        device.SimLockType == CrOnc.LockType.PUK;
  },

  /**
   * Returns a NetworkProperties object for <network-siminfo> built from
   * the device properties (since there will be no active network).
   * @return {!CrOnc.NetworkProperties}
   * @private
   */
  getCellularState_: function() {
    let device = this.deviceState;
    return {
      GUID: '',
      Type: CrOnc.Type.CELLULAR,
      Cellular: {
        SIMLockStatus: {
          LockType: device.SimLockType || '',
          LockEnabled: device.SimLockType != CrOnc.LockType.NONE,
        },
        SIMPresent: device.SimPresent,
      },
    };
  },

  /**
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function() {
    return !!this.deviceState &&
        this.deviceState.State ==
        chrome.networkingPrivate.DeviceStateType.ENABLED;
  },

  /**
   * @return {boolean} Whether the dom-if for the network list should be true.
   *   The logic here is designed to allow the enclosed content to be stamped
   *   before it is expanded.
   * @private
   */
  networksDomIfIsTrue_() {
    if (this.expanded_ == this.wasExpanded_)
      return this.expanded_;
    if (this.expanded_) {
      Polymer.RenderStatus.afterNextRender(this, function() {
        this.wasExpanded_ = true;
      }.bind(this));
      return true;
    }
    return this.wasExpanded_;
  },

  /**
   * @return {boolean} Whether the iron-collapse for the network list should
   *   be opened.
   * @private
   */
  networksIronCollapseIsOpened_() {
    return this.expanded_ && this.wasExpanded_;
  },

  /**
   * @return {boolean}
   * @private
   */
  enableIsVisible_: function() {
    return this.deviceState.Type != CrOnc.Type.ETHERNET &&
        this.deviceState.Type != CrOnc.Type.VPN;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function() {
    return this.deviceState.Type == CrOnc.Type.CELLULAR &&
        this.networkStateList.length == 1;
  },

  /**
   * @return {boolean} Whether or not to show the UI to expand the list.
   * @private
   */
  expandIsVisible_: function() {
    if (!this.deviceIsEnabled_())
      return false;
    let type = this.deviceState.Type;
    var minLength =
        (type == CrOnc.Type.WI_FI || type == CrOnc.Type.VPN) ? 1 : 2;
    return this.networkStateList.length >= minLength;
  },

  /**
   * @return {boolean} True if the known networks button should be shown.
   * @private
   */
  knownNetworksIsVisible_: function() {
    return !!this.activeNetworkState &&
        this.activeNetworkState.Type == CrOnc.Type.WI_FI;
  },

  /**
   * Event triggered when the details div is tapped or Enter is pressed.
   * @param {!Event} event The enable button event.
   * @private
   */
  onDetailsTap_: function(event) {
    if ((event.target && event.target.id == 'expandListButton') ||
        (this.deviceState && !this.deviceIsEnabled_())) {
      // Already handled or disabled, do nothing.
      return;
    }
    if (this.expandIsVisible_()) {
      // Expandable, toggle expand.
      this.expanded_ = !this.expanded_;
      return;
    }
    // Not expandable, fire 'selected' with |activeNetworkState|.
    this.fire('selected', this.activeNetworkState);
  },

  /**
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_: function(event) {
    if (!this.activeNetworkState.GUID)
      return;
    this.fire('show-detail', this.activeNetworkState);
    event.stopPropagation();
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
    var deviceIsEnabled = this.deviceIsEnabled_();
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire(
        'device-enabled-toggled', {enabled: !deviceIsEnabled, type: type});
    // Make sure this does not propagate to onDetailsTap_.
    event.stopPropagation();
  },

  /**
   * @return {string}
   * @private
   */
  getTabIndex_: function() {
    return this.deviceIsEnabled_() ? '0' : '-1';
  },
});
