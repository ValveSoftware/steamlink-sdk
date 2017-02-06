// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */

Polymer({
  is: 'cr-policy-network-indicator',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyNetworkBehavior],

  properties: {
    /**
     * Network property associated with the indicator.
     * @type {!CrOnc.ManagedProperty|undefined}
     */
    property: {type: Object, observer: 'propertyChanged_'},

    /**
     * Which indicator type to show (or NONE).
     * @type {!CrPolicyIndicatorType}
     */
    indicatorType: {type: String, value: CrPolicyIndicatorType.NONE},

    /**
     * Recommended value for non enforced properties.
     * @type {?CrOnc.NetworkPropertyType}
     */
    recommended: {type: Object, value: null},
  },

  /**
   * @param {!CrOnc.ManagedProperty} property Always defined property value.
   * @private
   */
  propertyChanged_: function(property) {
    if (!this.isNetworkPolicyControlled(property)) {
      this.indicatorType = CrPolicyIndicatorType.NONE;
      return;
    }
    var effective = property.Effective;
    var active = property.Active;
    if (active == undefined)
      active = property[effective];

    if (property.UserEditable === true &&
        property.hasOwnProperty('UserPolicy')) {
      // We ignore UserEditable unless there is a UserPolicy.
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
      this.recommended =
          /** @type {CrOnc.NetworkPropertyType} */(property.UserPolicy);
    } else if (property.DeviceEditable === true &&
               property.hasOwnProperty('DevicePolicy')) {
      // We ignore DeviceEditable unless there is a DevicePolicy.
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
      this.recommended =
          /** @type {CrOnc.NetworkPropertyType} */(property.DevicePolicy);
    } else if (effective == 'UserPolicy') {
      this.indicatorType = CrPolicyIndicatorType.USER_POLICY;
    } else if (effective == 'DevicePolicy') {
      this.indicatorType = CrPolicyIndicatorType.DEVICE_POLICY;
    } else {
      this.indicatorType = CrPolicyIndicatorType.NONE;
    }
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @param {!CrOnc.ManagedProperty} property
   * @param {!CrOnc.NetworkPropertyType} recommended
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltip_: function(type, property, recommended) {
    if (type == CrPolicyIndicatorType.NONE || typeof property != 'object')
      return '';
    if (type == CrPolicyIndicatorType.RECOMMENDED) {
      var value = property.Active;
      if (value == undefined && property.Effective)
        value = property[property.Effective];
      if (value == recommended)
        return this.i18n_('controlledSettingRecommendedMatches');
      return this.i18n_('controlledSettingRecommendedDiffers');
    }
    return this.getPolicyIndicatorTooltip(type, '');
  }
});
