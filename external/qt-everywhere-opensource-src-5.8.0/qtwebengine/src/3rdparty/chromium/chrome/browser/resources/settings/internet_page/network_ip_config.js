// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state. TODO(stevenjb): Allow editing of static IP configurations
 * when 'editable' is true.
 */
(function() {
'use strict';

Polymer({
  is: 'network-ip-config',

  properties: {
    /**
     * The network properties dictionary containing the IP Config properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_'
    },

    /**
     * Whether or not the IP Address can be edited.
     * TODO(stevenjb): Implement editing.
     */
    editable: {
      type: Boolean,
      value: false
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     */
    automatic: {
      type: Boolean,
      value: false,
      observer: 'automaticChanged_'
    },

    /**
     * The currently visible IP Config property dictionary. The 'RoutingPrefix'
     * property is a human-readable mask instead of a prefix length.
     * @type {!{
     *   ipv4: !CrOnc.IPConfigUIProperties,
     *   ipv6: !CrOnc.IPConfigUIProperties
     * }|undefined}
     */
    ipConfig: {
      type: Object
    },

    /**
     * Array of properties to pass to the property list.
     * @type {!Array<string>}
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.IPAddress',
          'ipv4.RoutingPrefix',
          'ipv4.Gateway',
          'ipv6.IPAddress'
        ];
      },
      readOnly: true
    },
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @type {!CrOnc.IPConfigUIProperties|undefined}
   */
  savedStaticIp_: undefined,

  /**
   * Polymer networkProperties changed method.
   */
  networkPropertiesChanged_: function(newValue, oldValue) {
    if (!this.networkProperties)
      return;

    if (newValue.GUID != (oldValue && oldValue.GUID))
      this.savedStaticIp_ = undefined;

    // Update the 'automatic' property.
    var ipConfigType =
        CrOnc.getActiveValue(this.networkProperties.IPAddressConfigType);
    this.automatic = (ipConfigType != CrOnc.IPConfigType.STATIC);

    // Update the 'ipConfig' property.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    var ipv6 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV6);
    this.ipConfig = {
      ipv4: this.getIPConfigUIProperties_(ipv4),
      ipv6: this.getIPConfigUIProperties_(ipv6)
    };
  },

  /**
   * Polymer automatic changed method.
   */
  automaticChanged_: function() {
    if (!this.automatic || !this.ipConfig)
      return;
    if (this.automatic || !this.savedStaticIp_) {
      // Save the static IP configuration when switching to automatic.
      this.savedStaticIp_ = this.ipConfig.ipv4;
      var configType =
          this.automatic ? CrOnc.IPConfigType.DHCP : CrOnc.IPConfigType.STATIC;
      this.fire('ip-change', {
        field: 'IPAddressConfigType',
        value: configType
      });
    } else {
      // Restore the saved static IP configuration.
      var ipconfig = {
        Gateway: this.savedStaticIp_.Gateway,
        IPAddress: this.savedStaticIp_.IPAddress,
        RoutingPrefix: this.savedStaticIp_.RoutingPrefix,
        Type: this.savedStaticIp_.Type
      };
      this.fire('ip-change', {
        field: 'StaticIPConfig',
        value: this.getIPConfigProperties_(ipconfig)
      });
    }
  },

  /**
   * @param {!CrOnc.IPConfigProperties|undefined} ipconfig
   * @return {!CrOnc.IPConfigUIProperties} A new IPConfigUIProperties object
   *     with RoutingPrefix expressed as a string mask instead of a prefix
   *     length. Returns an empty object if |ipconfig| is undefined.
   * @private
   */
  getIPConfigUIProperties_: function(ipconfig) {
    var result = {};
    if (!ipconfig)
      return result;
    for (let key in ipconfig) {
      let value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsNetmask(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @param {!CrOnc.IPConfigUIProperties} ipconfig The IP Config UI properties.
   * @return {!CrOnc.IPConfigProperties} A new IPConfigProperties object with
   *     RoutingPrefix expressed as a a prefix length.
   * @private
   */
  getIPConfigProperties_: function(ipconfig) {
    var result = {};
    for (let key in ipconfig) {
      let value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsLength(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @param {!CrOnc.IPConfigUIProperties} ipConfig The IP Config UI properties.
   * @param {boolean} editable The editable property.
   * @param {boolean} automatic The automatic property.
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_: function(ipConfig, editable, automatic) {
    if (!editable || automatic)
      return {};
    return {
      'ipv4.IPAddress': 'String',
      'ipv4.RoutingPrefix': 'String',
      'ipv4.Gateway': 'String'
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!{detail: { field: string, value: string}}} event The
   *     network-property-list change event.
   * @private
   */
  onIPChange_: function(event) {
    if (!this.ipConfig)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig.' + field, value);
    this.fire('ip-change', {
      field: 'StaticIPConfig',
      value: this.getIPConfigProperties_(this.ipConfig.ipv4)
    });
  },
});
})();
