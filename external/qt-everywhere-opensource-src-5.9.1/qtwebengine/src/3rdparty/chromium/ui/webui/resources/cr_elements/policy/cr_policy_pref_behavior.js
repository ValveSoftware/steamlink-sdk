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
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED &&
           pref.controlledBy != chrome.settingsPrivate.ControlledBy.EXTENSION;
  },

  /**
   * @param {chrome.settingsPrivate.ControlledBy} controlledBy
   * @param {chrome.settingsPrivate.Enforcement} enforcement
   * @return {CrPolicyIndicatorType} The indicator type based on |controlledBy|
   *     and |enforcement|.
   */
  getIndicatorType: function(controlledBy, enforcement) {
    if (enforcement == chrome.settingsPrivate.Enforcement.RECOMMENDED)
      return CrPolicyIndicatorType.RECOMMENDED;
    if (enforcement == chrome.settingsPrivate.Enforcement.ENFORCED) {
      switch (controlledBy) {
        case chrome.settingsPrivate.ControlledBy.PRIMARY_USER:
          return CrPolicyIndicatorType.PRIMARY_USER;
        case chrome.settingsPrivate.ControlledBy.OWNER:
          return CrPolicyIndicatorType.OWNER;
        case chrome.settingsPrivate.ControlledBy.USER_POLICY:
          return CrPolicyIndicatorType.USER_POLICY;
        case chrome.settingsPrivate.ControlledBy.DEVICE_POLICY:
          return CrPolicyIndicatorType.DEVICE_POLICY;
      }
    }
    return CrPolicyIndicatorType.NONE;
  },
};
