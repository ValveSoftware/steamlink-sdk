// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!Array<!print_preview.Destination.Origin>} origins Match
   *     destinations from these origins.
   * @param {RegExp} idRegExp Match destination's id.
   * @param {RegExp} displayNameRegExp Match destination's displayName.
   * @param {boolean} skipVirtualDestinations Whether to ignore virtual
   *     destinations, for example, Save as PDF.
   * @constructor
   */
  function DestinationMatch(
      origins, idRegExp, displayNameRegExp, skipVirtualDestinations) {

    /** @private {!Array<!print_preview.Destination.Origin>} */
    this.origins_ = origins;

    /** @private {RegExp} */
    this.idRegExp_ = idRegExp;

    /** @private {RegExp} */
    this.displayNameRegExp_ = displayNameRegExp;

    /** @private {boolean} */
    this.skipVirtualDestinations_ = skipVirtualDestinations;
  };

  DestinationMatch.prototype = {

    /**
     * @param {!print_preview.Destination.Origin} origin Origin to match.
     * @return {boolean} Whether the origin is one of the {@code origins_}.
     */
    matchOrigin: function(origin) {
      return arrayContains(this.origins_, origin);
    },

    /**
     * @param {string} id Id of the destination.
     * @param {string} origin Origin of the destination.
     * @return {boolean} Whether destination is the same as initial.
     */
    matchIdAndOrigin: function(id, origin) {
      return this.matchOrigin(origin) &&
             this.idRegExp_ &&
             this.idRegExp_.test(id);
    },

    /**
     * @param {!print_preview.Destination} destination Destination to match.
     * @return {boolean} Whether {@code destination} matches the last user
     *     selected one.
     */
    match: function(destination) {
      if (!this.matchOrigin(destination.origin)) {
        return false;
      }
      if (this.idRegExp_ && !this.idRegExp_.test(destination.id)) {
        return false;
      }
      if (this.displayNameRegExp_ &&
          !this.displayNameRegExp_.test(destination.displayName)) {
        return false;
      }
      if (this.skipVirtualDestinations_ &&
          this.isVirtualDestination_(destination)) {
        return false;
      }
      return true;
    },

    /**
     * @param {!print_preview.Destination} destination Destination to check.
     * @return {boolean} Whether {@code destination} is virtual, in terms of
     *     destination selection.
     * @private
     */
    isVirtualDestination_: function(destination) {
      if (destination.origin == print_preview.Destination.Origin.LOCAL) {
        return arrayContains(
            [print_preview.Destination.GooglePromotedId.SAVE_AS_PDF],
            destination.id);
      }
      return arrayContains(
          [print_preview.Destination.GooglePromotedId.DOCS,
           print_preview.Destination.GooglePromotedId.FEDEX],
          destination.id);
    }
  };

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!print_preview.NativeLayer} nativeLayer Used to fetch local print
   *     destinations.
   * @param {!print_preview.UserInfo} userInfo User information repository.
   * @param {!print_preview.AppState} appState Application state.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore(nativeLayer, userInfo, appState) {
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
     * @type {!print_preview.DestinationSearchMetricsContext}
     * @private
     */
    this.metrics_ = new print_preview.DestinationSearchMetricsContext();

    /**
     * Internal backing store for the data store.
     * @type {!Array<!print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Cache used for constant lookup of destinations by origin and id.
     * @type {Object<!print_preview.Destination>}
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
     * matches this set of parameters.
     * @type {print_preview.DestinationMatch}
     * @private
     */
    this.autoSelectMatchingDestination_ = null;

    /**
     * Event tracker used to track event listeners of the destination store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Whether PDF printer is enabled. It's disabled, for example, in App Kiosk
     * mode.
     * @type {boolean}
     * @private
     */
    this.pdfPrinterEnabled_ = false;

    /**
     * ID of the system default destination.
     * @type {?string}
     * @private
     */
    this.systemDefaultDestinationId_ = null;

    /**
     * Used to fetch cloud-based print destinations.
     * @type {cloudprint.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Maps user account to the list of origins for which destinations are
     * already loaded.
     * @type {!Object<Array<print_preview.Destination.Origin>>}
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
     * Whether a search for extension destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isExtensionDestinationSearchInProgress_ = false;

    /**
     * Whether the destination store has already loaded all extension
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllExtensionDestinations_ = false;

    /**
     * ID of a timeout set at the start of an extension destination search. The
     * timeout ends the search.
     * @type {?number}
     * @private
     */
    this.extensionSearchTimeout_ = null;

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
    PROVISIONAL_DESTINATION_RESOLVED:
        'print_preview.DestinationStore.PROVISIONAL_DESTINATION_RESOLVED',
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
  DestinationStore.PRIVET_SEARCH_DURATION_ = 5000;

  /**
   * Maximum amount of time spent searching for extension destinations, in
   * milliseconds.
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.EXTENSION_SEARCH_DURATION_ = 5000;

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
        'ISO_A0': 'A0',
        'ISO_A1': 'A1',
        'ISO_A2': 'A2',
        'ISO_A3': 'A3',
        'ISO_A4': 'A4',
        'ISO_A5': 'A5',
        'NA_LEGAL': 'Legal',
        'NA_LETTER': 'Letter',
        'NA_LEDGER': 'Tabloid'
      };
      for (var i = 0, media; media = mediaSize.option[i]; i++) {
        // No need to patch capabilities with localized names provided.
        if (!media.custom_display_name_localized) {
          media.custom_display_name =
              media.custom_display_name ||
              mediaDisplayNames[media.name] ||
              media.name;
        }
      }
    }
    return capabilities;
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @param {string=} opt_account Account to filter destinations by. When
     *     omitted, all destinations are returned.
     * @return {!Array<!print_preview.Destination>} List of destinations
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

    /** @return {boolean} Whether destination selection is pending or not. */
    get isAutoSelectDestinationInProgress() {
      return this.selectedDestination_ == null &&
          this.autoSelectTimeout_ != null;
    },

    /**
     * @return {boolean} Whether a search for local destinations is in progress.
     */
    get isLocalDestinationSearchInProgress() {
      return this.isLocalDestinationSearchInProgress_ ||
        this.isPrivetDestinationSearchInProgress_ ||
        this.isExtensionDestinationSearchInProgress_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return !!this.cloudPrintInterface_ &&
             this.cloudPrintInterface_.isCloudDestinationSearchInProgress;
    },

    /**
     * Initializes the destination store. Sets the initially selected
     * destination. If any inserted destinations match this ID, that destination
     * will be automatically selected. This method must be called after the
     * print_preview.AppState has been initialized.
     * @param {boolean} isInAppKioskMode Whether the print preview is in App
     *     Kiosk mode.
     * @param {?string} systemDefaultDestinationId ID of the system default
     *     destination.
     * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
     *     default destination selection rules.
     */
    init: function(
        isInAppKioskMode,
        systemDefaultDestinationId,
        serializedDefaultDestinationSelectionRulesStr) {
      this.pdfPrinterEnabled_ = !isInAppKioskMode;
      this.systemDefaultDestinationId_ = systemDefaultDestinationId;
      this.createLocalPdfPrintDestination_();

      if (!this.appState_.selectedDestinationId ||
          !this.appState_.selectedDestinationOrigin) {
        var destinationMatch = this.convertToDestinationMatch_(
            serializedDefaultDestinationSelectionRulesStr);
        if (destinationMatch) {
          this.fetchMatchingDestination_(destinationMatch);
          return;
        }
      }

      if (!this.systemDefaultDestinationId_ &&
          !(this.appState_.selectedDestinationId &&
            this.appState_.selectedDestinationOrigin)) {
        this.selectPdfDestination_();
        return;
      }

      var origin = print_preview.Destination.Origin.LOCAL;
      var id = this.systemDefaultDestinationId_;
      var account = '';
      var name = '';
      var capabilities = null;
      var extensionId = '';
      var extensionName = '';
      if (this.appState_.selectedDestinationId &&
          this.appState_.selectedDestinationOrigin) {
        origin = this.appState_.selectedDestinationOrigin;
        id = this.appState_.selectedDestinationId;
        account = this.appState_.selectedDestinationAccount || '';
        name = this.appState_.selectedDestinationName || '';
        capabilities = this.appState_.selectedDestinationCapabilities;
        extensionId = this.appState_.selectedDestinationExtensionId || '';
        extensionName = this.appState_.selectedDestinationExtensionName || '';
      }
      var candidate =
          this.destinationMap_[this.getDestinationKey_(origin, id, account)];
      if (candidate != null) {
        this.selectDestination(candidate);
        return;
      }

      if (this.fetchPreselectedDestination_(
              origin,
              id,
              account,
              name,
              capabilities,
              extensionId,
              extensionName)) {
        return;
      }

      this.selectPdfDestination_();
    },

    /**
     * Attempts to fetch capabilities of the destination identified by the
     * provided origin, id and account.
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @param {string} id Destination id.
     * @param {string} account User account destination is registered for.
     * @param {string} name Destination display name.
     * @param {?print_preview.Cdd} capabilities Destination capabilities.
     * @param {string} extensionId Extension ID associated with this
     *     destination.
     * @param {string} extensionName Extension name associated with this
     *     destination.
     * @private
     */
    fetchPreselectedDestination_: function(
        origin, id, account, name, capabilities, extensionId, extensionName) {
      this.autoSelectMatchingDestination_ =
          this.createExactDestinationMatch_(origin, id);

      if (origin == print_preview.Destination.Origin.LOCAL) {
        this.nativeLayer_.startGetLocalDestinationCapabilities(id);
        return true;
      }

      if (this.cloudPrintInterface_ &&
          (origin == print_preview.Destination.Origin.COOKIES ||
           origin == print_preview.Destination.Origin.DEVICE)) {
        this.cloudPrintInterface_.printer(id, origin, account);
        return true;
      }

      if (origin == print_preview.Destination.Origin.PRIVET) {
        // TODO(noamsml): Resolve a specific printer instead of listing all
        // privet printers in this case.
        this.nativeLayer_.startGetPrivetDestinations();

        // Create a fake selectedDestination_ that is not actually in the
        // destination store. When the real destination is created, this
        // destination will be overwritten.
        this.selectedDestination_ = new print_preview.Destination(
            id,
            print_preview.Destination.Type.LOCAL,
            print_preview.Destination.Origin.PRIVET,
            name,
            false /*isRecent*/,
            print_preview.Destination.ConnectionStatus.ONLINE);
        this.selectedDestination_.capabilities = capabilities;

        cr.dispatchSimpleEvent(
          this,
          DestinationStore.EventType.CACHED_SELECTED_DESTINATION_INFO_READY);
        return true;
      }

      if (origin == print_preview.Destination.Origin.EXTENSION) {
        // TODO(tbarzic): Add support for requesting a single extension's
        // printer list.
        this.startLoadExtensionDestinations();

        this.selectedDestination_ =
            print_preview.ExtensionDestinationParser.parse({
              extensionId: extensionId,
              extensionName: extensionName,
              id: id,
              name: name
            });

        if (capabilities) {
          this.selectedDestination_.capabilities = capabilities;

          cr.dispatchSimpleEvent(
              this,
              DestinationStore.EventType
                  .CACHED_SELECTED_DESTINATION_INFO_READY);
        }
        return true;
      }

      return false;
    },

    /**
     * Attempts to find a destination matching the provided rules.
     * @param {!print_preview.DestinationMatch} destinationMatch Rules to match.
     * @private
     */
    fetchMatchingDestination_: function(destinationMatch) {
      this.autoSelectMatchingDestination_ = destinationMatch;

      if (destinationMatch.matchOrigin(
            print_preview.Destination.Origin.LOCAL)) {
        this.startLoadLocalDestinations();
      }
      if (destinationMatch.matchOrigin(
            print_preview.Destination.Origin.PRIVET)) {
        this.startLoadPrivetDestinations();
      }
      if (destinationMatch.matchOrigin(
            print_preview.Destination.Origin.EXTENSION)) {
        this.startLoadExtensionDestinations();
      }
      if (destinationMatch.matchOrigin(
            print_preview.Destination.Origin.COOKIES) ||
          destinationMatch.matchOrigin(
            print_preview.Destination.Origin.DEVICE) ||
          destinationMatch.matchOrigin(
            print_preview.Destination.Origin.PROFILE)) {
        this.startLoadCloudDestinations();
      }
    },

    /**
     * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
     *     default destination selection rules.
     * @return {!print_preview.DestinationMatch} Creates rules matching
     *     previously selected destination.
     * @private
     */
    convertToDestinationMatch_: function(
        serializedDefaultDestinationSelectionRulesStr) {
      var matchRules = null;
      try {
        if (serializedDefaultDestinationSelectionRulesStr) {
          matchRules =
              JSON.parse(serializedDefaultDestinationSelectionRulesStr);
        }
      } catch(e) {
        console.error(
            'Failed to parse defaultDestinationSelectionRules: ' + e);
      }
      if (!matchRules)
        return;

      var isLocal = !matchRules.kind || matchRules.kind == 'local';
      var isCloud = !matchRules.kind || matchRules.kind == 'cloud';
      if (!isLocal && !isCloud) {
        console.error('Unsupported type: "' + matchRules.kind + '"');
        return null;
      }

      var origins = [];
      if (isLocal) {
        origins.push(print_preview.Destination.Origin.LOCAL);
        origins.push(print_preview.Destination.Origin.PRIVET);
        origins.push(print_preview.Destination.Origin.EXTENSION);
      }
      if (isCloud) {
        origins.push(print_preview.Destination.Origin.COOKIES);
        origins.push(print_preview.Destination.Origin.DEVICE);
        origins.push(print_preview.Destination.Origin.PROFILE);
      }

      var idRegExp = null;
      try {
        if (matchRules.idPattern) {
          idRegExp = new RegExp(matchRules.idPattern || '.*');
        }
      } catch (e) {
        console.error('Failed to parse regexp for "id": ' + e);
      }

      var displayNameRegExp = null;
      try {
        if (matchRules.namePattern) {
          displayNameRegExp = new RegExp(matchRules.namePattern || '.*');
        }
      } catch (e) {
        console.error('Failed to parse regexp for "name": ' + e);
      }

      return new DestinationMatch(
          origins,
          idRegExp,
          displayNameRegExp,
          true /*skipVirtualDestinations*/);
    },

    /**
     * @return {print_preview.DestinationMatch} Creates rules matching
     *     previously selected destination.
     * @private
     */
    convertPreselectedToDestinationMatch_: function() {
      if (this.appState_.selectedDestinationId &&
          this.appState_.selectedDestinationOrigin) {
        return this.createExactDestinationMatch_(
            this.appState_.selectedDestinationOrigin,
            this.appState_.selectedDestinationId);
      }
      if (this.systemDefaultDestinationId_) {
        return this.createExactDestinationMatch_(
            print_preview.Destination.Origin.LOCAL,
            this.systemDefaultDestinationId_);
      }
      return null;
    },

    /**
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @param {string} id Destination id.
     * @return {!print_preview.DestinationMatch} Creates rules matching
     *     provided destination.
     * @private
     */
    createExactDestinationMatch_: function(origin, id) {
      return new DestinationMatch(
          [origin],
          new RegExp('^' + id.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '$'),
          null /*displayNameRegExp*/,
          false /*skipVirtualDestinations*/);
    },

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!cloudprint.CloudPrintInterface} cloudPrintInterface Interface
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
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PROCESS_INVITE_DONE,
          this.onCloudPrintProcessInviteDone_.bind(this));
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

    /**
     * @param {print_preview.Destination} destination Destination to select.
     */
    selectDestination: function(destination) {
      this.autoSelectMatchingDestination_ = null;
      // When auto select expires, DESTINATION_SELECT event has to be dispatched
      // anyway (see isAutoSelectDestinationInProgress() logic).
      if (this.autoSelectTimeout_) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      } else if (destination == this.selectedDestination_) {
        return;
      }
      if (destination == null) {
        this.selectedDestination_ = null;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SELECT);
        return;
      }

      assert(!destination.isProvisional,
             'Unable to select provisonal destinations');

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
        this.metrics_.record(destination.isPrivet ?
            print_preview.Metrics.DestinationSearchBucket.
                PRIVET_DUPLICATE_SELECTED :
            print_preview.Metrics.DestinationSearchBucket.
                CLOUD_DUPLICATE_SELECTED);
      }
      // Notify about selected destination change.
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      // Request destination capabilities, of not known yet.
      if (destination.capabilities == null) {
        if (destination.isPrivet) {
          this.nativeLayer_.startGetPrivetDestinationCapabilities(
              destination.id);
        } else if (destination.isExtension) {
          this.nativeLayer_.startGetExtensionDestinationCapabilities(
              destination.id);
        } else if (destination.isLocal) {
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
     * Attempts to resolve a provisional destination.
     * @param {!print_preview.Destination} destinaion Provisional destination
     *     that should be resolved.
     */
    resolveProvisionalDestination: function(destination) {
      assert(
          destination.provisionalType ==
              print_preview.Destination.ProvisionalType.NEEDS_USB_PERMISSION,
          'Provisional type cannot be resolved.');
      this.nativeLayer_.grantExtensionPrinterAccess(destination.id);
    },

    /**
     * Selects 'Save to PDF' destination (since it always exists).
     * @private
     */
    selectPdfDestination_: function() {
      var saveToPdfKey = this.getDestinationKey_(
          print_preview.Destination.Origin.LOCAL,
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          '');
      this.selectDestination(
          this.destinationMap_[saveToPdfKey] || this.destinations_[0] || null);
    },

    /**
     * Attempts to select system default destination with a fallback to
     * 'Save to PDF' destination.
     * @private
     */
    selectDefaultDestination_: function() {
      if (this.systemDefaultDestinationId_) {
        if (this.autoSelectMatchingDestination_ &&
            !this.autoSelectMatchingDestination_.matchIdAndOrigin(
                this.systemDefaultDestinationId_,
                print_preview.Destination.Origin.LOCAL)) {
          if (this.fetchPreselectedDestination_(
                print_preview.Destination.Origin.LOCAL,
                this.systemDefaultDestinationId_,
                '' /*account*/,
                '' /*name*/,
                null /*capabilities*/,
                '' /*extensionId*/,
                '' /*extensionName*/)) {
            return;
          }
        }
      }
      this.selectPdfDestination_();
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
        if (this.privetDestinationSearchInProgress_)
          clearTimeout(this.privetSearchTimeout_);
        this.isPrivetDestinationSearchInProgress_ = true;
        this.nativeLayer_.startGetPrivetDestinations();
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
        this.privetSearchTimeout_ = setTimeout(
            this.endPrivetPrinterSearch_.bind(this),
            DestinationStore.PRIVET_SEARCH_DURATION_);
      }
    },

    /** Initializes loading of extension managed print destinations. */
    startLoadExtensionDestinations: function() {
      if (this.hasLoadedAllExtensionDestinations_)
        return;

      if (this.isExtensionDestinationSearchInProgress_)
        clearTimeout(this.extensionSearchTimeout_);

      this.isExtensionDestinationSearchInProgress_ = true;
      this.nativeLayer_.startGetExtensionDestinations();
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      this.extensionSearchTimeout_ = setTimeout(
          this.endExtensionPrinterSearch_.bind(this),
          DestinationStore.EXTENSION_SEARCH_DURATION_);
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

    /** Initiates loading of all known destination types. */
    startLoadAllDestinations: function() {
      this.startLoadCloudDestinations();
      this.startLoadLocalDestinations();
      this.startLoadPrivetDestinations();
      this.startLoadExtensionDestinations();
    },

    /**
     * Wait for a privet device to be registered.
     */
    waitForRegister: function(id) {
      this.nativeLayer_.startGetPrivetDestinations();
      this.waitForRegisterDestination_ = id;
    },

    /**
     * Event handler for {@code
     * print_preview.NativeLayer.EventType.PROVISIONAL_DESTINATION_RESOLVED}.
     * Currently assumes the provisional destination is an extension
     * destination.
     * Called when a provisional destination resolvement attempt finishes.
     * The provisional destination is removed from the store and replaced with
     * a destination created from the resolved destination properties, if any
     * are reported.
     * Emits {@code DestinationStore.EventType.PROVISIONAL_DESTINATION_RESOLVED}
     * event.
     * @param {!Event} The event containing the provisional destination ID and
     *     resolved destination description. If the destination was not
     *     successfully resolved, the description will not be set.
     * @private
     */
    handleProvisionalDestinationResolved_: function(evt) {
      var provisionalDestinationIndex = -1;
      var provisionalDestination = null;
      for (var i = 0; i < this.destinations_.length; ++i) {
        if (evt.provisionalId == this.destinations_[i].id) {
          provisionalDestinationIndex = i;
          provisionalDestination = this.destinations_[i];
          break;
        }
      }

      if (!provisionalDestination)
        return;

      this.destinations_.splice(provisionalDestinationIndex, 1);
      delete this.destinationMap_[this.getKey_(provisionalDestination)];

      var destination = evt.destination ?
          print_preview.ExtensionDestinationParser.parse(evt.destination) :
          null;

      if (destination)
        this.insertIntoStore_(destination);

      var event = new Event(
          DestinationStore.EventType.PROVISIONAL_DESTINATION_RESOLVED);
      event.provisionalId = evt.provisionalId;
      event.destination = destination;
      this.dispatchEvent(event);
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
     * @param {!Array<print_preview.Destination>} destinations Print
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
     * update selected destination to match
     * {@code autoSelectMatchingDestination_}.
     * @param {print_preview.Destination=} opt_destination The only destination
     *     that was changed or skipped if possibly more than one destination was
     *     changed. Used as a hint to limit destination search scope against
     *     {@code autoSelectMatchingDestination_).
     */
    destinationsInserted_: function(opt_destination) {
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      if (this.autoSelectMatchingDestination_) {
        var destinationsToSearch =
            opt_destination && [opt_destination] || this.destinations_;
        destinationsToSearch.some(function(destination) {
          if (this.autoSelectMatchingDestination_.match(destination)) {
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
     * Called when loading of extension managed printers is done.
     * @private
     */
    endExtensionPrinterSearch_: function() {
      this.isExtensionDestinationSearchInProgress_ = false;
      this.hasLoadedAllExtensionDestinations_ = true;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
      // Clear initially selected (cached) extension destination if it hasn't
      // been found among reported extension destinations.
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchOrigin(
              print_preview.Destination.Origin.EXTENSION) &&
          this.selectedDestination_ &&
          this.selectedDestination_.isExtension) {
        this.selectDefaultDestination_();
      }
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
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.EXTENSION_PRINTERS_ADDED,
          this.onExtensionPrintersAdded_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.EXTENSION_CAPABILITIES_SET,
          this.onExtensionCapabilitiesSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PROVISIONAL_DESTINATION_RESOLVED,
          this.handleProvisionalDestinationResolved_.bind(this));
    },

    /**
     * Creates a local PDF print destination.
     * @return {!print_preview.Destination} Created print destination.
     * @private
     */
    createLocalPdfPrintDestination_: function() {
      // TODO(alekseys): Create PDF printer in the native code and send its
      // capabilities back with other local printers.
      if (this.pdfPrinterEnabled_) {
        this.insertDestination_(new print_preview.Destination(
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
            print_preview.Destination.Type.LOCAL,
            print_preview.Destination.Origin.LOCAL,
            loadTimeData.getString('printToPDF'),
            false /*isRecent*/,
            print_preview.Destination.ConnectionStatus.ONLINE));
      }
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
      this.hasLoadedAllPrivetDestinations_ = false;
      this.hasLoadedAllExtensionDestinations_ = false;

      clearTimeout(this.autoSelectTimeout_);
      this.autoSelectTimeout_ = setTimeout(
          this.selectDefaultDestination_.bind(this),
          DestinationStore.AUTO_SELECT_TIMEOUT_);
    },

    /**
     * Called when the local destinations have been got from the native layer.
     * @param {Event} event Contains the local destinations.
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
      var printerName = event.settingsInfo['printerName'];
      var printerDescription = event.settingsInfo['printerDescription'];
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
          destination = print_preview.LocalDestinationParser.parse(
              {deviceName: destinationId,
               printerName: printerName,
               printerDescription: printerDescription});
          destination.capabilities = capabilities;
          this.insertDestination_(destination);
        }
      }
      if (this.selectedDestination_ &&
          this.selectedDestination_.id == destinationId) {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
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
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchIdAndOrigin(
              event.destinationId, event.destinationOrigin)) {
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
     * @param {Object} event Contains the ID of the destination that was failed
     *     to be looked up.
     * @private
     */
    onCloudPrintPrinterFailed_: function(event) {
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchIdAndOrigin(
              event.destinationId, event.destinationOrigin)) {
        console.error(
            'Failed to fetch last used printer caps: ' + event.destinationId);
        this.selectDefaultDestination_();
      }
    },

    /**
     * Called when printer sharing invitation was processed successfully.
     * @param {Event} event Contains detailed information about the invite and
     *     newly accepted destination (if known).
     * @private
     */
    onCloudPrintProcessInviteDone_: function(event) {
      if (event.accept && event.printer) {
        // Hint the destination list to promote this new destination.
        event.printer.isRecent = true;
        this.insertDestination_(event.printer);
      }
    },

    /**
     * Called when a Privet printer is added to the local network.
     * @param {Object} event Contains information about the added printer.
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
     * @param {Object} event Contains the capabilities and printer ID.
     * @private
     */
    onPrivetCapabilitiesSet_: function(event) {
      var destinations =
          print_preview.PrivetDestinationParser.parse(event.printer);
      destinations.forEach(function(dest) {
        dest.capabilities = event.capabilities;
        this.updateDestination_(dest);
      }, this);
    },

    /**
     * Called when an extension responds to a getExtensionDestinations
     * request.
     * @param {Object} event Contains information about list of printers
     *     reported by the extension.
     *     {@code done} parameter is set iff this is the final list of printers
     *     returned as part of getExtensionDestinations request.
     * @private
     */
    onExtensionPrintersAdded_: function(event) {
      this.insertDestinations_(event.printers.map(function(printer) {
        return print_preview.ExtensionDestinationParser.parse(printer);
      }));

      if (event.done && this.isExtensionDestinationSearchInProgress_) {
        clearTimeout(this.extensionSearchTimeout_);
        this.endExtensionPrinterSearch_();
      }
    },

    /**
     * Called when capabilities for an extension managed printer are set.
     * @param {Object} event Contains the printer's capabilities and ID.
     * @private
     */
    onExtensionCapabilitiesSet_: function(event) {
      var destinationKey = this.getDestinationKey_(
          print_preview.Destination.Origin.EXTENSION,
          event.printerId,
          '' /* account */);
      var destination = this.destinationMap_[destinationKey];
      if (!destination)
        return;
      destination.capabilities = event.capabilities;
      this.updateDestination_(destination);
    },

    /**
     * Called from native layer after the user was requested to sign in, and did
     * so successfully.
     * @private
     */
    onDestinationsReload_: function() {
      this.reset_();
      this.autoSelectMatchingDestination_ =
          this.convertPreselectedToDestinationMatch_();
      this.createLocalPdfPrintDestination_();
      this.startLoadAllDestinations();
    },

    // TODO(vitalybuka): Remove three next functions replacing Destination.id
    //    and Destination.origin by complex ID.
    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @param {string} id Destination id.
     * @param {string} account User account destination is registered for.
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
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});
