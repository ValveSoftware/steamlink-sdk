// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): Maybe clear print ticket when destination changes. Or
  // better yet, carry over any print ticket state that is possible. I.e. if
  // destination changes, the new destination might not support duplex anymore,
  // so we should clear the ticket's isDuplexEnabled state.

  /**
   * Storage of the print ticket and document statistics. Dispatches events when
   * the contents of the print ticket or document statistics change. Also
   * handles validation of the print ticket against destination capabilities and
   * against the document.
   * @param {!print_preview.DestinationStore} destinationStore Used to
   *     understand which printer is selected.
   * @param {!print_preview.AppState} appState Print preview application state.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function PrintTicketStore(destinationStore, appState, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Destination store used to understand which printer is selected.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * App state used to persist and load ticket values.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Printing capabilities of Chromium and the currently selected destination.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = new print_preview.CapabilitiesHolder();

    /**
     * Current measurement system. Used to work with margin measurements.
     * @type {!print_preview.MeasurementSystem}
     * @private
     */
    this.measurementSystem_ = new print_preview.MeasurementSystem(
        ',', '.', print_preview.MeasurementSystem.UnitType.IMPERIAL);

    /**
     * Collate ticket item.
     * @type {!print_preview.ticket_items.Collate}
     * @private
     */
    this.collate_ = new print_preview.ticket_items.Collate(
        this.appState_, this.destinationStore_);

    /**
     * Color ticket item.
     * @type {!print_preview.ticket_items.Color}
     * @private
     */
    this.color_ = new print_preview.ticket_items.Color(
        this.appState_, this.destinationStore_);

    /**
     * Copies ticket item.
     * @type {!print_preview.ticket_items.Copies}
     * @private
     */
    this.copies_ =
        new print_preview.ticket_items.Copies(this.destinationStore_);

    /**
     * Duplex ticket item.
     * @type {!print_preview.ticket_items.Duplex}
     * @private
     */
    this.duplex_ = new print_preview.ticket_items.Duplex(
        this.appState_, this.destinationStore_);

    /**
     * Page range ticket item.
     * @type {!print_preview.ticket_items.PageRange}
     * @private
     */
    this.pageRange_ =
        new print_preview.ticket_items.PageRange(this.documentInfo_);

    /**
     * Custom margins ticket item.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = new print_preview.ticket_items.CustomMargins(
        this.appState_, this.documentInfo_);

    /**
     * Margins type ticket item.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = new print_preview.ticket_items.MarginsType(
        this.appState_, this.documentInfo_, this.customMargins_);

    /**
     * Media size ticket item.
     * @type {!print_preview.ticket_items.MediaSize}
     * @private
     */
    this.mediaSize_ = new print_preview.ticket_items.MediaSize(
        this.appState_,
        this.destinationStore_,
        this.documentInfo_,
        this.marginsType_,
        this.customMargins_);

    /**
     * Landscape ticket item.
     * @type {!print_preview.ticket_items.Landscape}
     * @private
     */
    this.landscape_ = new print_preview.ticket_items.Landscape(
        this.appState_,
        this.destinationStore_,
        this.documentInfo_,
        this.marginsType_,
        this.customMargins_);

    /**
     * Header-footer ticket item.
     * @type {!print_preview.ticket_items.HeaderFooter}
     * @private
     */
    this.headerFooter_ = new print_preview.ticket_items.HeaderFooter(
        this.appState_,
        this.documentInfo_,
        this.marginsType_,
        this.customMargins_);

    /**
     * Fit-to-page ticket item.
     * @type {!print_preview.ticket_items.FitToPage}
     * @private
     */
    this.fitToPage_ = new print_preview.ticket_items.FitToPage(
        this.documentInfo_, this.destinationStore_);

    /**
     * Print CSS backgrounds ticket item.
     * @type {!print_preview.ticket_items.CssBackground}
     * @private
     */
    this.cssBackground_ = new print_preview.ticket_items.CssBackground(
        this.appState_, this.documentInfo_);

    /**
     * Print selection only ticket item.
     * @type {!print_preview.ticket_items.SelectionOnly}
     * @private
     */
    this.selectionOnly_ =
        new print_preview.ticket_items.SelectionOnly(this.documentInfo_);

    /**
     * Keeps track of event listeners for the print ticket store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Whether the print preview has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the print ticket store.
   * @enum {string}
   */
  PrintTicketStore.EventType = {
    CAPABILITIES_CHANGE: 'print_preview.PrintTicketStore.CAPABILITIES_CHANGE',
    DOCUMENT_CHANGE: 'print_preview.PrintTicketStore.DOCUMENT_CHANGE',
    INITIALIZE: 'print_preview.PrintTicketStore.INITIALIZE',
    TICKET_CHANGE: 'print_preview.PrintTicketStore.TICKET_CHANGE'
  };

  PrintTicketStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Whether the print preview has been initialized.
     * @type {boolean}
     */
    get isInitialized() {
      return this.isInitialized_;
    },

    get collate() {
      return this.collate_;
    },

    get color() {
      return this.color_;
    },

    get copies() {
      return this.copies_;
    },

    get cssBackground() {
      return this.cssBackground_;
    },

    get customMargins() {
      return this.customMargins_;
    },

    get duplex() {
      return this.duplex_;
    },

    get fitToPage() {
      return this.fitToPage_;
    },

    get headerFooter() {
      return this.headerFooter_;
    },

    get mediaSize() {
      return this.mediaSize_;
    },

    get landscape() {
      return this.landscape_;
    },

    get marginsType() {
      return this.marginsType_;
    },

    get pageRange() {
      return this.pageRange_;
    },

    get selectionOnly() {
      return this.selectionOnly_;
    },

    /**
     * @return {!print_preview.MeasurementSystem} Measurement system of the
     *     local system.
     */
    get measurementSystem() {
      return this.measurementSystem_;
    },

    /**
     * Initializes the print ticket store. Dispatches an INITIALIZE event.
     * @param {string} thousandsDelimeter Delimeter of the thousands place.
     * @param {string} decimalDelimeter Delimeter of the decimal point.
     * @param {!print_preview.MeasurementSystem.UnitType} unitType Type of unit
     *     of the local measurement system.
     * @param {boolean} selectionOnly Whether only selected content should be
     *     printed.
     */
    init: function(
        thousandsDelimeter, decimalDelimeter, unitType, selectionOnly) {
      this.measurementSystem_.setSystem(thousandsDelimeter, decimalDelimeter,
                                        unitType);
      this.selectionOnly_.updateValue(selectionOnly);

      // Initialize ticket with user's previous values.
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLOR_ENABLED)) {
        this.color_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLOR_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_DUPLEX_ENABLED)) {
        this.duplex_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_DUPLEX_ENABLED));
      }
      if (this.appState_.hasField(print_preview.AppState.Field.MEDIA_SIZE)) {
        this.mediaSize_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.MEDIA_SIZE));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_LANDSCAPE_ENABLED)) {
        this.landscape_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_LANDSCAPE_ENABLED));
      }
      // Initialize margins after landscape because landscape may reset margins.
      if (this.appState_.hasField(print_preview.AppState.Field.MARGINS_TYPE)) {
        this.marginsType_.updateValue(
            this.appState_.getField(print_preview.AppState.Field.MARGINS_TYPE));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.CUSTOM_MARGINS)) {
        this.customMargins_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.CUSTOM_MARGINS));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_HEADER_FOOTER_ENABLED)) {
        this.headerFooter_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_HEADER_FOOTER_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLLATE_ENABLED)) {
        this.collate_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLLATE_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED)) {
        this.cssBackground_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED));
      }
    },

    /**
     * @return {boolean} {@code true} if the stored print ticket is valid,
     *     {@code false} otherwise.
     */
    isTicketValid: function() {
      return this.isTicketValidForPreview() &&
          (!this.copies_.isCapabilityAvailable() || this.copies_.isValid()) &&
          (!this.pageRange_.isCapabilityAvailable() ||
              this.pageRange_.isValid());
    },

    /** @return {boolean} Whether the ticket is valid for preview generation. */
    isTicketValidForPreview: function() {
      return (!this.marginsType_.isCapabilityAvailable() ||
              !this.marginsType_.isValueEqual(
                  print_preview.ticket_items.MarginsType.Value.CUSTOM) ||
              this.customMargins_.isValid());
    },

    /**
     * Creates an object that represents a Google Cloud Print print ticket.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @return {!Object} Google Cloud Print print ticket.
     */
    createPrintTicket: function(destination) {
      assert(!destination.isLocal || destination.isPrivet,
             'Trying to create a Google Cloud Print print ticket for a local ' +
                 ' non-privet destination');

      assert(destination.capabilities,
             'Trying to create a Google Cloud Print print ticket for a ' +
                 'destination with no print capabilities');
      var cjt = {
        version: '1.0',
        print: {}
      };
      if (this.collate.isCapabilityAvailable() && this.collate.isUserEdited()) {
        cjt.print.collate = {collate: this.collate.getValue()};
      }
      if (this.color.isCapabilityAvailable() && this.color.isUserEdited()) {
        var selectedOption = this.color.getSelectedOption();
        if (!selectedOption) {
          console.error('Could not find correct color option');
        } else {
          cjt.print.color = {type: selectedOption.type};
          if (selectedOption.hasOwnProperty('vendor_id')) {
            cjt.print.color.vendor_id = selectedOption.vendor_id;
          }
        }
      }
      if (this.copies.isCapabilityAvailable() && this.copies.isUserEdited()) {
        cjt.print.copies = {copies: this.copies.getValueAsNumber()};
      }
      if (this.duplex.isCapabilityAvailable() && this.duplex.isUserEdited()) {
        cjt.print.duplex =
            {type: this.duplex.getValue() ? 'LONG_EDGE' : 'NO_DUPLEX'};
      }
      if (this.mediaSize.isCapabilityAvailable()) {
        var value = this.mediaSize.getValue();
        cjt.print.media_size = {
          width_microns: value.width_microns,
          height_microns: value.height_microns,
          is_continuous_feed: value.is_continuous_feed,
          vendor_id: value.vendor_id
        };
      }
      if (this.landscape.isCapabilityAvailable() &&
          this.landscape.isUserEdited()) {
        cjt.print.page_orientation =
            {type: this.landscape.getValue() ? 'LANDSCAPE' : 'PORTRAIT'};
      }
      return JSON.stringify(cjt);
    },

    /**
     * Adds event listeners for the print ticket store.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));

      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onSelectedDestinationCapabilitiesReady_.bind(this));

      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              CACHED_SELECTED_DESTINATION_INFO_READY,
          this.onSelectedDestinationCapabilitiesReady_.bind(this));

      // TODO(rltoscano): Print ticket store shouldn't be re-dispatching these
      // events, the consumers of the print ticket store events should listen
      // for the events from document info instead. Will move this when
      // consumers are all migrated.
      this.tracker_.add(
          this.documentInfo_,
          print_preview.DocumentInfo.EventType.CHANGE,
          this.onDocumentInfoChange_.bind(this));
    },

    /**
     * Called when the destination selected.
     * @private
     */
    onDestinationSelect_: function() {
      // Reset user selection for certain ticket items.
      if (this.capabilitiesHolder_.get() != null) {
        this.customMargins_.updateValue(null);
        if (this.marginsType_.getValue() ==
            print_preview.ticket_items.MarginsType.Value.CUSTOM) {
          this.marginsType_.updateValue(
              print_preview.ticket_items.MarginsType.Value.DEFAULT);
        }
      }
    },

    /**
     * Called when the capabilities of the selected destination are ready.
     * @private
     */
    onSelectedDestinationCapabilitiesReady_: function() {
      var caps = this.destinationStore_.selectedDestination.capabilities;
      var isFirstUpdate = this.capabilitiesHolder_.get() == null;
      this.capabilitiesHolder_.set(caps);
      if (isFirstUpdate) {
        this.isInitialized_ = true;
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.INITIALIZE);
      } else {
        cr.dispatchSimpleEvent(
            this, PrintTicketStore.EventType.CAPABILITIES_CHANGE);
      }
    },

    /**
     * Called when document data model has changed. Dispatches a print ticket
     * store event.
     * @private
     */
    onDocumentInfoChange_: function() {
      cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.DOCUMENT_CHANGE);
    },
  };

  // Export
  return {
    PrintTicketStore: PrintTicketStore
  };
});
