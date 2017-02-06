// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-detail' is the settings subpage containing details
 * for a network.
 */
(function() {
'use strict';

/** @const */ var CARRIER_VERIZON = 'Verizon Wireless';

Polymer({
  is: 'settings-internet-detail-page',

  behaviors: [CrPolicyNetworkBehavior],

  properties: {
    /**
     * The network GUID to display details for.
     */
    guid: {
      type: String,
      value: '',
    },

    /**
     * The current properties for the network matching |guid|.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_'
    },

    /**
     * The network AutoConnect state.
     */
    autoConnect: {
      type: Boolean,
      value: false,
      observer: 'autoConnectChanged_'
    },

    /**
     * The network preferred state.
     */
    preferNetwork: {
      type: Boolean,
      value: false,
      observer: 'preferNetworkChanged_'
    },

    /**
     * The network IP Address.
     */
    IPAddress: {
      type: String,
      value: ''
    },

    /**
     * Highest priority connected network or null.
     * @type {?CrOnc.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null
    },

    /**
     * Object providing network type values for data binding.
     * @const
     */
    NetworkType: {
      type: Object,
      value: {
        CELLULAR: CrOnc.Type.CELLULAR,
        ETHERNET: CrOnc.Type.ETHERNET,
        VPN: CrOnc.Type.VPN,
        WIFI: CrOnc.Type.WI_FI,
        WIMAX: CrOnc.Type.WI_MAX,
      },
      readOnly: true
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
    },
  },

  observers: [
    'guidChanged_(guid, networkingPrivate)',
  ],

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
   * Polymer guid changed method.
   */
  guidChanged_: function() {
    if (!this.guid)
      return;
    this.getNetworkDetails_();
  },

  /**
   * Polymer networkProperties changed method.
   */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties)
      return;

    // Update autoConnect if it has changed. Default value is false.
    var autoConnect = CrOnc.getAutoConnect(this.networkProperties);
    if (autoConnect != this.autoConnect)
      this.autoConnect = autoConnect;

    // Update preferNetwork if it has changed. Default value is false.
    var priority = /** @type {number} */(CrOnc.getActiveValue(
        this.networkProperties.Priority) || 0);
    var preferNetwork = priority > 0;
    if (preferNetwork != this.preferNetwork)
      this.preferNetwork = preferNetwork;

    // Set the IPAddress property to the IPV4 Address.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.IPAddress = (ipv4 && ipv4.IPAddress) || '';
  },

  /**
   * Polymer autoConnect changed method.
   */
  autoConnectChanged_: function() {
    if (!this.networkProperties || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setTypeProperty(onc, 'AutoConnect', this.autoConnect);
    this.setNetworkProperties_(onc);
  },

  /**
   * Polymer preferNetwork changed method.
   */
  preferNetworkChanged_: function() {
    if (!this.networkProperties || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    onc.Priority = this.preferNetwork ? 1 : 0;
    this.setNetworkProperties_(onc);
  },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    if (networkIds.indexOf(this.guid) != -1)
      this.getNetworkDetails_();
  },

  /**
   * Calls networkingPrivate.getProperties for this.guid.
   * @private
   */
  getNetworkDetails_: function() {
    if (!this.guid)
      return;
    this.networkingPrivate.getManagedProperties(
        this.guid, this.getPropertiesCallback_.bind(this));
  },

  /**
   * networkingPrivate.getProperties callback.
   * @param {CrOnc.NetworkProperties} properties The network properties.
   * @private
   */
  getPropertiesCallback_: function(properties) {
    this.networkProperties = properties;
    if (!properties) {
      // If |properties| becomes null (i.e. the network is no longer visible),
      // close the page.
      this.fire('close');
    }
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    if (!this.guid)
      return;
    this.networkingPrivate.setProperties(this.guid, onc, function() {
      if (chrome.runtime.lastError) {
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    }.bind(this));
  },

  /**
   * @return {!chrome.networkingPrivate.NetworkConfigProperties} An ONC
   *     dictionary with just the Type property set. Used for passing properties
   *     to setNetworkProperties_.
   * @private
   */
  getEmptyNetworkProperties_: function() {
    return {Type: this.networkProperties.Type};
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {string} The text to display for the network name.
   * @private
   */
  getStateName_: function(properties) {
    return /** @type {string} */(CrOnc.getActiveValue(
      this.networkProperties.Name) || '');
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(properties) {
    // TODO(stevenjb): Localize.
    return (properties && properties.ConnectionState) || '';
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(properties) {
    return properties.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} Whether or not to show the 'Connect' button.
   * @private
   */
  showConnect_: function(properties) {
    return properties.Type != CrOnc.Type.ETHERNET &&
           properties.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} Whether or not to show the 'Activate' button.
   * @private
   */
  showActivate_: function(properties) {
    if (!properties || properties.Type != CrOnc.Type.CELLULAR)
      return false;
    var activation = properties.Cellular.ActivationState;
    return activation == CrOnc.ActivationState.NOT_ACTIVATED ||
           activation == CrOnc.ActivationState.PARTIALLY_ACTIVATED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} Whether or not to show the 'View Account' button.
   * @private
   */
  showViewAccount_: function(properties) {
    // Show either the 'Activate' or the 'View Account' button.
    if (this.showActivate_(properties))
      return false;

    if (!properties || properties.Type != CrOnc.Type.CELLULAR ||
        !properties.Cellular) {
      return false;
    }

    // Only show if online payment URL is provided or the carrier is Verizon.
    var carrier = CrOnc.getActiveValue(properties.Cellular.Carrier);
    if (carrier != CARRIER_VERIZON) {
      var paymentPortal = properties.Cellular.PaymentPortal;
      if (!paymentPortal || !paymentPortal.Url)
        return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(properties)) {
      var technology = properties.Cellular.NetworkTechnology;
      if (technology != CrOnc.NetworkTechnology.LTE &&
          technology != CrOnc.NetworkTechnology.LTE_ADVANCED) {
        return false;
      }
      if (!properties.Cellular.MDN)
        return false;
    }

    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @param {?CrOnc.NetworkStateProperties} defaultNetwork
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(properties, defaultNetwork) {
    if (!properties || !this.showConnect_(properties))
      return false;
    if (properties.Type == CrOnc.Type.CELLULAR && CrOnc.isSimLocked(properties))
      return false;
    if (properties.Type == CrOnc.Type.VPN && !defaultNetwork)
      return false;
    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} Whether or not to show the 'Disconnect' button.
   * @private
   */
  showDisconnect_: function(properties) {
    return properties.Type != CrOnc.Type.ETHERNET &&
           properties.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * Callback when the Connect button is tapped.
   * @private
   */
  onConnectTap_: function() {
    this.networkingPrivate.startConnect(this.guid);
  },

  /**
   * Callback when the Disconnect button is tapped.
   * @private
   */
  onDisconnectTap_: function() {
    this.networkingPrivate.startDisconnect(this.guid);
  },

  /**
   * Callback when the Activate button is tapped.
   * @private
   */
  onActivateTap_: function() {
    this.networkingPrivate.startActivate(this.guid);
  },

  /**
   * Callback when the View Account button is tapped.
   * @private
   */
  onViewAccountTap_: function() {
    // startActivate() will show the account page for activated networks.
    this.networkingPrivate.startActivate(this.guid);
  },

  /**
   * Event triggered for elements associated with network properties.
   * @param {!{detail: !{field: string, value: (string|!Object)}}} event
   * @private
   */
  onNetworkPropertyChange_: function(event) {
    if (!this.networkProperties)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    var onc = this.getEmptyNetworkProperties_();
    if (field == 'APN') {
      CrOnc.setTypeProperty(onc, 'APN', value);
    } else if (field == 'SIMLockStatus') {
      CrOnc.setTypeProperty(onc, 'SIMLockStatus', value);
    } else {
      console.error('Unexpected property change event: ' + field);
      return;
    }
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the IP Config or NameServers element changes.
   * @param {!{detail: !{field: string,
   *                     value: (string|!CrOnc.IPConfigProperties|
   *                             !Array<string>)}}} event
   *     The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.networkProperties)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    // Get an empty ONC dictionary and set just the IP Config properties that
    // need to change.
    var onc = this.getEmptyNetworkProperties_();
    var ipConfigType =
        /** @type {chrome.networkingPrivate.IPConfigType|undefined} */(
            CrOnc.getActiveValue(this.networkProperties.IPAddressConfigType));
    if (field == 'IPAddressConfigType') {
      var newIpConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */(value);
      if (newIpConfigType == ipConfigType)
        return;
      onc.IPAddressConfigType = newIpConfigType;
    } else if (field == 'NameServersConfigType') {
      var nsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType|undefined} */(
              CrOnc.getActiveValue(
                  this.networkProperties.NameServersConfigType));
      var newNsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */(value);
      if (newNsConfigType == nsConfigType)
        return;
      onc.NameServersConfigType = newNsConfigType;
    } else if (field == 'StaticIPConfig') {
      if (ipConfigType == CrOnc.IPConfigType.STATIC) {
        var staticIpConfig = this.networkProperties.StaticIPConfig;
        if (staticIpConfig &&
            this.allPropertiesMatch_(staticIpConfig,
                                     /** @type {!Object} */(value))) {
          return;
        }
      }
      onc.IPAddressConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */({});
      }
      for (let key in value)
        onc.StaticIPConfig[key] = value[key];
    } else if (field == 'NameServers') {
      // If a StaticIPConfig property is specified and its NameServers value
      // matches the new value, no need to set anything.
      var nameServers = /** @type {!Array<string>} */(value);
      if (onc.NameServersConfigType == CrOnc.IPConfigType.STATIC &&
          onc.StaticIPConfig &&
          onc.StaticIPConfig.NameServers == nameServers) {
        return;
      }
      onc.NameServersConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */({});
      }
      onc.StaticIPConfig.NameServers = nameServers;
    } else {
      console.error('Unexpected change field: ' + field);
      return;
    }
    // setValidStaticIPConfig will fill in any other properties from
    // networkProperties. This is necessary since we update IP Address and
    // NameServers independently.
    CrOnc.setValidStaticIPConfig(onc, this.networkProperties);
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!{detail: {field: string, value: !CrOnc.ProxySettings}}} event
   *     The network-proxy change event.
   * @private
   */
  onProxyChange_: function(event) {
    if (!this.networkProperties)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    if (field != 'ProxySettings')
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setProperty(onc, 'ProxySettings', /** @type {!Object} */(value));
    this.setNetworkProperties_(onc);
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function(properties) {
    return properties.Source == 'Device' || properties.Source == 'DevicePolicy';
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function(properties) {
    return properties.Type != CrOnc.Type.ETHERNET &&
           properties.Source != CrOnc.Source.NONE;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!CrOnc.ManagedProperty|undefined} Managed AutoConnect property.
   * @private
   */
  getManagedAutoConnect_: function(properties) {
    return CrOnc.getManagedAutoConnect(properties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function(properties) {
    // TODO(stevenjb): Resolve whether or not we want to allow "preferred" for
    // properties.Type == CrOnc.Type.ETHERNET.
    return properties.Source != CrOnc.Source.NONE;
  },

  /**
   * @param {boolean} preferNetwork
   * @return {string} The icon to use for the preferred button.
   * @private
   */
  getPreferredIcon_: function(preferNetwork) {
    return preferNetwork ? 'cr:star' : 'cr:star-border';
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function(properties) {
    /** @type {!Array<string>} */ var fields = [];
    if (!properties)
      return fields;

    if (properties.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.ActivationState',
                  'Cellular.RoamingState',
                  'RestrictedConnectivity',
                  'Cellular.ServingOperator.Name');
    }
    if (properties.Type == CrOnc.Type.VPN) {
      fields.push('VPN.Host', 'VPN.Type');
      if (properties.VPN.Type == 'OpenVPN')
        fields.push('VPN.OpenVPN.Username');
      else if (properties.VPN.Type == 'L2TP-IPsec')
        fields.push('VPN.L2TP.Username');
      else if (properties.VPN.Type == 'ThirdPartyVPN')
        fields.push('VPN.ThirdPartyVPN.ProviderName');
    }
    if (properties.Type == CrOnc.Type.WI_FI)
      fields.push('RestrictedConnectivity');
    if (properties.Type == CrOnc.Type.WI_MAX) {
      fields.push('RestrictedConnectivity', 'WiMAX.EAP.Identity');
    }
    return fields;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!Array<string>} The fields to display in the Advanced section.
   * @private
   */
  getAdvancedFields_: function(properties) {
    /** @type {!Array<string>} */ var fields = [];
    if (!properties)
      return fields;
    fields.push('MacAddress');
    if (properties.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.Carrier',
                  'Cellular.Family',
                  'Cellular.NetworkTechnology',
                  'Cellular.ServingOperator.Code');
    }
    if (properties.Type == CrOnc.Type.WI_FI) {
      fields.push('WiFi.SSID',
                  'WiFi.BSSID',
                  'WiFi.Security',
                  'WiFi.SignalStrength',
                  'WiFi.Frequency');
    }
    if (properties.Type == CrOnc.Type.WI_MAX)
      fields.push('WiFi.SignalStrength');
    return fields;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!Array<string>} The fields to display in the device section.
   * @private
   */
  getDeviceFields_: function(properties) {
    /** @type {!Array<string>} */ var fields = [];
    if (!properties)
      return fields;
    if (properties.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.HomeProvider.Name',
                  'Cellular.HomeProvider.Country',
                  'Cellular.HomeProvider.Code',
                  'Cellular.Manufacturer',
                  'Cellular.ModelID',
                  'Cellular.FirmwareRevision',
                  'Cellular.HardwareRevision',
                  'Cellular.ESN',
                  'Cellular.ICCID',
                  'Cellular.IMEI',
                  'Cellular.IMSI',
                  'Cellular.MDN',
                  'Cellular.MEID',
                  'Cellular.MIN',
                  'Cellular.PRLVersion');
    }
    return fields;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if there are any advanced fields to display.
   * @private
   */
  hasAdvancedOrDeviceFields_: function(properties) {
    return this.getAdvancedFields_(properties).length > 0 ||
           this.hasDeviceFields_(properties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if there are any device fields to display.
   * @private
   */
  hasDeviceFields_: function(properties) {
    var fields = this.getDeviceFields_(properties);
    return fields.length > 0;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the network section should be shown.
   * @private
   */
  hasNetworkSection_: function(properties) {
    return properties.Type != CrOnc.Type.VPN;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @param {string} type The network type.
   * @return {boolean} True if the network type matches 'type'.
   * @private
   */
  isType_: function(properties, type) {
    return properties.Type == type;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {boolean} True if the Cellular SIM section should be shown.
   * @private
   */
  showCellularSim_: function(properties) {
    if (!properties || properties.Type != 'Cellular' || !properties.Cellular)
      return false;
    return properties.Cellular.Family == 'GSM';
  },

  /**
   * @param {!Object} curValue
   * @param {!Object} newValue
   * @return {boolean} True if all properties set in |newValue| are equal to
   *     the corresponding properties in |curValue|. Note: Not all properties
   *     of |curValue| need to be specified in |newValue| for this to return
   *     true.
   * @private
   */
  allPropertiesMatch_: function(curValue, newValue) {
    for (let key in newValue) {
      if (newValue[key] != curValue[key])
        return false;
    }
    return true;
  }
});
})();
