// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */

Polymer({
  is: 'cr-network-list-item',

  properties: {
    /** @type {!CrNetworkList.CrNetworkListItemType|undefined} */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    /**
     * The ONC data properties used to display the list item.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
    },

    /**
     * Determines how the list item will be displayed:
     *  True - Displays the network icon (with strength) and name
     *  False - The element is a stand-alone item (e.g. part of a summary)
     *      and displays the name of the network type plus the network name
     *      and connection state.
     */
    isListItem: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** Whether to show any buttons for network items. Defaults to false. */
    showButtons: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Reflect the element's tabindex attribute to a property so that embedded
     * elements (e.g. the settings button) can become keyboard focusable when
     * this element has keyboard focus.
     */
    tabindex: {
      type: Number,
      value: -1,
      reflectToAttribute: true,
    },
  },

  behaviors: [I18nBehavior],

  /** @private */
  itemChanged_: function() {
    if (this.item && !this.item.hasOwnProperty('customItemName')) {
      this.networkState =
          /** @type {!CrOnc.NetworkStateProperties} */ (this.item);
    } else if (this.networkState) {
      this.networkState = undefined;
    }
  },

  /** @private */
  networkStateChanged_: function() {
    if (this.networkState &&
        this.networkState.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.fire('network-connected', this.networkState);
    }
  },

  /**
   * This gets called for network items and custom items.
   * @private
   */
  getItemName_: function() {
    if (this.item.hasOwnProperty('customItemName')) {
      let item = /** @type {!CrNetworkList.CustomItemState} */ (this.item);
      let name = item.customItemName || '';
      if (this.i18nExists(item.customItemName))
        name = this.i18n(item.customItemName);
      return name;
    }
    let network = /** @type {!CrOnc.NetworkStateProperties} */ (this.item);
    if (this.isListItem)
      return CrOnc.getNetworkName(network, this);
    return this.i18n('OncType' + network.Type);
  },

  /** @private */
  isStateTextVisible_() {
    return !!this.networkState && (!this.isListItem || this.isConnected_());
  },

  /** @private */
  isStateTextConnected_() {
    return this.isListItem && this.isConnected_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @private
   */
  getNetworkStateText_: function() {
    if (!this.isStateTextVisible_())
      return '';
    let network = this.networkState;
    if (this.isListItem) {
      if (this.isConnected_())
        return this.i18n('networkListItemConnected');
      return '';
    }
    if (network.Name && network.ConnectionState) {
      return this.getConnectionStateText_(
          network.ConnectionState, CrOnc.getNetworkName(network, this));
    }
    return this.i18n('networkDisabled');
  },

  /**
   * Returns the appropriate connection state text.
   * @param {string} state The connection state.
   * @param {string} name The name of the network.
   * @return {string}
   */
  getConnectionStateText_: function(state, name) {
    if (state == CrOnc.ConnectionState.CONNECTED)
      return name;
    if (state == CrOnc.ConnectionState.CONNECTING)
      return this.i18n('networkListItemConnecting', name);
    if (state == CrOnc.ConnectionState.NOT_CONNECTED)
      return this.i18n('networkListItemNotConnected');
    // TODO(stevenjb): Audit state translations and remove test.
    if (this.i18nExists(state))
      return this.i18n(state);
    return state;
  },

  /** @private */
  isSettingsButtonVisible_: function() {
    return !!this.networkState && this.showButtons;
  },

  /** @private */
  isConnected_: function() {
    return this.networkState &&
        this.networkState.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },
});
