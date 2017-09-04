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

  behaviors:
      [CrPolicyNetworkBehavior, settings.RouteObserverBehavior, I18nBehavior],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /**
     * The current properties for the network matching |guid|.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Highest priority connected network or null.
     * @type {?CrOnc.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * The network AutoConnect state.
     * @private
     */
    autoConnect_: {
      type: Boolean,
      value: false,
      observer: 'autoConnectChanged_',
    },

    /**
     * The network preferred state.
     * @private
     */
    preferNetwork_: {
      type: Boolean,
      value: false,
      observer: 'preferNetworkChanged_',
    },

    /**
     * The network IP Address.
     * @private
     */
    IPAddress_: {
      type: String,
      value: '',
    },

    /** @private */
    advancedExpanded_: Boolean,

    /**
     * Object providing network type values for data binding.
     * @const
     * @private
     */
    NetworkType_: {
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
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {?function(!Array<string>)}
   * @private
   */
  networksChangedListener_: null,

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.Route.NETWORK_DETAIL) {
      if (this.networksChangedListener_) {
        this.networkingPrivate.onNetworksChanged.removeListener(
            this.networksChangedListener_);
        this.networksChangedListener_ = null;
      }
      return;
    }
    if (!this.networksChangedListener_) {
      this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
      this.networkingPrivate.onNetworksChanged.addListener(
          this.networksChangedListener_);
    }
    let queryParams = settings.getQueryParameters();
    this.guid = queryParams.get('guid') || '';
    if (!this.guid) {
      console.error('No guid specified for page:' + route);
      this.close_();
    }
    this.getNetworkDetails_();
  },

  /** @private */
  close_: function() {
    // Delay navigating until the next render frame to allow other subpages to
    // load first.
    setTimeout(function() {
      settings.navigateTo(settings.Route.INTERNET);
    });
  },

  /** @private */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties)
      return;

    // Update autoConnect if it has changed. Default value is false.
    var autoConnect = CrOnc.getAutoConnect(this.networkProperties);
    if (autoConnect != this.autoConnect_)
      this.autoConnect_ = autoConnect;

    // Update preferNetwork if it has changed. Default value is false.
    var priority = /** @type {number} */ (
        CrOnc.getActiveValue(this.networkProperties.Priority) || 0);
    var preferNetwork = priority > 0;
    if (preferNetwork != this.preferNetwork_)
      this.preferNetwork_ = preferNetwork;

    // Set the IPAddress property to the IPV4 Address.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.IPAddress_ = (ipv4 && ipv4.IPAddress) || '';

    // Update the detail page title.
    this.parentNode.pageTitle =
        CrOnc.getNetworkName(this.networkProperties, this);
  },

  /** @private */
  autoConnectChanged_: function() {
    if (!this.networkProperties || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setTypeProperty(onc, 'AutoConnect', this.autoConnect_);
    this.setNetworkProperties_(onc);
  },

  /** @private */
  preferNetworkChanged_: function() {
    if (!this.networkProperties || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    onc.Priority = this.preferNetwork_ ? 1 : 0;
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
    assert(!!this.guid);
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
      console.error('Network no longer exists: ' + this.guid);
      this.close_();
    }
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    assert(!!this.guid);
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
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function() {
    return this.i18n('Onc' + this.networkProperties.ConnectionState);
  },

  /**
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function() {
    return this.networkProperties.ConnectionState ==
        CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isRemembered_: function() {
    var source = this.networkProperties.Source;
    return !!source && source != CrOnc.Source.NONE;
  },

  /**
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function() {
    return this.isRemembered_() || this.isConnectedState_();
  },

  /**
   * @return {boolean}
   * @private
   */
  showConnect_: function() {
    return this.networkProperties.Type != CrOnc.Type.ETHERNET &&
        this.networkProperties.ConnectionState ==
        CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDisconnect_: function() {
    return this.networkProperties.Type != CrOnc.Type.ETHERNET &&
        this.networkProperties.ConnectionState !=
        CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showForget_: function() {
    var type = this.networkProperties.Type;
    if (type != CrOnc.Type.WI_FI && type != CrOnc.Type.VPN)
      return false;
    return this.isRemembered_();
  },

  /**
   * @return {boolean}
   * @private
   */
  showActivate_: function() {
    if (this.networkProperties.Type != CrOnc.Type.CELLULAR)
      return false;
    var activation = this.networkProperties.Cellular.ActivationState;
    return activation == CrOnc.ActivationState.NOT_ACTIVATED ||
        activation == CrOnc.ActivationState.PARTIALLY_ACTIVATED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showConfigure_: function() {
    var type = this.networkProperties.Type;
    if (type == CrOnc.Type.CELLULAR || type == CrOnc.Type.WI_MAX)
      return false;
    if (type == CrOnc.Type.WI_FI &&
        this.networkProperties.ConnectionState !=
            CrOnc.ConnectionState.NOT_CONNECTED) {
      return false;
    }
    return this.isRemembered_();
  },

  /**
   * @return {boolean}
   * @private
   */
  showViewAccount_: function() {
    // Show either the 'Activate' or the 'View Account' button.
    if (this.showActivate_())
      return false;

    if (this.networkProperties.Type != CrOnc.Type.CELLULAR ||
        !this.networkProperties.Cellular) {
      return false;
    }

    // Only show if online payment URL is provided or the carrier is Verizon.
    var carrier = CrOnc.getActiveValue(this.networkProperties.Cellular.Carrier);
    if (carrier != CARRIER_VERIZON) {
      var paymentPortal = this.networkProperties.Cellular.PaymentPortal;
      if (!paymentPortal || !paymentPortal.Url)
        return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_()) {
      var technology = this.networkProperties.Cellular.NetworkTechnology;
      if (technology != CrOnc.NetworkTechnology.LTE &&
          technology != CrOnc.NetworkTechnology.LTE_ADVANCED) {
        return false;
      }
      if (!this.networkProperties.Cellular.MDN)
        return false;
    }

    return true;
  },

  /**
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function() {
    if (!this.showConnect_())
      return false;
    if (this.networkProperties.Type == CrOnc.Type.CELLULAR &&
        CrOnc.isSimLocked(this.networkProperties)) {
      return false;
    }
    if (this.networkProperties.Type == CrOnc.Type.VPN && !this.defaultNetwork)
      return false;
    return true;
  },

  /** @private */
  onConnectTap_: function() {
    this.networkingPrivate.startConnect(this.guid);
  },

  /** @private */
  onDisconnectTap_: function() {
    this.networkingPrivate.startDisconnect(this.guid);
  },

  /** @private */
  onForgetTap_: function() {
    this.networkingPrivate.forgetNetwork(this.guid);
    // A forgotten WiFi network can still be configured, but not other types.
    if (this.networkProperties.Type != CrOnc.Type.WI_FI)
      this.close_();
  },

  /** @private */
  onActivateTap_: function() {
    this.networkingPrivate.startActivate(this.guid);
  },

  /** @private */
  onConfigureTap_: function() {
    chrome.send('configureNetwork', [this.guid]);
  },

  /** @private */
  onViewAccountTap_: function() {
    // startActivate() will show the account page for activated networks.
    this.networkingPrivate.startActivate(this.guid);
  },

  /**
   * @param {Event} event
   * @private
   */
  toggleAdvancedExpanded_: function(event) {
    if (event.target.id == 'expandButton')
      return;  // Already handled.
    this.advancedExpanded_ = !this.advancedExpanded_;
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
        /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
            CrOnc.getActiveValue(this.networkProperties.IPAddressConfigType));
    if (field == 'IPAddressConfigType') {
      var newIpConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newIpConfigType == ipConfigType)
        return;
      onc.IPAddressConfigType = newIpConfigType;
    } else if (field == 'NameServersConfigType') {
      var nsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
              CrOnc.getActiveValue(
                  this.networkProperties.NameServersConfigType));
      var newNsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newNsConfigType == nsConfigType)
        return;
      onc.NameServersConfigType = newNsConfigType;
    } else if (field == 'StaticIPConfig') {
      if (ipConfigType == CrOnc.IPConfigType.STATIC) {
        let staticIpConfig = this.networkProperties.StaticIPConfig;
        let ipConfigValue = /** @type {!Object} */ (value);
        if (staticIpConfig &&
            this.allPropertiesMatch_(staticIpConfig, ipConfigValue)) {
          return;
        }
      }
      onc.IPAddressConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
      }
      for (let key in value)
        onc.StaticIPConfig[key] = value[key];
    } else if (field == 'NameServers') {
      // If a StaticIPConfig property is specified and its NameServers value
      // matches the new value, no need to set anything.
      let nameServers = /** @type {!Array<string>} */ (value);
      if (onc.NameServersConfigType == CrOnc.IPConfigType.STATIC &&
          onc.StaticIPConfig && onc.StaticIPConfig.NameServers == nameServers) {
        return;
      }
      onc.NameServersConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
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
    CrOnc.setProperty(onc, 'ProxySettings', /** @type {!Object} */ (value));
    this.setNetworkProperties_(onc);
  },

  /**
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function() {
    return this.networkProperties.Source == 'Device' ||
        this.networkProperties.Source == 'DevicePolicy';
  },

  /**
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function() {
    return this.networkProperties.Type != CrOnc.Type.ETHERNET &&
        this.isRemembered_();
  },

  /**
   * @return {!CrOnc.ManagedProperty|undefined} Managed AutoConnect property.
   * @private
   */
  getManagedAutoConnect_: function() {
    return CrOnc.getManagedAutoConnect(this.networkProperties);
  },

  /**
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function() {
    // TODO(stevenjb): Resolve whether or not we want to allow "preferred" for
    // networkProperties.Type == CrOnc.Type.ETHERNET.
    return this.isRemembered_();
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_: function(fields) {
    for (let key of fields) {
      let value = this.get(key, this.networkProperties);
      if (value !== undefined && value !== '')
        return true;
    }
    return false;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasInfoFields_: function() {
    return this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function() {
    /** @type {!Array<string>} */ var fields = [];
    if (this.networkProperties.Type == CrOnc.Type.CELLULAR) {
      fields.push(
          'Cellular.ActivationState', 'Cellular.RoamingState',
          'RestrictedConnectivity', 'Cellular.ServingOperator.Name');
    }
    if (this.networkProperties.Type == CrOnc.Type.VPN) {
      let vpnType = CrOnc.getActiveValue(this.networkProperties.VPN.Type);
      if (vpnType == 'ThirdPartyVPN') {
        fields.push('VPN.ThirdPartyVPN.ProviderName');
      } else {
        fields.push('VPN.Host', 'VPN.Type');
        if (vpnType == 'OpenVPN')
          fields.push('VPN.OpenVPN.Username');
        else if (vpnType == 'L2TP-IPsec')
          fields.push('VPN.L2TP.Username');
      }
    }
    if (this.networkProperties.Type == CrOnc.Type.WI_FI)
      fields.push('RestrictedConnectivity');
    if (this.networkProperties.Type == CrOnc.Type.WI_MAX) {
      fields.push('RestrictedConnectivity', 'WiMAX.EAP.Identity');
    }
    return fields;
  },

  /**
   * @return {!Array<string>} The fields to display in the Advanced section.
   * @private
   */
  getAdvancedFields_: function() {
    /** @type {!Array<string>} */ var fields = [];
    fields.push('MacAddress');
    if (this.networkProperties.Type == CrOnc.Type.CELLULAR) {
      fields.push(
          'Cellular.Carrier', 'Cellular.Family', 'Cellular.NetworkTechnology',
          'Cellular.ServingOperator.Code');
    }
    if (this.networkProperties.Type == CrOnc.Type.WI_FI) {
      fields.push(
          'WiFi.SSID', 'WiFi.BSSID', 'WiFi.Security', 'WiFi.SignalStrength',
          'WiFi.Frequency');
    }
    if (this.networkProperties.Type == CrOnc.Type.WI_MAX)
      fields.push('WiFi.SignalStrength');
    return fields;
  },

  /**
   * @return {!Array<string>} The fields to display in the device section.
   * @private
   */
  getDeviceFields_: function() {
    /** @type {!Array<string>} */ var fields = [];
    if (this.networkProperties.Type == CrOnc.Type.CELLULAR) {
      fields.push(
          'Cellular.HomeProvider.Name', 'Cellular.HomeProvider.Country',
          'Cellular.HomeProvider.Code', 'Cellular.Manufacturer',
          'Cellular.ModelID', 'Cellular.FirmwareRevision',
          'Cellular.HardwareRevision', 'Cellular.ESN', 'Cellular.ICCID',
          'Cellular.IMEI', 'Cellular.IMSI', 'Cellular.MDN', 'Cellular.MEID',
          'Cellular.MIN', 'Cellular.PRLVersion');
    }
    return fields;
  },

  /**
   * @return {boolean}
   * @private
   */
  showAdvanced_: function() {
    return this.hasAdvancedFields_() || this.hasDeviceFields_() ||
        this.isRememberedOrConnected_();
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedFields_: function() {
    return this.hasVisibleFields_(this.getAdvancedFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDeviceFields_: function() {
    return this.hasVisibleFields_(this.getDeviceFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedOrDeviceFields_: function() {
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  },

  /**
   * @return {boolean} True if the network section should be shown.
   * @private
   */
  hasNetworkSection_: function() {
    if (this.networkProperties.Type == CrOnc.Type.VPN)
      return false;
    if (this.networkProperties.Type == CrOnc.Type.CELLULAR)
      return true;
    return this.isRememberedOrConnected_();
  },

  /**
   * @param {string} type The network type.
   * @return {boolean} True if the network type matches 'type'.
   * @private
   */
  isType_: function(type) {
    return this.networkProperties.Type == type;
  },

  /**
   * @return {boolean} True if the Cellular SIM section should be shown.
   * @private
   */
  showCellularSim_: function() {
    if (this.networkProperties.Type != 'Cellular' ||
        !this.networkProperties.Cellular) {
      return false;
    }
    return this.networkProperties.Cellular.Family == 'GSM';
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
