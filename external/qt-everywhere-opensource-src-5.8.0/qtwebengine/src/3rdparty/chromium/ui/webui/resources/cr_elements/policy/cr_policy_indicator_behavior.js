// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled indicators.
 */

/** @enum {string} */
var CrPolicyIndicatorType = {
  DEVICE_POLICY: 'devicePolicy',
  EXTENSION: 'extension',
  NONE: 'none',
  OWNER: 'owner',
  PRIMARY_USER: 'primary_user',
  RECOMMENDED: 'recommended',
  USER_POLICY: 'userPolicy',
};

/** @polymerBehavior */
var CrPolicyIndicatorBehavior = {
  /**
   * @param {CrPolicyIndicatorType} type
   * @return {boolean} True if the indicator should be shown.
   * @private
   */
  isIndicatorVisible: function(type) {
    return type != CrPolicyIndicatorType.NONE;
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @return {string} The iron-icon icon name.
   * @private
   */
  getPolicyIndicatorIcon: function(type) {
    var icon = '';
    switch (type) {
      case CrPolicyIndicatorType.NONE:
        return icon;
      case CrPolicyIndicatorType.PRIMARY_USER:
        icon = 'group';
        break;
      case CrPolicyIndicatorType.OWNER:
        icon = 'person';
        break;
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
      case CrPolicyIndicatorType.RECOMMENDED:
        icon = 'domain';
        break;
      case CrPolicyIndicatorType.EXTENSION:
        icon = 'extension';
        break;
      default:
        assertNotReached();
    }
    return 'cr:' + icon;
  },

  /**
   * @param {string} id The id of the string to translate.
   * @param {string=} opt_name An optional name argument.
   * @return The translated string.
   */
  i18n_: function (id, opt_name) {
    return loadTimeData.getStringF(id, opt_name);
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @param {string} name The name associated with the controllable. See
   *     chrome.settingsPrivate.PrefObject.policySourceName
   * @return {string} The tooltip text for |type|.
   */
  getPolicyIndicatorTooltip: function(type, name) {
    switch (type) {
      case CrPolicyIndicatorType.PRIMARY_USER:
        return this.i18n_('controlledSettingShared', name);
      case CrPolicyIndicatorType.OWNER:
        return this.i18n_('controlledSettingOwner', name);
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
        return this.i18n_('controlledSettingPolicy');
      case CrPolicyIndicatorType.EXTENSION:
        return this.i18n_('controlledSettingExtension', name);
      case CrPolicyIndicatorType.RECOMMENDED:
        // This case is not handled here since it requires knowledge of the
        // value and recommended value associated with the controllable.
        assertNotReached();
    }
    return '';
  }
};
