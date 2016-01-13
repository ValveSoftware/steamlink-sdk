// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Color ticket item whose value is a {@code boolean} that indicates whether
   * the document should be printed in color.
   * @param {!print_preview.AppState} appState App state persistence object to
   *     save the state of the color selection.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether color printing should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Color(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_COLOR_ENABLED,
        destinationStore);
  };

  /*
   * @private {!Array.<string>} List of capability types considered color.
   * @const
   */
  Color.COLOR_TYPES_ = ['STANDARD_COLOR', 'CUSTOM_COLOR'];

  /*
   * @private {!Array.<string>} List of capability types considered monochrome.
   * @const
   */
  Color.MONOCHROME_TYPES_ = ['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'];

  Color.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var capability = this.capability;
      if (!capability) {
        return false;
      }
      var hasColor = false;
      var hasMonochrome = false;
      capability.option.forEach(function(option) {
        hasColor = hasColor || (Color.COLOR_TYPES_.indexOf(option.type) >= 0);
        hasMonochrome = hasMonochrome ||
            (Color.MONOCHROME_TYPES_.indexOf(option.type) >= 0);
      });
      return hasColor && hasMonochrome;
    },

    /** @return {Object} Color capability of the selected destination. */
    get capability() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.color) ||
             null;
    },

    /** @return {Object} Color option corresponding to the current value. */
    getSelectedOption: function() {
      var capability = this.capability;
      var options = capability ? capability.option : null;
      if (options) {
        var typesToLookFor =
            this.getValue() ? Color.COLOR_TYPES_ : Color.MONOCHROME_TYPES_;
        for (var i = 0; i < typesToLookFor.length; i++) {
          var matchingOptions = options.filter(function(option) {
            return option.type == typesToLookFor[i];
          });
          if (matchingOptions.length > 0) {
            return matchingOptions[0];
          }
        }
      }
      return null;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var capability = this.capability;
      var defaultOption = capability ?
          this.getDefaultColorOption_(capability.option) : null;
      return defaultOption &&
          (Color.COLOR_TYPES_.indexOf(defaultOption.type) >= 0);
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      // TODO(rltoscano): Get rid of this check based on destination ID. These
      // destinations should really update their CDDs to have only one color
      // option that has type 'STANDARD_COLOR'.
      var dest = this.getSelectedDestInternal();
      if (dest) {
        if (dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
            dest.id == print_preview.Destination.GooglePromotedId.FEDEX ||
            dest.type == print_preview.Destination.Type.MOBILE) {
          return true;
        }
      }
      return this.getDefaultValueInternal();
    },

    /**
     * @param options {!Array.<!Object.<{type: string=, is_default: boolean=}>>
     * @return {Object.<{type: string=, is_default: boolean=}>} Default color
     *     option of the given list.
     * @private
     */
    getDefaultColorOption_: function(options) {
      var defaultOptions = options.filter(function(option) {
        return option.is_default;
      });
      return (defaultOptions.length == 0) ? null : defaultOptions[0];
    }
  };

  // Export
  return {
    Color: Color
  };
});
