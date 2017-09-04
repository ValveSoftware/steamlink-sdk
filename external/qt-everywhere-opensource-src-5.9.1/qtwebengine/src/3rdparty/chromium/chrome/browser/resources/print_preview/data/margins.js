// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a Margins object that holds four margin values in points.
   * @param {number} top The top margin in pts.
   * @param {number} right The right margin in pts.
   * @param {number} bottom The bottom margin in pts.
   * @param {number} left The left margin in pts.
   * @constructor
   */
  function Margins(top, right, bottom, left) {
    /**
     * Backing store for the margin values in points.
     * @type {!Object<
     *     !print_preview.ticket_items.CustomMargins.Orientation, number>}
     * @private
     */
    this.value_ = {};
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.TOP] = top;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.RIGHT] =
        right;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM] =
        bottom;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.LEFT] =
        left;
  };

  /**
   * Parses a margins object from the given serialized state.
   * @param {Object} state Serialized representation of the margins created by
   *     the {@code serialize} method.
   * @return {!print_preview.Margins} New margins instance.
   */
  Margins.parse = function(state) {
    return new print_preview.Margins(
        state[print_preview.ticket_items.CustomMargins.Orientation.TOP] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.RIGHT] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.LEFT] || 0);
  };

  Margins.prototype = {
    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin value to get.
     * @return {number} Value of the margin of the given orientation.
     */
    get: function(orientation) {
      return this.value_[orientation];
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to set.
     * @param {number} value Updated value of the margin in points to modify.
     * @return {!print_preview.Margins} A new copy of |this| with the
     *     modification made to the specified margin.
     */
    set: function(orientation, value) {
      var newValue = this.clone_();
      newValue[orientation] = value;
      return new Margins(
          newValue[print_preview.ticket_items.CustomMargins.Orientation.TOP],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.RIGHT],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.LEFT]);
    },

    /**
     * @param {print_preview.Margins} other The other margins object to compare
     *     against.
     * @return {boolean} Whether this margins object is equal to another.
     */
    equals: function(other) {
      if (other == null) {
        return false;
      }
      for (var orientation in this.value_) {
        if (this.value_[orientation] != other.value_[orientation]) {
          return false;
        }
      }
      return true;
    },

    /** @return {Object} A serialized representation of the margins. */
    serialize: function() {
      return this.clone_();
    },

    /**
     * @return {Object} Cloned state of the margins.
     * @private
     */
    clone_: function() {
      var clone = {};
      for (var o in this.value_) {
        clone[o] = this.value_[o];
      }
      return clone;
    }
  };

  // Export
  return {
    Margins: Margins
  };
});
