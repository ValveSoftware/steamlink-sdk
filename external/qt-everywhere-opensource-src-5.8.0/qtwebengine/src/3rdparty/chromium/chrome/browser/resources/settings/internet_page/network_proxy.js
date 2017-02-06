// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */
Polymer({
  is: 'network-proxy',

  behaviors: [CrPolicyNetworkBehavior],

  properties: {
    /**
     * The network properties dictionary containing the proxy properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_'
    },

    /**
     * Whether or not the proxy values can be edited.
     */
    editable: {
      type: Boolean,
      value: false
    },

    /**
     * UI visible / edited proxy configuration.
     * @type {!CrOnc.ProxySettings}
     */
    proxy: {
      type: Object,
      value: function() { return this.createDefaultProxySettings_(); }
    },

    /**
     * The Web Proxy Auto Discovery URL extracted from networkProperties.
     */
    WPAD: {
      type: String,
      value: ''
    },

    /**
     * Whetner or not to use the same manual proxy for all protocols.
     */
    useSameProxy: {
      type: Boolean,
      value: false,
      observer: 'useSameProxyChanged_'
    },

    /**
     * Array of proxy configuration types.
     * @type {!Array<string>}
     * @const
     */
    proxyTypes_: {
      type: Array,
      value: [
        CrOnc.ProxySettingsType.DIRECT,
        CrOnc.ProxySettingsType.PAC,
        CrOnc.ProxySettingsType.WPAD,
        CrOnc.ProxySettingsType.MANUAL
      ],
      readOnly: true
    },

    /**
     * Object providing proxy type values for data binding.
     * @type {!Object}
     * @const
     */
    ProxySettingsType: {
      type: Object,
      value: {
        DIRECT: CrOnc.ProxySettingsType.DIRECT,
        PAC: CrOnc.ProxySettingsType.PAC,
        MANUAL: CrOnc.ProxySettingsType.MANUAL,
        WPAD: CrOnc.ProxySettingsType.WPAD
      },
      readOnly: true
    },
  },

  /**
   * Saved Manual properties so that switching to another type does not loose
   * any set properties while the UI is open.
   * @type {!CrOnc.ManualProxySettings|undefined}
   */
  savedManual_: undefined,

  /**
   * Saved ExcludeDomains properties so that switching to a non-Manual type does
   * not loose any set exclusions while the UI is open.
   * @type {!Array<string>|undefined}
   */
  savedExcludeDomains_: undefined,

  /**
   * Polymer networkProperties changed method.
   */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties)
      return;

    /** @type {!CrOnc.ProxySettings} */
    var proxy = this.createDefaultProxySettings_();
    /** @type {!chrome.networkingPrivate.ManagedProxySettings|undefined} */
    var proxySettings = this.networkProperties.ProxySettings;
    if (proxySettings) {
      proxy.Type = /** @type {!CrOnc.ProxySettingsType} */(
          CrOnc.getActiveValue(proxySettings.Type));
      if (proxySettings.Manual) {
        proxy.Manual.HTTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */(
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.HTTPProxy));
        proxy.Manual.SecureHTTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */(
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.SecureHTTPProxy));
        proxy.Manual.FTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */(
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.FTPProxy));
        proxy.Manual.SOCKS = /** @type {!CrOnc.ProxyLocation|undefined} */(
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.SOCKS));
      }
      if (proxySettings.ExcludeDomains) {
        proxy.ExcludeDomains = /** @type {!Array<string>|undefined} */(
            CrOnc.getActiveValue(proxySettings.ExcludeDomains));
      }
      proxy.PAC = /** @type {string|undefined} */(
          CrOnc.getActiveValue(proxySettings.PAC));
    }
    // Use saved ExcludeDomanains and Manual if not defined.
    proxy.ExcludeDomains = proxy.ExcludeDomains || this.savedExcludeDomains_;
    proxy.Manual = proxy.Manual || this.savedManual_;

    this.set('proxy', proxy);
    this.$.selectType.value = proxy.Type;

    // Set the Web Proxy Auto Discovery URL.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.WPAD = (ipv4 && ipv4.WebProxyAutoDiscoveryUrl) || '';
  },

  /**
   * @return {CrOnc.ProxySettings} An empty/default proxy settings object.
   */
  createDefaultProxySettings_: function() {
    return {
      Type: CrOnc.ProxySettingsType.DIRECT,
      ExcludeDomains: [],
      Manual: {
        HTTPProxy: {Host: '', Port: 80},
        SecureHTTPProxy: {Host: '', Port: 80},
        FTPProxy: {Host: '', Port: 80},
        SOCKS: {Host: '', Port: 1080}
      },
      PAC: ''
    };
  },

  /**
   * Polymer useSameProxy changed method.
   */
  useSameProxyChanged_: function() {
    this.sendProxyChange_();
  },

  /**
   * Called when the proxy changes in the UI.
   */
  sendProxyChange_: function() {
    if (this.proxy.Type == CrOnc.ProxySettingsType.MANUAL) {
      if (this.useSameProxy) {
        var defaultProxy = this.proxy.Manual.HTTPProxy;
        this.set('proxy.Manual.SecureHTTPProxy',
                 Object.assign({}, defaultProxy));
        this.set('proxy.Manual.FTPProxy', Object.assign({}, defaultProxy));
        this.set('proxy.Manual.SOCKS', Object.assign({}, defaultProxy));
      }
      this.savedManual_ = this.proxy.Manual;
      this.savedExcludeDomains_ = this.proxy.ExcludeDomains;
    }
    this.fire('proxy-change', {
      field: 'ProxySettings',
      value: this.proxy
    });
  },

  /**
   * Event triggered when the selected proxy type changes.
   * @param {Event} event The select node change event.
   * @private
   */
  onTypeChange_: function(event) {
    var type = this.proxyTypes_[event.target.selectedIndex];
    this.set('proxy.Type', type);
    if (type != CrOnc.ProxySettingsType.MANUAL ||
        this.savedManual_) {
      this.sendProxyChange_();
    }
  },

  /**
   * Event triggered when a proxy value changes.
   * @param {Event} event The proxy value change event.
   * @private
   */
  onProxyInputChange_: function(event) {
    this.sendProxyChange_();
  },

  /**
   * Event triggered when a proxy exclusion is added.
   * @param {Event} event The add proxy exclusion event.
   * @private
   */
  onAddProxyExclusionTap_: function(event) {
    var value = this.$.proxyExclusion.value;
    if (!value)
      return;
    this.push('proxy.ExcludeDomains', value);
    // Clear input.
    this.$.proxyExclusion.value = '';
    this.sendProxyChange_();
  },

  /**
   * Event triggered when the proxy exclusion list changes.
   * @param {Event} event The remove proxy exclusions change event.
   * @private
   */
  onProxyExclusionsChange_: function(event) {
    this.sendProxyChange_();
  },

  /**
   * @param {string} proxyType The proxy type.
   * @return {string} The description for |proxyType|.
   * @private
   */
  proxyTypeDesc_: function(proxyType) {
    // TODO(stevenjb): Translate.
    if (proxyType == CrOnc.ProxySettingsType.MANUAL)
      return 'Manual proxy configuration';
    if (proxyType == CrOnc.ProxySettingsType.PAC)
      return 'Automatic proxy configuration';
    if (proxyType == CrOnc.ProxySettingsType.WPAD)
      return 'Web proxy autodiscovery';
    return 'Direct Internet connection';
  },

  /**
   * @param {boolean} editable
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {string} key
   * @return {boolean} Whether the property is editable.
   * @private
   */
  isPropertyEditable_: function(editable, networkProperties, key) {
    if (!editable)
      return false;
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */(
        this.get(key, networkProperties));
    return !this.isNetworkPolicyEnforced(property);
  },

  /**
   * @param {string} property The property to test
   * @param {string} value The value to test against
   * @return {boolean} True if property == value
   * @private
   */
  matches_: function(property, value) {
    return property == value;
  }
});
