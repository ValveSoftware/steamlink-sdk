// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * An object that represents a user modifiable item in a print ticket. Each
   * ticket item has a value which can be set by the user. Ticket items can also
   * be unavailable for modifying if the print destination doesn't support it or
   * if other ticket item constraints are not met.
   * @param {print_preview.AppState=} appState Application state model to update
   *     when ticket items update.
   * @param {print_preview.AppState.Field=} field Field of the app state to
   *     update when ticket item is updated.
   * @param {print_preview.DestinationStore=} destinationStore Used listen for
   *     changes in the currently selected destination's capabilities. Since
   *     this is a common dependency of ticket items, it's handled in the base
   *     class.
   * @param {print_preview.DocumentInfo=} documentInfo Used to listen for
   *     changes in the document. Since this is a common dependency of ticket
   *     items, it's handled in the base class.
   * @constructor
   */
  function TicketItem(appState, field, destinationStore, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Application state model to update when ticket items update.
     * @type {print_preview.AppState}
     * @private
     */
    this.appState_ = appState || null;

    /**
     * Field of the app state to update when ticket item is updated.
     * @type {print_preview.AppState.Field}
     * @private
     */
    this.field_ = field || null;

    /**
     * Used listen for changes in the currently selected destination's
     * capabilities.
     * @type {print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore || null;

    /**
     * Used to listen for changes in the document.
     * @type {print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo || null;

    /**
     * Backing store of the print ticket item.
     * @type {Object}
     * @private
     */
    this.value_ = null;

    /**
     * Keeps track of event listeners for the ticket item.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventHandlers_();
  };

  /**
   * Event types dispatched by this class.
   * @enum {string}
   */
  TicketItem.EventType = {
    CHANGE: 'print_preview.ticket_items.TicketItem.CHANGE'
  };

  TicketItem.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Determines whether a given value is valid for the ticket item.
     * @param {Object} value The value to check for validity.
     * @return {boolean} Whether the given value is valid for the ticket item.
     */
    wouldValueBeValid: function(value) {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {boolean} Whether the print destination capability is available.
     */
    isCapabilityAvailable: function() {
      throw Error('Abstract method not overridden');
    },

    /** @return {!Object} The value of the ticket item. */
    getValue: function() {
      if (this.isCapabilityAvailable()) {
        if (this.value_ == null) {
          return this.getDefaultValueInternal();
        } else {
          return this.value_;
        }
      } else {
        return this.getCapabilityNotAvailableValueInternal();
      }
    },

    /** @return {boolean} Whether the ticket item was modified by the user. */
    isUserEdited: function() {
      return this.value_ != null;
    },

    /** @return {boolean} Whether the ticket item's value is valid. */
    isValid: function() {
      if (!this.isUserEdited()) {
        return true;
      }
      return this.wouldValueBeValid(this.value_);
    },

    /**
     * @param {Object} value Value to compare to the value of this ticket item.
     * @return {boolean} Whether the given value is equal to the value of the
     *     ticket item.
     */
    isValueEqual: function(value) {
      return this.getValue() == value;
    },

    /** @param {!Object} Value to set as the value of the ticket item. */
    updateValue: function(value) {
      // Use comparison with capabilities for event.
      var sendUpdateEvent = !this.isValueEqual(value);
      // Don't lose requested value if capability is not available.
      this.updateValueInternal(value);
      if (this.appState_) {
        this.appState_.persistField(this.field_, value);
      }
      if (sendUpdateEvent)
        cr.dispatchSimpleEvent(this, TicketItem.EventType.CHANGE);
    },

    /**
     * @return {!Object} Default value of the ticket item if no value was set by
     *     the user.
     * @protected
     */
    getDefaultValueInternal: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {!Object} Default value of the ticket item if the capability is
     *     not available.
     * @protected
     */
    getCapabilityNotAvailableValueInternal: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {!EventTracker} Event tracker to keep track of events from
     *     dependencies.
     * @protected
     */
    getTrackerInternal: function() {
      return this.tracker_;
    },

    /**
     * @return {print_preview.Destination} Selected destination from the
     *     destination store, or {@code null} if no destination is selected.
     * @protected
     */
    getSelectedDestInternal: function() {
      return this.destinationStore_ ?
          this.destinationStore_.selectedDestination : null;
    },

    /**
     * @return {print_preview.DocumentInfo} Document data model.
     * @protected
     */
    getDocumentInfoInternal: function() {
      return this.documentInfo_;
    },

    /**
     * Dispatches a CHANGE event.
     * @protected
     */
    dispatchChangeEventInternal: function() {
      cr.dispatchSimpleEvent(
          this, print_preview.ticket_items.TicketItem.EventType.CHANGE);
    },

    /**
     * Updates the value of the ticket item without dispatching any events or
     * persisting the value.
     * @protected
     */
    updateValueInternal: function(value) {
      this.value_ = value;
    },

    /**
     * Adds event handlers for this class.
     * @private
     */
    addEventHandlers_: function() {
      if (this.destinationStore_) {
        this.tracker_.add(
            this.destinationStore_,
            print_preview.DestinationStore.EventType.
                SELECTED_DESTINATION_CAPABILITIES_READY,
            this.dispatchChangeEventInternal.bind(this));
      }
      if (this.documentInfo_) {
        this.tracker_.add(
            this.documentInfo_,
            print_preview.DocumentInfo.EventType.CHANGE,
            this.dispatchChangeEventInternal.bind(this));
      }
    },
  };

  // Export
  return {
    TicketItem: TicketItem
  };
});
