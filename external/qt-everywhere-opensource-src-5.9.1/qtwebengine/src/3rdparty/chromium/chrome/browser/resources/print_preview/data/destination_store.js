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
   * @private {number}
   * @const
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 15000;

  /**
   * Amount of time spent searching for privet destination, in milliseconds.
   * @private {number}
   * @const
   */
  DestinationStore.PRIVET_SEARCH_DURATION_ = 5000;

  /**
   * Maximum amount of time spent searching for extension destinations, in
   * milliseconds.
   * @private {number}
   * @const
   */
  DestinationStore.EXTENSION_SEARCH_DURATION_ = 5000;

  /**
   * Human readable names for media sizes in the cloud print CDD.
   * https://developers.google.com/cloud-print/docs/cdd
   * @private {Object<string>}
   * @const
   */
  DestinationStore.MEDIA_DISPLAY_NAMES_ = {
    'ISO_2A0': '2A0',
    'ISO_A0': 'A0',
    'ISO_A0X3': 'A0x3',
    'ISO_A1': 'A1',
    'ISO_A10': 'A10',
    'ISO_A1X3': 'A1x3',
    'ISO_A1X4': 'A1x4',
    'ISO_A2': 'A2',
    'ISO_A2X3': 'A2x3',
    'ISO_A2X4': 'A2x4',
    'ISO_A2X5': 'A2x5',
    'ISO_A3': 'A3',
    'ISO_A3X3': 'A3x3',
    'ISO_A3X4': 'A3x4',
    'ISO_A3X5': 'A3x5',
    'ISO_A3X6': 'A3x6',
    'ISO_A3X7': 'A3x7',
    'ISO_A3_EXTRA': 'A3 Extra',
    'ISO_A4': 'A4',
    'ISO_A4X3': 'A4x3',
    'ISO_A4X4': 'A4x4',
    'ISO_A4X5': 'A4x5',
    'ISO_A4X6': 'A4x6',
    'ISO_A4X7': 'A4x7',
    'ISO_A4X8': 'A4x8',
    'ISO_A4X9': 'A4x9',
    'ISO_A4_EXTRA': 'A4 Extra',
    'ISO_A4_TAB': 'A4 Tab',
    'ISO_A5': 'A5',
    'ISO_A5_EXTRA': 'A5 Extra',
    'ISO_A6': 'A6',
    'ISO_A7': 'A7',
    'ISO_A8': 'A8',
    'ISO_A9': 'A9',
    'ISO_B0': 'B0',
    'ISO_B1': 'B1',
    'ISO_B10': 'B10',
    'ISO_B2': 'B2',
    'ISO_B3': 'B3',
    'ISO_B4': 'B4',
    'ISO_B5': 'B5',
    'ISO_B5_EXTRA': 'B5 Extra',
    'ISO_B6': 'B6',
    'ISO_B6C4': 'B6C4',
    'ISO_B7': 'B7',
    'ISO_B8': 'B8',
    'ISO_B9': 'B9',
    'ISO_C0': 'C0',
    'ISO_C1': 'C1',
    'ISO_C10': 'C10',
    'ISO_C2': 'C2',
    'ISO_C3': 'C3',
    'ISO_C4': 'C4',
    'ISO_C5': 'C5',
    'ISO_C6': 'C6',
    'ISO_C6C5': 'C6C5',
    'ISO_C7': 'C7',
    'ISO_C7C6': 'C7C6',
    'ISO_C8': 'C8',
    'ISO_C9': 'C9',
    'ISO_DL': 'Envelope DL',
    'ISO_RA0': 'RA0',
    'ISO_RA1': 'RA1',
    'ISO_RA2': 'RA2',
    'ISO_SRA0': 'SRA0',
    'ISO_SRA1': 'SRA1',
    'ISO_SRA2': 'SRA2',
    'JIS_B0': 'B0 (JIS)',
    'JIS_B1': 'B1 (JIS)',
    'JIS_B10': 'B10 (JIS)',
    'JIS_B2': 'B2 (JIS)',
    'JIS_B3': 'B3 (JIS)',
    'JIS_B4': 'B4 (JIS)',
    'JIS_B5': 'B5 (JIS)',
    'JIS_B6': 'B6 (JIS)',
    'JIS_B7': 'B7 (JIS)',
    'JIS_B8': 'B8 (JIS)',
    'JIS_B9': 'B9 (JIS)',
    'JIS_EXEC': 'Executive (JIS)',
    'JPN_CHOU2': 'Choukei 2',
    'JPN_CHOU3': 'Choukei 3',
    'JPN_CHOU4': 'Choukei 4',
    'JPN_HAGAKI': 'Hagaki',
    'JPN_KAHU': 'Kahu Envelope',
    'JPN_KAKU2': 'Kaku 2',
    'JPN_OUFUKU': 'Oufuku Hagaki',
    'JPN_YOU4': 'You 4',
    'NA_10X11': '10x11',
    'NA_10X13': '10x13',
    'NA_10X14': '10x14',
    'NA_10X15': '10x15',
    'NA_11X12': '11x12',
    'NA_11X15': '11x15',
    'NA_12X19': '12x19',
    'NA_5X7': '5x7',
    'NA_6X9': '6x9',
    'NA_7X9': '7x9',
    'NA_9X11': '9x11',
    'NA_A2': 'A2',
    'NA_ARCH_A': 'Arch A',
    'NA_ARCH_B': 'Arch B',
    'NA_ARCH_C': 'Arch C',
    'NA_ARCH_D': 'Arch D',
    'NA_ARCH_E': 'Arch E',
    'NA_ASME_F': 'ASME F',
    'NA_B_PLUS': 'B-plus',
    'NA_C': 'C',
    'NA_C5': 'C5',
    'NA_D': 'D',
    'NA_E': 'E',
    'NA_EDP': 'EDP',
    'NA_EUR_EDP': 'European EDP',
    'NA_EXECUTIVE': 'Executive',
    'NA_F': 'F',
    'NA_FANFOLD_EUR': 'FanFold European',
    'NA_FANFOLD_US': 'FanFold US',
    'NA_FOOLSCAP': 'FanFold German Legal',
    'NA_GOVT_LEGAL': 'Government Legal',
    'NA_GOVT_LETTER': 'Government Letter',
    'NA_INDEX_3X5': 'Index 3x5',
    'NA_INDEX_4X6': 'Index 4x6',
    'NA_INDEX_4X6_EXT': 'Index 4x6 ext',
    'NA_INDEX_5X8': '5x8',
    'NA_INVOICE': 'Invoice',
    'NA_LEDGER': 'Tabloid',  // Ledger in portrait is called Tabloid.
    'NA_LEGAL': 'Legal',
    'NA_LEGAL_EXTRA': 'Legal extra',
    'NA_LETTER': 'Letter',
    'NA_LETTER_EXTRA': 'Letter extra',
    'NA_LETTER_PLUS': 'Letter plus',
    'NA_MONARCH': 'Monarch',
    'NA_NUMBER_10': 'Envelope #10',
    'NA_NUMBER_11': 'Envelope #11',
    'NA_NUMBER_12': 'Envelope #12',
    'NA_NUMBER_14': 'Envelope #14',
    'NA_NUMBER_9': 'Envelope #9',
    'NA_PERSONAL': 'Personal',
    'NA_QUARTO': 'Quarto',
    'NA_SUPER_A': 'Super A',
    'NA_SUPER_B': 'Super B',
    'NA_WIDE_FORMAT': 'Wide format',
    'OM_DAI_PA_KAI': 'Dai-pa-kai',
    'OM_FOLIO': 'Folio',
    'OM_FOLIO_SP': 'Folio SP',
    'OM_INVITE': 'Invite Envelope',
    'OM_ITALIAN': 'Italian Envelope',
    'OM_JUURO_KU_KAI': 'Juuro-ku-kai',
    'OM_LARGE_PHOTO': 'Large photo',
    'OM_OFICIO': 'Oficio',
    'OM_PA_KAI': 'Pa-kai',
    'OM_POSTFIX': 'Postfix Envelope',
    'OM_SMALL_PHOTO': 'Small photo',
    'PRC_1': 'prc1 Envelope',
    'PRC_10': 'prc10 Envelope',
    'PRC_16K': 'prc 16k',
    'PRC_2': 'prc2 Envelope',
    'PRC_3': 'prc3 Envelope',
    'PRC_32K': 'prc 32k',
    'PRC_4': 'prc4 Envelope',
    'PRC_5': 'prc5 Envelope',
    'PRC_6': 'prc6 Envelope',
    'PRC_7': 'prc7 Envelope',
    'PRC_8': 'prc8 Envelope',
    'ROC_16K': 'ROC 16K',
    'ROC_8K': 'ROC 8k',
  };

  /**
   * Localizes printer capabilities.
   * @param {!Object} capabilities Printer capabilities to localize.
   * @return {!Object} Localized capabilities.
   * @private
   */
  DestinationStore.localizeCapabilities_ = function(capabilities) {
    if (!capabilities.printer)
      return capabilities;

    var mediaSize = capabilities.printer.media_size;
    if (!mediaSize)
      return capabilities;

    for (var i = 0, media; media = mediaSize.option[i]; i++) {
      // No need to patch capabilities with localized names provided.
      if (!media.custom_display_name_localized) {
        media.custom_display_name =
            media.custom_display_name ||
            DestinationStore.MEDIA_DISPLAY_NAMES_[media.name] ||
            media.name;
      }
    }
    return capabilities;
  };

  /**
   * Compare two media sizes by their names.
   * @param {!Object} a Media to compare.
   * @param {!Object} b Media to compare.
   * @return {number} 1 if a > b, -1 if a < b, or 0 if a == b.
   * @private
   */
  DestinationStore.compareMediaNames_ = function(a, b) {
    var nameA = a.custom_display_name_localized || a.custom_display_name;
    var nameB = b.custom_display_name_localized || b.custom_display_name;
    return nameA == nameB ? 0 : (nameA > nameB ? 1 : -1);
  };

  /**
   * Sort printer media sizes.
   * @param {!Object} capabilities Printer capabilities to localize.
   * @return {!Object} Localized capabilities.
   * @private
   */
  DestinationStore.sortMediaSizes_ = function(capabilities) {
    if (!capabilities.printer)
      return capabilities;

    var mediaSize = capabilities.printer.media_size;
    if (!mediaSize)
      return capabilities;

    // For the standard sizes, separate into categories, as seen in the Cloud
    // Print CDD guide:
    // - North American
    // - Chinese
    // - ISO
    // - Japanese
    // - Other metric
    // Otherwise, assume they are custom sizes.
    var categoryStandardNA = [];
    var categoryStandardCN = [];
    var categoryStandardISO = [];
    var categoryStandardJP = [];
    var categoryStandardMisc = [];
    var categoryCustom = [];
    for (var i = 0, media; media = mediaSize.option[i]; i++) {
      var name = media.name || 'CUSTOM';
      var category;
      if (name.startsWith('NA_')) {
        category = categoryStandardNA;
      } else if (name.startsWith('PRC_') || name.startsWith('ROC_') ||
                 name == 'OM_DAI_PA_KAI' || name == 'OM_JUURO_KU_KAI' ||
                 name == 'OM_PA_KAI') {
        category = categoryStandardCN;
      } else if (name.startsWith('ISO_')) {
        category = categoryStandardISO;
      } else if (name.startsWith('JIS_') || name.startsWith('JPN_')) {
        category = categoryStandardJP;
      } else if (name.startsWith('OM_')) {
        category = categoryStandardMisc;
      } else {
        assert(name == 'CUSTOM', 'Unknown media size. Assuming custom');
        category = categoryCustom;
      }
      category.push(media);
    }

    // For each category, sort by name.
    categoryStandardNA.sort(DestinationStore.compareMediaNames_);
    categoryStandardCN.sort(DestinationStore.compareMediaNames_);
    categoryStandardISO.sort(DestinationStore.compareMediaNames_);
    categoryStandardJP.sort(DestinationStore.compareMediaNames_);
    categoryStandardMisc.sort(DestinationStore.compareMediaNames_);
    categoryCustom.sort(DestinationStore.compareMediaNames_);

    // Then put it all back together.
    mediaSize.option = categoryStandardNA;
    mediaSize.option.push(...categoryStandardCN, ...categoryStandardISO,
        ...categoryStandardJP, ...categoryStandardMisc, ...categoryCustom);
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
      }
      return this.destinations_.slice(0);
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
     * @return {boolean} Whether the selected destination is valid.
     */
    selectedDestinationValid_: function() {
      return this.appState_.selectedDestination &&
             this.appState_.selectedDestination.id &&
             this.appState_.selectedDestination.origin;
    },

    /*
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

      if (!this.selectedDestinationValid_()) {
        var destinationMatch = this.convertToDestinationMatch_(
            serializedDefaultDestinationSelectionRulesStr);
        if (destinationMatch) {
          this.fetchMatchingDestination_(destinationMatch);
          return;
        }
      }

      if (!this.systemDefaultDestinationId_ &&
          !this.selectedDestinationValid_()) {
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
      var foundDestination = false;
      if (this.appState_.recentDestinations) {
        // Run through the destinations forward. As soon as we find a
        // destination, don't select any future destinations, just mark
        // them recent. Otherwise, there is a race condition between selecting
        // destinations/updating the print ticket and this selecting a new
        // destination that causes random print preview errors.
        for (var i = 0; i < this.appState_.recentDestinations.length; i++) {
          origin = this.appState_.recentDestinations[i].origin;
          id = this.appState_.recentDestinations[i].id;
          account = this.appState_.recentDestinations[i].account || '';
          name = this.appState_.recentDestinations[i].name || '';
          capabilities = this.appState_.recentDestinations[i].capabilities;
          extensionId = this.appState_.recentDestinations[i].extensionId ||
                        '';
          extensionName =
              this.appState_.recentDestinations[i].extensionName || '';
          var candidate =
              this.destinationMap_[this.getDestinationKey_(origin,
                                                           id, account)];
          if (candidate != null) {
            if (!foundDestination)
              this.selectDestination(candidate);
            candidate.isRecent = true;
            foundDestination = true;
          } else if (!foundDestination) {
            foundDestination = this.fetchPreselectedDestination_(
                                    origin,
                                    id,
                                    account,
                                    name,
                                    capabilities,
                                    extensionId,
                                    extensionName);
          }
        }
      }
      if (foundDestination) return;
      // Try the system default
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
      if (this.selectedDestinationValid_()) {
        return this.createExactDestinationMatch_(
            this.appState_.selectedDestination.origin,
            this.appState_.selectedDestination.id);
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
     *     {@code autoSelectMatchingDestination_}.
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
     * @private
     */
    updateDestination_: function(destination) {
      assert(destination.constructor !== Array, 'Single printer expected');
      destination.capabilities_ = DestinationStore.localizeCapabilities_(
          destination.capabilities_);
      destination.capabilities_ = DestinationStore.sortMediaSizes_(
          destination.capabilities_);
      var existingDestination = this.destinationMap_[this.getKey_(destination)];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
      } else {
        this.insertDestination_(destination);
      }

      if (this.selectedDestination_ &&
          (existingDestination == this.selectedDestination_ ||
           destination == this.selectedDestination_)) {
        this.appState_.persistSelectedDestination(this.selectedDestination_);
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
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
        destination.isRecent |= this.appState_.recentDestinations.some(
            function(recent) {
              return (destination.id == recent.id &&
                      destination.origin == recent.origin);
            }, this);
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
