// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

(function() {

/**
 * Polymer class definition for 'cr-network-list'.
 * TODO(stevenjb): Update with iron-list(?) once implemented in Polymer 1.0.
 */
Polymer({
  is: 'cr-network-list',

  properties: {
    /**
     * The maximum height in pixels for the list.
     */
    maxHeight: {
      type: Number,
      value: 1000,
      observer: 'maxHeightChanged_'
    },

    /**
     * Determines how the list item will be displayed:
     *  'visible' - displays the network icon (with strength) and name
     *  'known' - displays the visible info along with a toggle icon for the
     *      preferred status and a remove button.
     */
    listType: {
      type: String,
      value: 'visible'
    },

    /**
     * The list of network state properties for the items to display.
     * See <cr-network-list-network-item/> for details.
     *
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networks: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * The list of custom state properties for the items to display.
     * See <cr-network-list-custom-item/> for details.
     *
     * @type {!Array<Object>}
     */
    customItems: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * True if the list is opened.
     */
    opened: {
      type: Boolean,
      value: true
    },

    /**
     * Shows all buttons from the list items.
     */
    showButtons: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Polymer maxHeight changed method.
   */
  maxHeightChanged_: function() {
    this.$.container.style.maxHeight = this.maxHeight + 'px';
  },

  /**
   * Event triggered when a list item is tapped.
   * @param {!{model: {item: !CrOnc.NetworkStateProperties}}} event
   * @private
   */
  onTap_: function(event) {
    this.fire('selected', event.model.item);
  },
});
})();
