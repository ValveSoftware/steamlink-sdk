// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */
(function() {
'use strict';

/**
 * Polymer class definition for 'cr-network-list-custom-item'.
 * Note: on-tapaction on this item fires "custom-item-selected" event
 * with |itemState| as data.
 */
Polymer({
  is: 'cr-network-list-custom-item',

  properties: {
    /**
     * Custom entry properties.
     * @type {?CrNetworkList.CustomItemState}
     */
    itemState: {
      type: Object,
      value: null,
    },
  },

  listeners: {
    'tap' : 'onTap_'
  },

  /**
   * @param {!Event} event
   */
  onTap_: function(event) {
    if (!this.itemState)
      return;

    this.fire("custom-item-selected", this.itemState);
  },

  /**
   * TODO(stevenjb): Replace getText with a proper localization function that
   * handles string substitution.
   * Performs argument substitution, replacing $1, $2, etc in 'text' with
   * corresponding entries in |args|.
   * @param {string} text The string to perform the substitution on.
   * @param {?Array<string>=} opt_args The arguments to replace $1, $2, etc
   *                                   with.
   */
  getText_: function(text, opt_args) {
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
  },
});
})();
