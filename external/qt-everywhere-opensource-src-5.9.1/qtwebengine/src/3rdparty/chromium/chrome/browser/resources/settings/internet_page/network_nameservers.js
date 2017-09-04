// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network nameserver options.
 */
Polymer({
  is: 'network-nameservers',

  properties: {
    /**
     * The network properties dictionary containing the nameserver properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /** Whether or not the nameservers can be edited. */
    editable: {
      type: Boolean,
      value: false,
    },

    /**
     * Array of nameserver addresses stored as strings.
     * @private {!Array<string>}
     */
    nameservers_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The selected nameserver type.
     * @private
     */
    nameserversType_: {
      type: String,
      value: 'automatic',
    },

    /**
     * Array of nameserver types.
     * @private
     */
    nameserverTypeNames_: {
      type: Array,
      value: ['automatic', 'google', 'custom'],
      readOnly: true,
    },
  },

  /** @const */
  GOOGLE_NAMESERVERS: [
    '8.8.4.4',
    '8.8.8.8',
  ],

  /** @const */
  MAX_NAMESERVERS: 4,

  /**
   * Saved nameservers when switching to 'automatic'.
   * @private {!Array<string>}
   */
  savedNameservers_: [],

  /** @private */
  networkPropertiesChanged_: function(newValue, oldValue) {
    if (!this.networkProperties)
      return;

    if (!oldValue || newValue.GUID != oldValue.GUID)
      this.savedNameservers_ = [];

    // Update the 'nameservers' property.
    var nameservers = [];
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    if (ipv4 && ipv4.NameServers)
      nameservers = ipv4.NameServers;

    // Update the 'nameserversType' property.
    var configType =
        CrOnc.getActiveValue(this.networkProperties.NameServersConfigType);
    var type;
    if (configType == CrOnc.IPConfigType.STATIC) {
      if (nameservers.join(',') == this.GOOGLE_NAMESERVERS.join(',')) {
        type = 'google';
      } else {
        type = 'custom';
      }
    } else {
      type = 'automatic';
    }
    this.setNameservers_(type, nameservers);
  },

  /**
   * @param {string} nameserversType
   * @param {!Array<string>} nameservers
   * @private
   */
  setNameservers_: function(nameserversType, nameservers) {
    if (nameserversType == 'custom') {
      // Add empty entries for unset custom nameservers.
      for (let i = nameservers.length; i < this.MAX_NAMESERVERS; ++i)
        nameservers[i] = '';
    }
    this.nameservers_ = nameservers;
    // Set nameserversType_ after dom-repeat has been stamped.
    this.async(function() {
      this.nameserversType_ = nameserversType;
    }.bind(this));
  },

  /**
   * @param {string} type The nameservers type.
   * @return {string} The description for |type|.
   * @private
   */
  nameserverTypeDesc_: function(type) {
    // TODO(stevenjb): Translate.
    if (type == 'custom')
      return 'Custom name servers';
    if (type == 'google')
      return 'Google name servers';
    return 'Automatic name servers';
  },

  /**
   * @return {boolean} True if the nameservers are editable.
   * @private
   */
  canEdit_: function() {
    return this.editable && this.nameserversType_ == 'custom';
  },

  /**
   * Event triggered when the selected type changes. Updates nameservers and
   * sends the change value if necessary.
   * @param {!Event} event
   * @private
   */
  onTypeChange_: function(event) {
    if (this.nameserversType_ == 'custom')
      this.savedNameservers_ = this.nameservers_;
    let target = /** @type {!HTMLSelectElement} */ (event.target);
    let type = target.value;
    this.nameserversType_ = type;
    if (type == 'custom') {
      // Restore the saved nameservers.
      this.setNameservers_(type, this.savedNameservers_);
      // Only send custom nameservers if they are not empty.
      if (this.savedNameservers_.length == 0)
        return;
    }
    this.sendNameServers_();
  },

  /**
   * Event triggered when a nameserver value changes.
   * @private
   */
  onValueChange_: function() {
    if (this.nameserversType_ != 'custom') {
      // If a user inputs Google nameservers in the custom nameservers fields,
      // |nameserversType| will change to 'google' so don't send the values.
      return;
    }
    this.sendNameServers_();
  },

  /**
   * Sends the current nameservers type (for automatic) or value.
   * @private
   */
  sendNameServers_: function() {
    var type = this.nameserversType_;

    if (type == 'custom') {
      let nameservers = [];
      for (let i = 0; i < this.MAX_NAMESERVERS; ++i) {
        let id = 'nameserver' + i;
        let nameserverInput = this.$$('#' + id);
        let nameserver = '';
        if (nameserverInput)
          nameserver = this.$$('#' + id).value;
        nameservers.push(nameserver);
      }
      this.fire('nameservers-change', {
        field: 'NameServers',
        value: nameservers,
      });
    } else if (type == 'google') {
      let nameservers = this.GOOGLE_NAMESERVERS;
      this.fire('nameservers-change', {
        field: 'NameServers',
        value: nameservers,
      });
    } else {
      // automatic
      this.fire('nameservers-change', {
        field: 'NameServersConfigType',
        value: CrOnc.IPConfigType.DHCP,
      });
    }
  },
});
