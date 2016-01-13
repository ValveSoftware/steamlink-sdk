// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Margins type ticket item whose value is a
   * {@link print_preview.ticket_items.MarginsType.Value} that indicates what
   * predefined margins type to use.
   * @param {!print_preview.AppState} appState App state persistence object to
   *     save the state of the margins type selection.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.CustomMargins} customMargins Custom margins ticket
   *     item, used to write when margins type changes.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function MarginsType(appState, documentInfo, customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.MARGINS_TYPE,
        null /*destinationStore*/,
        documentInfo);

    /**
     * Custom margins ticket item, used to write when margins type changes.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;
  };

  /**
   * Enumeration of margin types. Matches enum MarginType in
   * printing/print_job_constants.h.
   * @enum {number}
   */
  MarginsType.Value = {
    DEFAULT: 0,
    NO_MARGINS: 1,
    MINIMUM: 2,
    CUSTOM: 3
  };

  MarginsType.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return MarginsType.Value.DEFAULT;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return MarginsType.Value.DEFAULT;
    },

    /** @override */
    updateValueInternal: function(value) {
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (this.isValueEqual(
          print_preview.ticket_items.MarginsType.Value.CUSTOM)) {
        // If CUSTOM, set the value of the custom margins so that it won't be
        // overridden by the default value.
        this.customMargins_.updateValue(this.customMargins_.getValue());
      }
    }
  };

  // Export
  return {
    MarginsType: MarginsType
  };
});
