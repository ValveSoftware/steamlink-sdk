// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!print_preview.NativeLayer} nativeLayer Used to fetch local print
   *     destinations.
   * @param {!print_preview.UserInfo} userInfo User information repository.
   * @param {!print_preview.AppState} appState Application state.
   * @param {!print_preview.Metrics} metrics Metrics.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore(nativeLayer, userInfo, appState, metrics) {
    cr.EventTarget.call(this);

    /**
     * Used to fetch local print destinations.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * User information repository.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = userInfo;

    /**
     * Used to load and persist the selected destination.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Used to track metrics.
     * @type {!print_preview.AppState}
     * @private
     */
    this.metrics_ = metrics;

    /**
     * Internal backing store for the data store.
     * @type {!Array.<!print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Cache used for constant lookup of destinations by origin and id.
     * @type {object.<string, !print_preview.Destination>}
     * @private
     */
    this.destinationMap_ = {};

    /**
     * Currently selected destination.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Whether the destination store will auto select the destination that
     * matches the last used destination stored in appState_.
     * @type {boolean}
     * @private
     */
    this.isInAutoSelectMode_ = false;

    /**
     * Event tracker used to track event listeners of the destination store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch cloud-based print destinations.
     * @type {print_preview.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Maps user account to the list of origins for which destinations are
     * already loaded.
     * @type {!Object.<string, Array.<print_preview.Destination.Origin>>}
     * @private
     */
    this.loadedCloudOrigins_ = {};

    /**
     * ID of a timeout after the initial destination ID is set. If no inserted
     * destination matches the initial destination ID after the specified
     * timeout, the first destination in the store will be automatically
     * selected.
     * @type {?number}
     * @private
     */
    this.autoSelectTimeout_ = null;

    /**
     * Whether a search for local destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isLocalDestinationSearchInProgress_ = false;

    /**
     * Whether the destination store has already loaded or is loading all local
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllLocalDestinations_ = false;

    /**
     * Whether a search for privet destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isPrivetDestinationSearchInProgress_ = false;

    /**
     * Whether the destination store has already loaded or is loading all privet
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllPrivetDestinations_ = false;

    /**
     * ID of a timeout after the start of a privet search to end that privet
     * search.
     * @type {?number}
     * @private
     */
    this.privetSearchTimeout_ = null;

    /**
     * MDNS service name of destination that we are waiting to register.
     * @type {?string}
     * @private
     */
    this.waitForRegisterDestination_ = null;

    this.addEventListeners_();
    this.reset_();
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATION_SEARCH_DONE:
        'print_preview.DestinationStore.DESTINATION_SEARCH_DONE',
    DESTINATION_SEARCH_STARTED:
        'print_preview.DestinationStore.DESTINATION_SEARCH_STARTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT',
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    CACHED_SELECTED_DESTINATION_INFO_READY:
        'print_preview.DestinationStore.CACHED_SELECTED_DESTINATION_INFO_READY',
    SELECTED_DESTINATION_CAPABILITIES_READY:
        'print_preview.DestinationStore.SELECTED_DESTINATION_CAPABILITIES_READY'
  };

  /**
   * Delay in milliseconds before the destination store ignores the initial
   * destination ID and just selects any printer (since the initial destination
   * was not found).
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 15000;

  /**
   * Amount of time spent searching for privet destination, in milliseconds.
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.PRIVET_SEARCH_DURATION_ = 2000;

  /**
   * Localizes printer capabilities.
   * @param {!Object} capabilities Printer capabilities to localize.
   * @return {!Object} Localized capabilities.
   * @private
   */
  DestinationStore.localizeCapabilities_ = function(capabilities) {
    var mediaSize = capabilities.printer.media_size;
    if (mediaSize) {
      var mediaDisplayNames = {
        'ISO_A4': 'A4',
        'ISO_A3': 'A3',
        'NA_LETTER': 'Letter',
        'NA_LEGAL': 'Legal',
        'NA_LEDGER': 'Tabloid'
      };
      for (var i = 0, media; media = mediaSize.option[i]; i++) {
        media.custom_display_name =
            media.custom_display_name ||
            mediaDisplayNames[media.name] ||
            media.name;
      }
    }
    return capabilities;
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @param {string=} opt_account Account to filter destinations by. When
     *     omitted, all destinations are returned.
     * @return {!Array.<!print_preview.Destination>} List of destinations
     *     accessible by the {@code account}.
     */
    destinations: function(opt_account) {
      if (opt_account) {
        return this.destinations_.filter(function(destination) {
          return !destination.account || destination.account == opt_account;
        });
      } else {
        return this.destinations_.slice(0);
      }
    },

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    },

    /**
     * @return {boolean} Whether a search for local destinations is in progress.
     */
    get isLocalDestinationSearchInProgress() {
      return this.isLocalDestinationSearchInProgress_ ||
        this.isPrivetDestinationSearchInProgress_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.cloudPrintInterface_ &&
             this.cloudPrintInterface_.isCloudDestinationSearchInProgress;
    },

    /**
     * Initializes the destination store. Sets the initially selected
     * destination. If any inserted destinations match this ID, that destination
     * will be automatically selected. This method must be called after the
     * print_preview.AppState has been initialized.
     * @private
     */
    init: function() {
      this.isInAutoSelectMode_ = true;
      if (!this.appState_.selectedDestinationId ||
          !this.appState_.selectedDestinationOrigin) {
        this.onAutoSelectFailed_();
      } else {
        var key = this.getDestinationKey_(
            this.appState_.selectedDestinationOrigin,
            this.appState_.selectedDestinationId,
            this.appState_.selectedDestinationAccount);
        var candidate = this.destinationMap_[key];
        if (candidate != null) {
          this.selectDestination(candidate);
        } else if (this.appState_.selectedDestinationOrigin ==
                   print_preview.Destination.Origin.LOCAL) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              this.appState_.selectedDestinationId);
        } else if (this.cloudPrintInterface_ &&
                   (this.appState_.selectedDestinationOrigin ==
                        print_preview.Destination.Origin.COOKIES ||
                    this.appState_.selectedDestinationOrigin ==
                        print_preview.Destination.Origin.DEVICE)) {
          this.cloudPrintInterface_.printer(
              this.appState_.selectedDestinationId,
              this.appState_.selectedDestinationOrigin,
              this.appState_.selectedDestinationAccount);
        } else if (this.appState_.selectedDestinationOrigin ==
                   print_preview.Destination.Origin.PRIVET) {
          // TODO(noamsml): Resolve a specific printer instead of listing all
          // privet printers in this case.
          this.nativeLayer_.startGetPrivetDestinations();

          var destinationName = this.appState_.selectedDestinationName || '';

          // Create a fake selectedDestination_ that is not actually in the
          // destination store. When the real destination is created, this
          // destination will be overwritten.
          this.selectedDestination_ = new print_preview.Destination(
              this.appState_.selectedDestinationId,
              print_preview.Destination.Type.LOCAL,
              print_preview.Destination.Origin.PRIVET,
              destinationName,
              false /*isRecent*/,
              print_preview.Destination.ConnectionStatus.ONLINE);
          this.selectedDestination_.capabilities =
              this.appState_.selectedDestinationCapabilities;

          cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.CACHED_SELECTED_DESTINATION_INFO_READY);

        } else {
          this.onAutoSelectFailed_();
        }
      }
    },

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_FAILED,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_DONE,
          this.onCloudPrintPrinterDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_FAILED,
          this.onCloudPrintPrinterFailed_.bind(this));
    },

    /**
     * @return {boolean} Whether only default cloud destinations have been
     *     loaded.
     */
    hasOnlyDefaultCloudDestinations: function() {
      // TODO: Move the logic to print_preview.
      return this.destinations_.every(function(dest) {
        return dest.isLocal ||
            dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
            dest.id == print_preview.Destination.GooglePromotedId.FEDEX;
      });
    },

    /** @param {!print_preview.Destination} Destination to select. */
    selectDestination: function(destination) {
      this.isInAutoSelectMode_ = false;
      this.cancelAutoSelectTimeout_();
      if (destination == this.selectedDestination_) {
        return;
      }
      if (destination == null) {
        this.selectedDestination_ = null;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SELECT);
        return;
      }
      // Update and persist selected destination.
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      if (destination.id == print_preview.Destination.GooglePromotedId.FEDEX &&
          !destination.isTosAccepted) {
        assert(this.cloudPrintInterface_ != null,
               'Selected FedEx destination, but GCP API is not available');
        destination.isTosAccepted = true;
        this.cloudPrintInterface_.updatePrinterTosAcceptance(destination, true);
      }
      this.appState_.persistSelectedDestination(this.selectedDestination_);
      // Adjust metrics.
      if (destination.cloudID &&
          this.destinations_.some(function(otherDestination) {
            return otherDestination.cloudID == destination.cloudID &&
                otherDestination != destination;
          })) {
        if (destination.isPrivet) {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  PRIVET_DUPLICATE_SELECTED);
        } else {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  CLOUD_DUPLICATE_SELECTED);
        }
      }
      // Notify about selected destination change.
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      // Request destination capabilities, of not known yet.
      if (destination.capabilities == null) {
        if (destination.isPrivet) {
          this.nativeLayer_.startGetPrivetDestinationCapabilities(
              destination.id);
        }
        else if (destination.isLocal) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              destination.id);
        } else {
          assert(this.cloudPrintInterface_ != null,
                 'Cloud destination selected, but GCP is not enabled');
          this.cloudPrintInterface_.printer(
              destination.id, destination.origin, destination.account);
        }
      } else {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Selects 'Save to PDF' destination (since it always exists).
     * @private
     */
    selectDefaultDestination_: function() {
      var destination = this.destinationMap_[this.getDestinationKey_(
          print_preview.Destination.Origin.LOCAL,
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          '')] || null;
      assert(destination != null, 'Save to PDF printer not found');
      this.selectDestination(destination);
    },

    /** Initiates loading of local print destinations. */
    startLoadLocalDestinations: function() {
      if (!this.hasLoadedAllLocalDestinations_) {
        this.hasLoadedAllLocalDestinations_ = true;
        this.nativeLayer_.startGetLocalDestinations();
        this.isLocalDestinationSearchInProgress_ = true;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      }
    },

    /** Initiates loading of privet print destinations. */
    startLoadPrivetDestinations: function() {
      if (!this.hasLoadedAllPrivetDestinations_) {
        this.isPrivetDestinationSearchInProgress_ = true;
        this.nativeLayer_.startGetPrivetDestinations();
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
        this.privetSearchTimeout_ = setTimeout(
            this.endPrivetPrinterSearch_.bind(this),
            DestinationStore.PRIVET_SEARCH_DURATION_);
      }
    },

    /**
     * Initiates loading of cloud destinations.
     * @param {print_preview.Destination.Origin=} opt_origin Search destinations
     *     for the specified origin only.
     */
    startLoadCloudDestinations: function(opt_origin) {
      if (this.cloudPrintInterface_ != null) {
        var origins = this.loadedCloudOrigins_[this.userInfo_.activeUser] || [];
        if (origins.length == 0 ||
            (opt_origin && origins.indexOf(opt_origin) < 0)) {
          this.cloudPrintInterface_.search(
              this.userInfo_.activeUser, opt_origin);
          cr.dispatchSimpleEvent(
              this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
        }
      }
    },

    /** Requests load of COOKIE based cloud destinations. */
    reloadUserCookieBasedDestinations: function() {
      var origins = this.loadedCloudOrigins_[this.userInfo_.activeUser] || [];
      if (origins.indexOf(print_preview.Destination.Origin.COOKIES) >= 0) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
      } else {
        this.startLoadCloudDestinations(
            print_preview.Destination.Origin.COOKIES);
      }
    },

    /**
     * Wait for a privet device to be registered.
     */
    waitForRegister: function(id) {
      this.nativeLayer_.startGetPrivetDestinations();
      this.waitForRegisterDestination_ = id;
    },

    /**
     * Inserts {@code destination} to the data store and dispatches a
     * DESTINATIONS_INSERTED event.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     * @private
     */
    insertDestination_: function(destination) {
      if (this.insertIntoStore_(destination)) {
        this.destinationsInserted_(destination);
      }
    },

    /**
     * Inserts multiple {@code destinations} to the data store and dispatches
     * single DESTINATIONS_INSERTED event.
     * @param {!Array.<print_preview.Destination>} destinations Print
     *     destinations to insert.
     * @private
     */
    insertDestinations_: function(destinations) {
      var inserted = false;
      destinations.forEach(function(destination) {
        inserted = this.insertIntoStore_(destination) || inserted;
      }, this);
      if (inserted) {
        this.destinationsInserted_();
      }
    },

    /**
     * Dispatches DESTINATIONS_INSERTED event. In auto select mode, tries to
     * update selected destination to match {@code appState_} settings.
     * @param {print_preview.Destination=} opt_destination The only destination
     *     that was changed or skipped if possibly more than one destination was
     *     changed. Used as a hint to limit destination search scope in
     *     {@code isInAutoSelectMode_).
     */
    destinationsInserted_: function(opt_destination) {
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      if (this.isInAutoSelectMode_) {
        var destinationsToSearch =
            opt_destination && [opt_destination] || this.destinations_;
        destinationsToSearch.some(function(destination) {
          if (this.matchPersistedDestination_(destination)) {
            this.selectDestination(destination);
            return true;
          }
        }, this);
      }
    },

    /**
     * Updates an existing print destination with capabilities and display name
     * information. If the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @return {!print_preview.Destination} The existing destination that was
     *     updated or {@code null} if it was the new destination.
     * @private
     */
    updateDestination_: function(destination) {
      assert(destination.constructor !== Array, 'Single printer expected');
      var existingDestination = this.destinationMap_[this.getKey_(destination)];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
      } else {
        this.insertDestination_(destination);
      }

      if (existingDestination == this.selectedDestination_ ||
          destination == this.selectedDestination_) {
        this.appState_.persistSelectedDestination(this.selectedDestination_);
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }

      return existingDestination;
    },

    /**
     * Called when the search for Privet printers is done.
     * @private
     */
    endPrivetPrinterSearch_: function() {
      this.nativeLayer_.stopGetPrivetDestinations();
      this.isPrivetDestinationSearchInProgress_ = false;
      this.hasLoadedAllPrivetDestinations_ = true;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Inserts a destination into the store without dispatching any events.
     * @return {boolean} Whether the inserted destination was not already in the
     *     store.
     * @private
     */
    insertIntoStore_: function(destination) {
      var key = this.getKey_(destination);
      var existingDestination = this.destinationMap_[key];
      if (existingDestination == null) {
        this.destinations_.push(destination);
        this.destinationMap_[key] = destination;
        return true;
      } else if (existingDestination.connectionStatus ==
                     print_preview.Destination.ConnectionStatus.UNKNOWN &&
                 destination.connectionStatus !=
                     print_preview.Destination.ConnectionStatus.UNKNOWN) {
        existingDestination.connectionStatus = destination.connectionStatus;
        return true;
      } else {
        return false;
      }
    },

    /**
     * Binds handlers to events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET,
          this.onLocalDestinationsSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CAPABILITIES_SET,
          this.onLocalDestinationCapabilitiesSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.GET_CAPABILITIES_FAIL,
          this.onGetCapabilitiesFail_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DESTINATIONS_RELOAD,
          this.onDestinationsReload_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRIVET_PRINTER_CHANGED,
          this.onPrivetPrinterAdded_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRIVET_CAPABILITIES_SET,
          this.onPrivetCapabilitiesSet_.bind(this));
    },

    /**
     * Creates a local PDF print destination.
     * @return {!print_preview.Destination} Created print destination.
     * @private
     */
    createLocalPdfPrintDestination_: function() {
      return new print_preview.Destination(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          print_preview.Destination.Type.LOCAL,
          print_preview.Destination.Origin.LOCAL,
          localStrings.getString('printToPDF'),
          false /*isRecent*/,
          print_preview.Destination.ConnectionStatus.ONLINE);
    },

    /**
     * Resets the state of the destination store to its initial state.
     * @private
     */
    reset_: function() {
      this.destinations_ = [];
      this.destinationMap_ = {};
      this.selectDestination(null);
      this.loadedCloudOrigins_ = {};
      this.hasLoadedAllLocalDestinations_ = false;
      // TODO(alekseys): Create PDF printer in the native code and send its
      // capabilities back with other local printers.
      this.insertDestination_(this.createLocalPdfPrintDestination_());
      this.resetAutoSelectTimeout_();
    },

    /**
     * Resets destination auto selection timeout.
     * @private
     */
    resetAutoSelectTimeout_: function() {
      this.cancelAutoSelectTimeout_();
      this.autoSelectTimeout_ =
          setTimeout(this.onAutoSelectFailed_.bind(this),
                     DestinationStore.AUTO_SELECT_TIMEOUT_);
    },

    /**
     * Cancels destination auto selection timeout.
     * @private
     */
    cancelAutoSelectTimeout_: function() {
      if (this.autoSelectTimeout_ != null) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      }
    },

    /**
     * Called when the local destinations have been got from the native layer.
     * @param {Event} Contains the local destinations.
     * @private
     */
    onLocalDestinationsSet_: function(event) {
      var localDestinations = event.destinationInfos.map(function(destInfo) {
        return print_preview.LocalDestinationParser.parse(destInfo);
      });
      this.insertDestinations_(localDestinations);
      this.isLocalDestinationSearchInProgress_ = false;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when the native layer retrieves the capabilities for the selected
     * local destination. Updates the destination with new capabilities if the
     * destination already exists, otherwise it creates a new destination and
     * then updates its capabilities.
     * @param {Event} event Contains the capabilities of the local print
     *     destination.
     * @private
     */
    onLocalDestinationCapabilitiesSet_: function(event) {
      var destinationId = event.settingsInfo['printerId'];
      var key = this.getDestinationKey_(
          print_preview.Destination.Origin.LOCAL,
          destinationId,
          '');
      var destination = this.destinationMap_[key];
      var capabilities = DestinationStore.localizeCapabilities_(
          event.settingsInfo.capabilities);
      // Special case for PDF printer (until local printers capabilities are
      // reported in CDD format too).
      if (destinationId ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        if (destination) {
          destination.capabilities = capabilities;
        }
      } else {
        if (destination) {
          // In case there were multiple capabilities request for this local
          // destination, just ignore the later ones.
          if (destination.capabilities != null) {
            return;
          }
          destination.capabilities = capabilities;
        } else {
          // TODO(rltoscano): This makes the assumption that the "deviceName" is
          // the same as "printerName". We should include the "printerName" in
          // the response. See http://crbug.com/132831.
          destination = print_preview.LocalDestinationParser.parse(
              {deviceName: destinationId, printerName: destinationId});
          destination.capabilities = capabilities;
          this.insertDestination_(destination);
        }
      }
      if (this.selectedDestination_ &&
          this.selectedDestination_.id == destinationId) {
        cr.dispatchSimpleEvent(this,
                               DestinationStore.EventType.
                                   SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Called when a request to get a local destination's print capabilities
     * fails. If the destination is the initial destination, auto-select another
     * destination instead.
     * @param {Event} event Contains the destination ID that failed.
     * @private
     */
    onGetCapabilitiesFail_: function(event) {
      console.error('Failed to get print capabilities for printer ' +
                    event.destinationId);
      if (this.isInAutoSelectMode_ &&
          this.sameAsPersistedDestination_(event.destinationId,
                                           event.destinationOrigin)) {
        this.selectDefaultDestination_();
      }
    },

    /**
     * Called when the /search call completes, either successfully or not.
     * In case of success, stores fetched destinations.
     * @param {Event} event Contains the request result.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      if (event.printers) {
        this.insertDestinations_(event.printers);
      }
      if (event.searchDone) {
        var origins = this.loadedCloudOrigins_[event.user] || [];
        if (origins.indexOf(event.origin) < 0) {
          this.loadedCloudOrigins_[event.user] = origins.concat([event.origin]);
        }
      }
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintPrinterDone_: function(event) {
      this.updateDestination_(event.printer);
    },

    /**
     * Called when the Google Cloud Print interface fails to lookup a
     * destination. Selects another destination if the failed destination was
     * the initial destination.
     * @param {object} event Contains the ID of the destination that was failed
     *     to be looked up.
     * @private
     */
    onCloudPrintPrinterFailed_: function(event) {
      if (this.isInAutoSelectMode_ &&
          this.sameAsPersistedDestination_(event.destinationId,
                                           event.destinationOrigin)) {
        console.error(
            'Failed to fetch last used printer caps: ' + event.destinationId);
        this.selectDefaultDestination_();
      }
    },

    /**
     * Called when a Privet printer is added to the local network.
     * @param {object} event Contains information about the added printer.
     * @private
     */
    onPrivetPrinterAdded_: function(event) {
      if (event.printer.serviceName == this.waitForRegisterDestination_ &&
          !event.printer.isUnregistered) {
        this.waitForRegisterDestination_ = null;
        this.onDestinationsReload_();
      } else {
        this.insertDestinations_(
            print_preview.PrivetDestinationParser.parse(event.printer));
      }
    },

    /**
     * Called when capabilities for a privet printer are set.
     * @param {object} event Contains the capabilities and printer ID.
     * @private
     */
    onPrivetCapabilitiesSet_: function(event) {
      var destinationId = event.printerId;
      var destinations =
          print_preview.PrivetDestinationParser.parse(event.printer);
      destinations.forEach(function(dest) {
        dest.capabilities = event.capabilities;
        this.updateDestination_(dest);
      }, this);
    },

    /**
     * Called from native layer after the user was requested to sign in, and did
     * so successfully.
     * @private
     */
    onDestinationsReload_: function() {
      this.reset_();
      this.isInAutoSelectMode_ = true;
      this.startLoadLocalDestinations();
      this.startLoadCloudDestinations();
      this.startLoadPrivetDestinations();
    },

    /**
     * Called when auto-selection fails. Selects the first destination in store.
     * @private
     */
    onAutoSelectFailed_: function() {
      this.cancelAutoSelectTimeout_();
      this.selectDefaultDestination_();
    },

    // TODO(vitalybuka): Remove three next functions replacing Destination.id
    //    and Destination.origin by complex ID.
    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @return {string} id Destination id.
     * @return {string} account User account destination is registered for.
     * @private
     */
    getDestinationKey_: function(origin, id, account) {
      return origin + '/' + id + '/' + account;
    },

    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination} destination Destination.
     * @private
     */
    getKey_: function(destination) {
      return this.getDestinationKey_(
          destination.origin, destination.id, destination.account);
    },

    /**
     * @param {!print_preview.Destination} destination Destination to match.
     * @return {boolean} Whether {@code destination} matches the last user
     *     selected one.
     * @private
     */
    matchPersistedDestination_: function(destination) {
      return !this.appState_.selectedDestinationId ||
             !this.appState_.selectedDestinationOrigin ||
             this.sameAsPersistedDestination_(
                 destination.id, destination.origin);
    },

    /**
     * @param {?string} id Id of the destination.
     * @param {?string} origin Oring of the destination.
     * @return {boolean} Whether destination is the same as initial.
     * @private
     */
    sameAsPersistedDestination_: function(id, origin) {
      return id == this.appState_.selectedDestinationId &&
             origin == this.appState_.selectedDestinationOrigin;
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});
