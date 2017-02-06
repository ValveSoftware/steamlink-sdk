// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */
Polymer({
  is: 'cr-policy-pref-indicator',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyPrefBehavior],

  properties: {
    /**
     * Optional preference object associated with the indicator. Initialized to
     * null so that computed functions will get called if this is never set.
     * @type {?chrome.settingsPrivate.PrefObject}
     */
    pref: {type: Object, value: null},

    /**
     * Optional email of the user controlling the setting when the setting does
     * not correspond to a pref (Chrome OS only). Only used when pref is null.
     * Initialized to '' so that computed functions will get called if this is
     * never set. TODO(stevenjb/michaelpg): Create a separate indicator for
     * non-pref (i.e. explicitly set) indicators (see languyage_detail_page).
     */
    controllingUser: {type: String, value: ''},

    /**
     * Which indicator type to show (or NONE).
     * @type {CrPolicyIndicatorType}
     */
    indicatorType: {
      type: String,
      value: CrPolicyIndicatorType.NONE,
      computed: 'getIndicatorType(pref.policySource, pref.policyEnforcement)',
    },
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @param {?chrome.settingsPrivate.PrefObject} pref
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltip_: function(type, pref, controllingUser) {
    if (type == CrPolicyIndicatorType.RECOMMENDED) {
      if (pref && pref.value == pref.recommendedValue)
        return this.i18n_('controlledSettingRecommendedMatches');
      return this.i18n_('controlledSettingRecommendedDiffers');
    }
    var name = pref ? pref.policySourceName : controllingUser;
    return this.getPolicyIndicatorTooltip(type, name);
  }
});
