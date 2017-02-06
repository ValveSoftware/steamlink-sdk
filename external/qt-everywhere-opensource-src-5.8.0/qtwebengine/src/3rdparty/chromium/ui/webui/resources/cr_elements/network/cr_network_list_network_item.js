// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */
(function() {
'use strict';

/**
 * TODO(stevenjb): Replace getText with a proper localization function that
 * handles string substitution.
 * Performs argument substitution, replacing $1, $2, etc in 'text' with
 * corresponding entries in |args|.
 * @param {string} text The string to perform the substitution on.
 * @param {?Array<string>=} opt_args The arguments to replace $1, $2, etc with.
 */
function getText(text, opt_args) {
  var res;
  if (loadTimeData && loadTimeData.data_)
    res = loadTimeData.getString(text) || text;
  else
    res = text;
  if (!opt_args)
    return res;
  for (let i = 0; i < opt_args.length; ++i) {
    let key = '$' + (i + 1);
    res = res.replace(key, opt_args[i]);
  }
  return res;
}

/**
 * Returns the appropriate connection state text.
 * @param {string} state The connection state.
 * @param {string} name The name of the network.
 */
function getConnectionStateText(state, name) {
  if (state == CrOnc.ConnectionState.CONNECTED)
    return getText('networkConnected', [name]);
  if (state == CrOnc.ConnectionState.CONNECTING)
    return getText('networkConnecting', [name]);
  if (state == CrOnc.ConnectionState.NOT_CONNECTED)
    return getText('networkNotConnected');
  return getText(state);
};

/**
 * Returns the name to display for |network|.
 * @param {?CrOnc.NetworkStateProperties} network
 */
function getNetworkName(network) {
  var name = network.Name;
  if (!name)
    return getText('OncType' + network.Type);
  if (network.Type == 'VPN' && network.VPN &&
      network.VPN.Type == 'ThirdPartyVPN' && network.VPN.ThirdPartyVPN) {
    var providerName = network.VPN.ThirdPartyVPN.ProviderName;
    if (providerName)
      return getText('vpnNameTemplate', [providerName, name]);
  }
  return name;
}

/**
 * Polymer class definition for 'cr-network-list-network-item'.
 */
Polymer({
  is: 'cr-network-list-network-item',

  properties: {
    /**
     * The ONC data properties used to display the list item.
     *
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * Determines how the list item will be displayed:
     *  'visible' - displays the network icon (with strength) and name
     *  'known' - displays the visible info along with a toggle icon for the
     *      preferred status and a remove button.
     *  'none' - The element is a stand-alone item (e.g. part of a summary)
     *      and displays the name of the network type plus the network name
     *      and connection state.
     */
    listItemType: {
      type: String,
      value: 'none',
      observer: 'networkStateChanged_'
    },

    /**
     * Whether to show buttons.
     */
    showButtons: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Polymer networkState changed method. Updates the element based on the
   * network state.
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;
    var network = this.networkState;
    var isDisconnected =
        network.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;

    if (network.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.fire("network-connected", this.networkState);
    }

    var name = getNetworkName(network);
    if (this.isListItem_(this.listItemType)) {
      this.$.itemName.textContent = name;
      this.$.itemName.classList.toggle('connected', !isDisconnected);
      return;
    }
    if (network.Name && network.ConnectionState) {
      this.$.itemName.textContent = getText('OncType' + network.Type);
      this.$.itemName.classList.toggle('connected', false);
      this.$.networkStateText.textContent =
          getConnectionStateText(network.ConnectionState, name);
      this.$.networkStateText.classList.toggle('connected', !isDisconnected);
      return;
    }
    this.$.itemName.textContent = getText('OncType' + network.Type);
    this.$.itemName.classList.toggle('connected', false);
    this.$.networkStateText.textContent = getText('networkDisabled');
    this.$.networkStateText.classList.toggle('connected', false);

    if (network.Type == CrOnc.Type.CELLULAR) {
      if (!network.GUID)
        this.$.networkStateText.textContent = getText('networkDisabled');
    }
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} networkState
   * @return {string} The icon to use for the shared button indicator.
   * @private
   */
  sharedIcon_: function(networkState) {
    var source = (networkState && networkState.Source) || '';
    var isShared = (source == CrOnc.Source.DEVICE ||
                    source == CrOnc.Source.DEVICE_POLICY);
    return isShared ? 'cr:check' : '';
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} networkState
   * @return {string} The icon to use for the preferred button.
   * @private
   */
  preferredIcon_: function(networkState) {
    var isPreferred = networkState && networkState.Priority > 0;
    return isPreferred ? 'cr:star' : 'cr:star-border';
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },

  /**
   * Fires the 'toggle-preferred' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireTogglePreferred_: function(event) {
    this.fire('toggle-preferred', this.networkState);
    event.stopPropagation();
  },

  /**
   * Fires the 'remove' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireRemove_: function(event) {
    this.fire('remove', this.networkState);
    event.stopPropagation();
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} networkState
   * @return {boolean} True if the network is managed by a policy.
   * @private
   */
  isPolicyManaged_: function(networkState) {
    var source = (networkState && networkState.Source) || '';
    var isPolicyManaged = source == CrOnc.Source.USER_POLICY ||
                          source == CrOnc.Source.DEVICE_POLICY;
    return isPolicyManaged;
  },

  /**
   * @param {string} listItemType The list item type.
   * @return {boolean} True if the the list item type is not 'none'.
   * @private
   */
  isListItem_: function(listItemType) {
    return listItemType != 'none';
  },

  /**
   * @param {string} listItemType The list item type.
   * @param {string} type The type to match against.
   * @return {boolean} True if the the list item type matches |type|.
   * @private
   */
  isListItemType_: function(listItemType, type) {
    return listItemType == type;
  },

  /**
   * @param {boolean} showButtons this.showButtons property
   * @param {string} listItemType this.listItemType property
   * @private
   */
  isSettingsButtonVisible_: function(showButtons, listItemType) {
    return showButtons && this.isListItemType_(listItemType, 'visible');
  },

  /**
   * @param {boolean} showButtons this.showButtons property
   * @param {string} listItemType this.listItemType property
   * @private
   */
  areKnownButtonsVisible_: function(showButtons, listItemType) {
    return showButtons && this.isListItemType_(listItemType, 'known');
  },
});
})();
