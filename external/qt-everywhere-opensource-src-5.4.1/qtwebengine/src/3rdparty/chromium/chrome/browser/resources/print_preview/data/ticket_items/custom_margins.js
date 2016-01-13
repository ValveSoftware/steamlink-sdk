// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Custom page margins ticket item whose value is a
   * {@code print_preview.Margins}.
   * @param {!print_preview.AppState} appState App state used to persist custom
   *     margins.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CustomMargins(appState, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.CUSTOM_MARGINS,
        null /*destinationStore*/,
        documentInfo);
  };

  /**
   * Enumeration of the orientations of margins.
   * @enum {string}
   */
  CustomMargins.Orientation = {
    TOP: 'top',
    RIGHT: 'right',
    BOTTOM: 'bottom',
    LEFT: 'left'
  };

  /**
   * Mapping of a margin orientation to its opposite.
   * @type {!Object.<!CustomMargins.Orientation, !CustomMargins.Orientation>}
   * @private
   */
  CustomMargins.OppositeOrientation_ = {};
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.TOP] =
      CustomMargins.Orientation.BOTTOM;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.RIGHT] =
      CustomMargins.Orientation.LEFT;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.BOTTOM] =
      CustomMargins.Orientation.TOP;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.LEFT] =
      CustomMargins.Orientation.RIGHT;

  /**
   * Minimum distance in points that two margins can be separated by.
   * @type {number}
   * @const
   * @private
   */
  CustomMargins.MINIMUM_MARGINS_DISTANCE_ = 72; // 1 inch.

  CustomMargins.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      for (var key in CustomMargins.Orientation) {
        var o = CustomMargins.Orientation[key];
        var max = this.getMarginMax_(
            o, margins.get(CustomMargins.OppositeOrientation_[o]));
        if (margins.get(o) > max || margins.get(o) < 0) {
          return false;
        }
      }
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    isValueEqual: function(value) {
      return this.getValue().equals(value);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to get the maximum value for.
     * @return {number} Maximum value in points of the specified margin.
     */
    getMarginMax: function(orientation) {
      var oppositeOrient = CustomMargins.OppositeOrientation_[orientation];
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      return this.getMarginMax_(orientation, margins.get(oppositeOrient));
    },

    /** @override */
    updateValue: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      if (margins != null) {
        margins = new print_preview.Margins(
            Math.round(margins.get(CustomMargins.Orientation.TOP)),
            Math.round(margins.get(CustomMargins.Orientation.RIGHT)),
            Math.round(margins.get(CustomMargins.Orientation.BOTTOM)),
            Math.round(margins.get(CustomMargins.Orientation.LEFT)));
      }
      print_preview.ticket_items.TicketItem.prototype.updateValue.call(
          this, margins);
    },

    /**
     * Updates the specified margin in points while keeping the value within
     * a maximum and minimum.
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to update.
     * @param {number} value Updated margin value in points.
     */
    updateMargin: function(orientation, value) {
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      var oppositeOrientation = CustomMargins.OppositeOrientation_[orientation];
      var max =
          this.getMarginMax_(orientation, margins.get(oppositeOrientation));
      value = Math.max(0, Math.min(max, value));
      this.updateValue(margins.set(orientation, value));
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
             new print_preview.Margins(72, 72, 72, 72);
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
             new print_preview.Margins(72, 72, 72, 72);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies which margin to get the maximum value of.
     * @param {number} oppositeMargin Value of the margin in points
     *     opposite the specified margin.
     * @return {number} Maximum value in points of the specified margin.
     * @private
     */
    getMarginMax_: function(orientation, oppositeMargin) {
      var max;
      if (orientation == CustomMargins.Orientation.TOP ||
          orientation == CustomMargins.Orientation.BOTTOM) {
        max = this.getDocumentInfoInternal().pageSize.height - oppositeMargin -
            CustomMargins.MINIMUM_MARGINS_DISTANCE_;
      } else {
        max = this.getDocumentInfoInternal().pageSize.width - oppositeMargin -
            CustomMargins.MINIMUM_MARGINS_DISTANCE_;
      }
      return Math.round(max);
    }
  };

  // Export
  return {
    CustomMargins: CustomMargins
  };
});
