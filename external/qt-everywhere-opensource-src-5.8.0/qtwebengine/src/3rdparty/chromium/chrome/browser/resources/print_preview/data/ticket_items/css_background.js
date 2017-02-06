// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item whose value is a {@code boolean} that represents whether to
   * print CSS backgrounds.
   * @param {!print_preview.AppState} appState App state to persist CSS
   *     background value.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CssBackground(appState, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED,
        null /*destinationStore*/,
        documentInfo);
  };

  CssBackground.prototype = {
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
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    CssBackground: CssBackground
  };
});
