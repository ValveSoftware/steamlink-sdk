// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * DPI ticket item.
   * @param {!print_preview.AppState} appState App state used to persist DPI
   *     selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used to determine if a destination has the DPI capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Dpi(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.DPI,
        destinationStore);
  };

  Dpi.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      if (!this.isCapabilityAvailable())
        return false;
      return this.capability.option.some(function(option) {
        return option.horizontal_dpi == value.horizontal_dpi &&
               option.vertical_dpi == value.vertical_dpi &&
               option.vendor_id == value.vendor_id;
      });
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.capability &&
             !!this.capability.option &&
             this.capability.option.length > 1;
    },

    /** @override */
    isValueEqual: function(value) {
      var myValue = this.getValue();
      return myValue.horizontal_dpi == value.horizontal_dpi &&
             myValue.vertical_dpi == value.vertical_dpi &&
             myValue.vendor_id == value.vendor_id;
    },

    /** @return {Object} DPI capability of the selected destination. */
    get capability() {
      var destination = this.getSelectedDestInternal();
      return (destination &&
              destination.capabilities &&
              destination.capabilities.printer &&
              destination.capabilities.printer.dpi) ||
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
    }
  };

  // Export
  return {
    Dpi: Dpi
  };
});
