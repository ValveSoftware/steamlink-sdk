// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs.
 */

/** @polymerBehavior */
var CrPolicyPrefBehavior = {
  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} True if the pref is controlled by an enforced policy.
   */
  isPrefPolicyControlled: function(pref) {
    return pref.policyEnforcement ==
           chrome.settingsPrivate.PolicyEnforcement.ENFORCED;
  },

  /**
   * @param {chrome.settingsPrivate.PolicySource} source
   * @param {chrome.settingsPrivate.PolicyEnforcement} enforcement
   * @return {CrPolicyIndicatorType} The indicator type based on |source| and
   *     |enforcement|.
   */
  getIndicatorType: function(source, enforcement) {
    if (enforcement == chrome.settingsPrivate.PolicyEnforcement.RECOMMENDED)
      return CrPolicyIndicatorType.RECOMMENDED;
    if (enforcement == chrome.settingsPrivate.PolicyEnforcement.ENFORCED) {
      switch (source) {
        case chrome.settingsPrivate.PolicySource.PRIMARY_USER:
          return CrPolicyIndicatorType.PRIMARY_USER;
        case chrome.settingsPrivate.PolicySource.OWNER:
          return CrPolicyIndicatorType.OWNER;
        case chrome.settingsPrivate.PolicySource.USER_POLICY:
          return CrPolicyIndicatorType.USER_POLICY;
        case chrome.settingsPrivate.PolicySource.DEVICE_POLICY:
          return CrPolicyIndicatorType.DEVICE_POLICY;
        case chrome.settingsPrivate.PolicySource.EXTENSION:
          return CrPolicyIndicatorType.EXTENSION;
      }
    }
    return CrPolicyIndicatorType.NONE;
  },
};
