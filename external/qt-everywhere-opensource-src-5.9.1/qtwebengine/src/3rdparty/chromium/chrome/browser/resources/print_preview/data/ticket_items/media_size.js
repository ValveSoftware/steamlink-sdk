// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Media size ticket item.
   * @param {!print_preview.AppState} appState App state used to persist media
   *     size selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used to determine if a destination has the media size capability.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.ticket_items.MarginsType} marginsType Reset when
   *     landscape value changes.
   * @param {!print_preview.ticket_items.CustomMargins} customMargins Reset when
   *     landscape value changes.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function MediaSize(
      appState, destinationStore, documentInfo, marginsType, customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.MEDIA_SIZE,
        destinationStore,
        documentInfo);

    /**
     * Margins ticket item. Reset when this item changes.
     * @private {!print_preview.ticket_items.MarginsType}
     */
    this.marginsType_ = marginsType;

    /**
     * Custom margins ticket item. Reset when this item changes.
     * @private {!print_preview.ticket_items.CustomMargins}
     */
    this.customMargins_ = customMargins;
  };

  MediaSize.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      if (!this.isCapabilityAvailable()) {
        return false;
      }
      return this.capability.option.some(function(option) {
        return option.width_microns == value.width_microns &&
               option.height_microns == value.height_microns &&
               option.is_continuous_feed == value.is_continuous_feed &&
               option.vendor_id == value.vendor_id;
      });
    },

    /** @override */
    isCapabilityAvailable: function() {
      var knownSizeToSaveAsPdf =
          (!this.getDocumentInfoInternal().isModifiable ||
           this.getDocumentInfoInternal().hasCssMediaStyles) &&
          this.getSelectedDestInternal() &&
          this.getSelectedDestInternal().id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
      return !knownSizeToSaveAsPdf && !!this.capability;
    },

    /** @override */
    isValueEqual: function(value) {
      var myValue = this.getValue();
      return myValue.width_microns == value.width_microns &&
             myValue.height_microns == value.height_microns &&
             myValue.is_continuous_feed == value.is_continuous_feed &&
             myValue.vendor_id == value.vendor_id;
    },

    /** @return {Object} Media size capability of the selected destination. */
    get capability() {
      var destination = this.getSelectedDestInternal();
      return (destination &&
              destination.capabilities &&
              destination.capabilities.printer &&
              destination.capabilities.printer.media_size) ||
             null;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var defaultOptions = this.capability.option.filter(function(option) {
        return option.is_default;
      });
      return defaultOptions.length > 0 ? defaultOptions[0] : null;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return {};
    },

    /** @override */
    updateValueInternal: function(value) {
      var updateMargins = !this.isValueEqual(value);
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (updateMargins) {
        // Reset the user set margins when media size changes.
        this.marginsType_.updateValue(
            print_preview.ticket_items.MarginsType.Value.DEFAULT);
        this.customMargins_.updateValue(null);
      }
    }
  };

  // Export
  return {
    MediaSize: MediaSize
  };
});
