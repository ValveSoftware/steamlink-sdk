// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 */

/** @polymerBehavior */
var CrPolicyNetworkBehavior = {
  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a policy
   *     (either enforced or recommended).
   */
  isNetworkPolicyControlled: function(property) {
    // If the property is not a dictionary, or does not have an Effective
    // sub-property set, then the property is not policy controlled.
    if (typeof property != 'object' || !property.Effective)
      return false;
    // Enforced
    var effective = property.Effective;
    if (effective == 'UserPolicy' || effective == 'DevicePolicy')
      return true;
    // Recommended
    if (property.UserPolicy || property.DevicePolicy)
      return true;
    // Neither enforced nor recommended = not policy controlled.
    return false;
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced: function(property) {
    if (!this.isNetworkPolicyControlled(property))
      return false;
    // If the property has a UserEditable sub-property, that determines whether
    // or not it is editable (not enforced).
    if (typeof property.UserEditable != 'undefined')
      return !property.UserEditable;

    // Otherwise if the property has a DeviceEditable sub-property, check that.
    if (typeof property.DeviceEditable != 'undefined')
      return !property.DeviceEditable;

    // If no 'Editable' sub-property exists, the policy value is enforced.
    return true;
  },
};
