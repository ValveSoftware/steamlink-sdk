// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */
Polymer({
  is: 'settings-internet-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls. May be overriden by tests.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The network type for the known networks subpage.
     * @private
     */
    knownNetworksType_: String,

    /**
     * Whether the 'Add connection' section is expanded.
     * @private
     */
    addConnectionExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * List of third party VPN providers.
     * @type {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
     * @private
     */
    thirdPartyVpnProviders_: {
      type: Array,
      value: function() {
        return [];
      }
    },
  },

  /** @override */
  attached: function() {
    chrome.management.onInstalled.addListener(
        this.onExtensionAdded_.bind(this));
    chrome.management.onEnabled.addListener(this.onExtensionAdded_.bind(this));
    chrome.management.onUninstalled.addListener(
        this.onExtensionRemoved_.bind(this));
    chrome.management.onDisabled.addListener(
        this.onExtensionDisabled_.bind(this));

    chrome.management.getAll(this.onGetAllExtensions_.bind(this));
  },

  /** @override */
  detached: function() {
    chrome.management.onInstalled.removeListener(
        this.onExtensionAdded_.bind(this));
    chrome.management.onEnabled.removeListener(
        this.onExtensionAdded_.bind(this));
    chrome.management.onUninstalled.removeListener(
        this.onExtensionRemoved_.bind(this));
    chrome.management.onDisabled.removeListener(
        this.onExtensionDisabled_.bind(this));
  },

  /**
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onShowDetail_: function(event) {
    settings.navigateTo(
        settings.Route.NETWORK_DETAIL,
        new URLSearchParams('guid=' + event.detail.GUID));
  },

  /**
   * @param {!{detail: {type: string}}} event
   * @private
   */
  onShowKnownNetworks_: function(event) {
    this.knownNetworksType_ = event.detail.type;
    settings.navigateTo(settings.Route.KNOWN_NETWORKS);
  },

  /**
   * Event triggered when the 'Add connections' div is tapped.
   * @param {Event} event
   * @private
   */
  onExpandAddConnectionsTap_: function(event) {
    if (event.target.id == 'expandAddConnections')
      return;
    this.addConnectionExpanded_ = !this.addConnectionExpanded_;
  },

  /** @private */
  onAddWiFiTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.WI_FI]);
  },

  /** @private */
  onAddVPNTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.VPN]);
  },

  /**
   * @param {!{model:
   *            !{item: !chrome.networkingPrivate.ThirdPartyVPNProperties},
   *        }} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    let provider = event.model.item;
    chrome.send('addNetwork', [CrOnc.Type.VPN, provider.ExtensionID]);
  },

  /**
   * chrome.management.getAll callback.
   * @param {!Array<!chrome.management.ExtensionInfo>} extensions
   * @private
   */
  onGetAllExtensions_: function(extensions) {
    let vpnProviders = [];
    for (var extension of extensions)
      this.addVpnProvider_(vpnProviders, extension);
    this.thirdPartyVpnProviders_ = vpnProviders;
  },

  /**
   * If |extension| is a third-party VPN provider, add it to |vpnProviders|.
   * @param {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
   *     vpnProviders
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  addVpnProvider_: function(vpnProviders, extension) {
    if (!extension.enabled ||
        extension.permissions.indexOf('vpnProvider') == -1) {
      return;
    }
    if (vpnProviders.find(function(provider) {
          return provider.ExtensionID == extension.id;
        })) {
      return;
    }
    var newProvider = {
      ExtensionID: extension.id,
      ProviderName: extension.name,
    };
    vpnProviders.push(newProvider);
  },

  /**
   * chrome.management.onInstalled or onEnabled event.
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  onExtensionAdded_: function(extension) {
    this.addVpnProvider_(this.thirdPartyVpnProviders_, extension);
  },

  /**
   * chrome.management.onUninstalled event.
   * @param {string} extensionId
   * @private
   */
  onExtensionRemoved_: function(extensionId) {
    for (var i = 0; i < this.thirdPartyVpnProviders_.length; ++i) {
      var provider = this.thirdPartyVpnProviders_[i];
      if (provider.ExtensionID == extensionId) {
        this.splice('thirdPartyVpnProviders_', i, 1);
        break;
      }
    }
  },

  /**
   * chrome.management.onDisabled event.
   * @param {{id: string}} extension
   * @private
   */
  onExtensionDisabled_: function(extension) {
    this.onExtensionRemoved_(extension.id);
  },

  /**
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} provider
   * @return {string}
   */
  getAddThirdParrtyVpnLabel_: function(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.ProviderName);
  }
});
