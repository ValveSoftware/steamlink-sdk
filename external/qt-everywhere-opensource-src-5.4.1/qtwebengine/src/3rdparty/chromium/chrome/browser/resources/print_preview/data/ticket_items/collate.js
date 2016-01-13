// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Collate ticket item whose value is a {@code boolean} that indicates whether
   * collation is enabled.
   * @param {!print_preview.AppState} appState App state used to persist collate
   *     selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the collate capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Collate(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_COLLATE_ENABLED,
        destinationStore);
  };

  Collate.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.getCollateCapability_();
    },

    /** @override */
    getDefaultValueInternal: function() {
      var capability = this.getCollateCapability_();
      return capability.hasOwnProperty('default') ? capability.default : true;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return true;
    },

    /**
     * @return {Object} Collate capability of the selected destination.
     * @private
     */
    getCollateCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.collate) ||
             null;
    }
  };

  // Export
  return {
    Collate: Collate
  };
});
