// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{selectSaveAsPdfDestination: boolean,
 *            layoutSettings.portrait: boolean,
 *            pageRange: string,
 *            headersAndFooters: boolean,
 *            backgroundColorsAndImages: boolean,
 *            margins: number}}
 * @see chrome/browser/printing/print_preview_pdf_generated_browsertest.cc
 */
print_preview.PreviewSettings;

cr.define('print_preview', function() {
  'use strict';

  /**
   * An interface to the native Chromium printing system layer.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function NativeLayer() {
    cr.EventTarget.call(this);

    // Bind global handlers
    global.setInitialSettings = this.onSetInitialSettings_.bind(this);
    global.setUseCloudPrint = this.onSetUseCloudPrint_.bind(this);
    global.setPrinters = this.onSetPrinters_.bind(this);
    global.updateWithPrinterCapabilities =
        this.onUpdateWithPrinterCapabilities_.bind(this);
    global.failedToGetPrinterCapabilities =
        this.onFailedToGetPrinterCapabilities_.bind(this);
    global.failedToGetPrivetPrinterCapabilities =
      this.onFailedToGetPrivetPrinterCapabilities_.bind(this);
    global.failedToGetExtensionPrinterCapabilities =
        this.onFailedToGetExtensionPrinterCapabilities_.bind(this);
    global.reloadPrintersList = this.onReloadPrintersList_.bind(this);
    global.printToCloud = this.onPrintToCloud_.bind(this);
    global.fileSelectionCancelled =
        this.onFileSelectionCancelled_.bind(this);
    global.fileSelectionCompleted =
        this.onFileSelectionCompleted_.bind(this);
    global.printPreviewFailed = this.onPrintPreviewFailed_.bind(this);
    global.invalidPrinterSettings =
        this.onInvalidPrinterSettings_.bind(this);
    global.onDidGetDefaultPageLayout =
        this.onDidGetDefaultPageLayout_.bind(this);
    global.onDidGetPreviewPageCount =
        this.onDidGetPreviewPageCount_.bind(this);
    global.onDidPreviewPage = this.onDidPreviewPage_.bind(this);
    global.updatePrintPreview = this.onUpdatePrintPreview_.bind(this);
    global.onDidGetAccessToken = this.onDidGetAccessToken_.bind(this);
    global.autoCancelForTesting = this.autoCancelForTesting_.bind(this);
    global.onPrivetPrinterChanged = this.onPrivetPrinterChanged_.bind(this);
    global.onPrivetCapabilitiesSet =
        this.onPrivetCapabilitiesSet_.bind(this);
    global.onPrivetPrintFailed = this.onPrivetPrintFailed_.bind(this);
    global.onExtensionPrintersAdded =
        this.onExtensionPrintersAdded_.bind(this);
    global.onExtensionCapabilitiesSet =
        this.onExtensionCapabilitiesSet_.bind(this);
    global.onEnableManipulateSettingsForTest =
        this.onEnableManipulateSettingsForTest_.bind(this);
    global.printPresetOptionsFromDocument =
        this.onPrintPresetOptionsFromDocument_.bind(this);
    global.onProvisionalPrinterResolved =
        this.onProvisionalDestinationResolved_.bind(this);
    global.failedToResolveProvisionalPrinter =
        this.failedToResolveProvisionalDestination_.bind(this);
  };

  /**
   * Event types dispatched from the Chromium native layer.
   * @enum {string}
   * @const
   */
  NativeLayer.EventType = {
    ACCESS_TOKEN_READY: 'print_preview.NativeLayer.ACCESS_TOKEN_READY',
    CAPABILITIES_SET: 'print_preview.NativeLayer.CAPABILITIES_SET',
    CLOUD_PRINT_ENABLE: 'print_preview.NativeLayer.CLOUD_PRINT_ENABLE',
    DESTINATIONS_RELOAD: 'print_preview.NativeLayer.DESTINATIONS_RELOAD',
    DISABLE_SCALING: 'print_preview.NativeLayer.DISABLE_SCALING',
    FILE_SELECTION_CANCEL: 'print_preview.NativeLayer.FILE_SELECTION_CANCEL',
    FILE_SELECTION_COMPLETE:
        'print_preview.NativeLayer.FILE_SELECTION_COMPLETE',
    GET_CAPABILITIES_FAIL: 'print_preview.NativeLayer.GET_CAPABILITIES_FAIL',
    INITIAL_SETTINGS_SET: 'print_preview.NativeLayer.INITIAL_SETTINGS_SET',
    LOCAL_DESTINATIONS_SET: 'print_preview.NativeLayer.LOCAL_DESTINATIONS_SET',
    MANIPULATE_SETTINGS_FOR_TEST:
        'print_preview.NativeLayer.MANIPULATE_SETTINGS_FOR_TEST',
    PAGE_COUNT_READY: 'print_preview.NativeLayer.PAGE_COUNT_READY',
    PAGE_LAYOUT_READY: 'print_preview.NativeLayer.PAGE_LAYOUT_READY',
    PAGE_PREVIEW_READY: 'print_preview.NativeLayer.PAGE_PREVIEW_READY',
    PREVIEW_GENERATION_DONE:
        'print_preview.NativeLayer.PREVIEW_GENERATION_DONE',
    PREVIEW_GENERATION_FAIL:
        'print_preview.NativeLayer.PREVIEW_GENERATION_FAIL',
    PRINT_TO_CLOUD: 'print_preview.NativeLayer.PRINT_TO_CLOUD',
    SETTINGS_INVALID: 'print_preview.NativeLayer.SETTINGS_INVALID',
    PRIVET_PRINTER_CHANGED: 'print_preview.NativeLayer.PRIVET_PRINTER_CHANGED',
    PRIVET_CAPABILITIES_SET:
        'print_preview.NativeLayer.PRIVET_CAPABILITIES_SET',
    PRIVET_PRINT_FAILED: 'print_preview.NativeLayer.PRIVET_PRINT_FAILED',
    EXTENSION_PRINTERS_ADDED:
        'print_preview.NativeLayer.EXTENSION_PRINTERS_ADDED',
    EXTENSION_CAPABILITIES_SET:
        'print_preview.NativeLayer.EXTENSION_CAPABILITIES_SET',
    PRINT_PRESET_OPTIONS: 'print_preview.NativeLayer.PRINT_PRESET_OPTIONS',
    PROVISIONAL_DESTINATION_RESOLVED:
        'print_preview.NativeLayer.PROVISIONAL_DESTINATION_RESOLVED'
  };

  /**
   * Constant values matching printing::DuplexMode enum.
   * @enum {number}
   */
  NativeLayer.DuplexMode = {
    SIMPLEX: 0,
    LONG_EDGE: 1,
    UNKNOWN_DUPLEX_MODE: -1
  };

  /**
   * Enumeration of color modes used by Chromium.
   * @enum {number}
   * @private
   */
  NativeLayer.ColorMode_ = {
    GRAY: 1,
    COLOR: 2
  };

  /**
   * Version of the serialized state of the print preview.
   * @type {number}
   * @const
   * @private
   */
  NativeLayer.SERIALIZED_STATE_VERSION_ = 1;

  NativeLayer.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Requests access token for cloud print requests.
     * @param {string} authType type of access token.
     */
    startGetAccessToken: function(authType) {
      chrome.send('getAccessToken', [authType]);
    },

    /** Gets the initial settings to initialize the print preview with. */
    startGetInitialSettings: function() {
      chrome.send('getInitialSettings');
    },

    /**
     * Requests the system's local print destinations. A LOCAL_DESTINATIONS_SET
     * event will be dispatched in response.
     */
    startGetLocalDestinations: function() {
      chrome.send('getPrinters');
    },

    /**
     * Requests the network's privet print destinations. A number of
     * PRIVET_PRINTER_CHANGED events will be fired in response, followed by a
     * PRIVET_SEARCH_ENDED.
     */
    startGetPrivetDestinations: function() {
      chrome.send('getPrivetPrinters');
    },

    /**
     * Requests that the privet print stack stop searching for privet print
     * destinations.
     */
    stopGetPrivetDestinations: function() {
      chrome.send('stopGetPrivetPrinters');
    },

    /**
     * Requests the privet destination's printing capabilities. A
     * PRIVET_CAPABILITIES_SET event will be dispatched in response.
     * @param {string} destinationId ID of the destination.
     */
    startGetPrivetDestinationCapabilities: function(destinationId) {
      chrome.send('getPrivetPrinterCapabilities', [destinationId]);
    },

    /**
     * Requests that extension system dispatches an event requesting the list of
     * extension managed printers.
     */
    startGetExtensionDestinations: function() {
      chrome.send('getExtensionPrinters');
    },

    /**
     * Requests an extension destination's printing capabilities. A
     * EXTENSION_CAPABILITIES_SET event will be dispatched in response.
     * @param {string} destinationId The ID of the destination whose
     *     capabilities are requested.
     */
    startGetExtensionDestinationCapabilities: function(destinationId) {
      chrome.send('getExtensionPrinterCapabilities', [destinationId]);
    },

    /**
     * Requests the destination's printing capabilities. A CAPABILITIES_SET
     * event will be dispatched in response.
     * @param {string} destinationId ID of the destination.
     */
    startGetLocalDestinationCapabilities: function(destinationId) {
      chrome.send('getPrinterCapabilities', [destinationId]);
    },

    /**
     * Requests Chrome to resolve provisional extension destination by granting
     * the provider extension access to the printer. Chrome will respond with
     * the resolved destination properties by calling
     * {@code onProvisionalPrinterResolved}, or in case of an error
     * {@code failedToResolveProvisionalPrinter}
     * @param {string} provisionalDestinationId
     */
    grantExtensionPrinterAccess: function(provisionalDestinationId) {
      chrome.send('grantExtensionPrinterAccess', [provisionalDestinationId]);
    },

    /**
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.ticket_items.Color} color Color ticket item.
     * @return {number} Native layer color model.
     * @private
     */
    getNativeColorModel_: function(destination, color) {
      // For non-local printers native color model is ignored anyway.
      var option = destination.isLocal ? color.getSelectedOption() : null;
      var nativeColorModel = parseInt(option ? option.vendor_id : null, 10);
      if (isNaN(nativeColorModel)) {
        return color.getValue() ?
            NativeLayer.ColorMode_.COLOR : NativeLayer.ColorMode_.GRAY;
      }
      return nativeColorModel;
    },

    /**
     * Requests that a preview be generated. The following events may be
     * dispatched in response:
     *   - PAGE_COUNT_READY
     *   - PAGE_LAYOUT_READY
     *   - PAGE_PREVIEW_READY
     *   - PREVIEW_GENERATION_DONE
     *   - PREVIEW_GENERATION_FAIL
     * @param {print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {number} requestId ID of the preview request.
     */
    startGetPreview: function(
        destination, printTicketStore, documentInfo, requestId) {
      assert(printTicketStore.isTicketValidForPreview(),
             'Trying to generate preview when ticket is not valid');

      var ticket = {
        'pageRange': printTicketStore.pageRange.getDocumentPageRanges(),
        'mediaSize': printTicketStore.mediaSize.getValue(),
        'landscape': printTicketStore.landscape.getValue(),
        'color': this.getNativeColorModel_(destination, printTicketStore.color),
        'headerFooterEnabled': printTicketStore.headerFooter.getValue(),
        'marginsType': printTicketStore.marginsType.getValue(),
        'isFirstRequest': requestId == 0,
        'requestID': requestId,
        'previewModifiable': documentInfo.isModifiable,
        'printToPDF':
            destination != null &&
            destination.id ==
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': destination != null && !destination.isLocal,
        'printWithPrivet': destination != null && destination.isPrivet,
        'printWithExtension': destination != null && destination.isExtension,
        'deviceName': destination == null ? 'foo' : destination.id,
        'generateDraftData': documentInfo.isModifiable,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),

        // NOTE: Even though the following fields don't directly relate to the
        // preview, they still need to be included.
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': 1,
        'collate': true,
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': printTicketStore.selectionOnly.getValue()
      };

      // Set 'cloudPrintID' only if the destination is not local.
      if (destination && !destination.isLocal) {
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.getValue() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      chrome.send(
          'getPreview',
          [JSON.stringify(ticket),
           requestId > 0 ? documentInfo.pageCount : -1,
           documentInfo.isModifiable]);
    },

    /**
     * Requests that the document be printed.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {cloudprint.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {boolean=} opt_isOpenPdfInPreview Whether to open the PDF in the
     *     system's preview application.
     * @param {boolean=} opt_showSystemDialog Whether to open system dialog for
     *     advanced settings.
     */
    startPrint: function(destination, printTicketStore, cloudPrintInterface,
                         documentInfo, opt_isOpenPdfInPreview,
                         opt_showSystemDialog) {
      assert(printTicketStore.isTicketValid(),
             'Trying to print when ticket is not valid');

      assert(!opt_showSystemDialog || (cr.isWindows && destination.isLocal),
             'Implemented for Windows only');

      var ticket = {
        'pageRange': printTicketStore.pageRange.getDocumentPageRanges(),
        'mediaSize': printTicketStore.mediaSize.getValue(),
        'pageCount': printTicketStore.pageRange.getPageNumberSet().size,
        'landscape': printTicketStore.landscape.getValue(),
        'color': this.getNativeColorModel_(destination, printTicketStore.color),
        'headerFooterEnabled': printTicketStore.headerFooter.getValue(),
        'marginsType': printTicketStore.marginsType.getValue(),
        'generateDraftData': true, // TODO(rltoscano): What should this be?
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.copies.getValueAsNumber(),
        'collate': printTicketStore.collate.getValue(),
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': printTicketStore.selectionOnly.getValue(),
        'previewModifiable': documentInfo.isModifiable,
        'printToPDF': destination.id ==
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': !destination.isLocal,
        'printWithPrivet': destination.isPrivet,
        'printWithExtension': destination.isExtension,
        'deviceName': destination.id,
        'isFirstRequest': false,
        'requestID': -1,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),
        'pageWidth': documentInfo.pageSize.width,
        'pageHeight': documentInfo.pageSize.height,
        'showSystemDialog': opt_showSystemDialog
      };

      if (!destination.isLocal) {
        // We can't set cloudPrintID if the destination is "Print with Cloud
        // Print" because the native system will try to print to Google Cloud
        // Print with this ID instead of opening a Google Cloud Print dialog.
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.isValueEqual(
              print_preview.ticket_items.MarginsType.Value.CUSTOM)) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      if (destination.isPrivet || destination.isExtension) {
        ticket['ticket'] = printTicketStore.createPrintTicket(destination);
        ticket['capabilities'] = JSON.stringify(destination.capabilities);
      }

      if (opt_isOpenPdfInPreview) {
        ticket['OpenPDFInPreview'] = true;
      }

      chrome.send('print', [JSON.stringify(ticket)]);
    },

    /** Requests that the current pending print request be cancelled. */
    startCancelPendingPrint: function() {
      chrome.send('cancelPendingPrintRequest');
    },

    /** Shows the system's native printing dialog. */
    startShowSystemDialog: function() {
      assert(!cr.isWindows);
      chrome.send('showSystemDialog');
    },

    /** Closes the print preview dialog. */
    startCloseDialog: function() {
      chrome.send('closePrintPreviewDialog');
      chrome.send('dialogClose');
    },

    /** Hide the print preview dialog and allow the native layer to close it. */
    startHideDialog: function() {
      chrome.send('hidePreview');
    },

    /**
     * Opens the Google Cloud Print sign-in tab. The DESTINATIONS_RELOAD event
     *     will be dispatched in response.
     * @param {boolean} addAccount Whether to open an 'add a new account' or
     *     default sign in page.
     */
    startCloudPrintSignIn: function(addAccount) {
      chrome.send('signIn', [addAccount]);
    },

    /** Navigates the user to the system printer settings interface. */
    startManageLocalDestinations: function() {
      chrome.send('manageLocalPrinters');
    },

    /**
     * Navigates the user to the Google Cloud Print management page.
     * @param {?string} user Email address of the user to open the management
     *     page for (user must be currently logged in, indeed) or {@code null}
     *     to open this page for the primary user.
     */
    startManageCloudDestinations: function(user) {
      chrome.send('manageCloudPrinters', [user || '']);
    },

    /** Forces browser to open a new tab with the given URL address. */
    startForceOpenNewTab: function(url) {
      chrome.send('forceOpenNewTab', [url]);
    },

    /**
     * @param {!Object} initialSettings Object containing all initial settings.
     */
    onSetInitialSettings_: function(initialSettings) {
      var numberFormatSymbols =
          print_preview.MeasurementSystem.parseNumberFormat(
              initialSettings['numberFormat']);
      var unitType = print_preview.MeasurementSystem.UnitType.IMPERIAL;
      if (initialSettings['measurementSystem'] != null) {
        unitType = initialSettings['measurementSystem'];
      }

      var nativeInitialSettings = new print_preview.NativeInitialSettings(
          initialSettings['printAutomaticallyInKioskMode'] || false,
          initialSettings['appKioskMode'] || false,
          numberFormatSymbols[0] || ',',
          numberFormatSymbols[1] || '.',
          unitType,
          initialSettings['previewModifiable'] || false,
          initialSettings['initiatorTitle'] || '',
          initialSettings['documentHasSelection'] || false,
          initialSettings['shouldPrintSelectionOnly'] || false,
          initialSettings['printerName'] || null,
          initialSettings['appState'] || null,
          initialSettings['defaultDestinationSelectionRules'] || null);

      var initialSettingsSetEvent = new Event(
          NativeLayer.EventType.INITIAL_SETTINGS_SET);
      initialSettingsSetEvent.initialSettings = nativeInitialSettings;
      this.dispatchEvent(initialSettingsSetEvent);
    },

    /**
     * Turn on the integration of Cloud Print.
     * @param {{cloudPrintURL: string, appKioskMode: string}} settings
     *     cloudPrintUrl: The URL to use for cloud print servers.
     * @private
     */
    onSetUseCloudPrint_: function(settings) {
      var cloudPrintEnableEvent = new Event(
          NativeLayer.EventType.CLOUD_PRINT_ENABLE);
      cloudPrintEnableEvent.baseCloudPrintUrl = settings['cloudPrintUrl'] || '';
      cloudPrintEnableEvent.appKioskMode = settings['appKioskMode'] || false;
      this.dispatchEvent(cloudPrintEnableEvent);
    },

    /**
     * Updates the print preview with local printers.
     * Called from PrintPreviewHandler::SetupPrinterList().
     * @param {Array} printers Array of printer info objects.
     * @private
     */
    onSetPrinters_: function(printers) {
      var localDestsSetEvent = new Event(
          NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
      localDestsSetEvent.destinationInfos = printers;
      this.dispatchEvent(localDestsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {Object} settingsInfo printer setting information.
     * @private
     */
    onUpdateWithPrinterCapabilities_: function(settingsInfo) {
      var capsSetEvent = new Event(NativeLayer.EventType.CAPABILITIES_SET);
      capsSetEvent.settingsInfo = settingsInfo;
      this.dispatchEvent(capsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {string} destinationId Printer affected by error.
     * @private
     */
    onFailedToGetPrinterCapabilities_: function(destinationId) {
      var getCapsFailEvent = new Event(
          NativeLayer.EventType.GET_CAPABILITIES_FAIL);
      getCapsFailEvent.destinationId = destinationId;
      getCapsFailEvent.destinationOrigin =
          print_preview.Destination.Origin.LOCAL;
      this.dispatchEvent(getCapsFailEvent);
    },

    /**
     * Called when native layer gets settings information for a requested privet
     * destination.
     * @param {string} destinationId Printer affected by error.
     * @private
     */
    onFailedToGetPrivetPrinterCapabilities_: function(destinationId) {
      var getCapsFailEvent = new Event(
          NativeLayer.EventType.GET_CAPABILITIES_FAIL);
      getCapsFailEvent.destinationId = destinationId;
      getCapsFailEvent.destinationOrigin =
          print_preview.Destination.Origin.PRIVET;
      this.dispatchEvent(getCapsFailEvent);
    },

    /**
     * Called when native layer fails to get settings information for a
     * requested extension destination.
     * @param {string} destinationId Printer affected by error.
     * @private
     */
    onFailedToGetExtensionPrinterCapabilities_: function(destinationId) {
      var getCapsFailEvent = new Event(
          NativeLayer.EventType.GET_CAPABILITIES_FAIL);
      getCapsFailEvent.destinationId = destinationId;
      getCapsFailEvent.destinationOrigin =
          print_preview.Destination.Origin.EXTENSION;
      this.dispatchEvent(getCapsFailEvent);
    },

    /** Reloads the printer list. */
    onReloadPrintersList_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DESTINATIONS_RELOAD);
    },

    /**
     * Called from the C++ layer.
     * Take the PDF data handed to us and submit it to the cloud, closing the
     * print preview dialog once the upload is successful.
     * @param {string} data Data to send as the print job.
     * @private
     */
    onPrintToCloud_: function(data) {
      var printToCloudEvent = new Event(
          NativeLayer.EventType.PRINT_TO_CLOUD);
      printToCloudEvent.data = data;
      this.dispatchEvent(printToCloudEvent);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
     * preview dialog regarding the file selection cancel event.
     * @private
     */
    onFileSelectionCancelled_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.FILE_SELECTION_CANCEL);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCompleted to notify the print
     * preview dialog regarding the file selection completed event.
     * @private
     */
    onFileSelectionCompleted_: function() {
      // If the file selection is completed and the dialog is not already closed
      // it means that a pending print to pdf request exists.
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.FILE_SELECTION_COMPLETE);
    },

    /**
     * Display an error message when print preview fails.
     * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
     * @private
     */
    onPrintPreviewFailed_: function() {
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.PREVIEW_GENERATION_FAIL);
    },

    /**
     * Display an error message when encountered invalid printer settings.
     * Called from PrintPreviewMessageHandler::OnInvalidPrinterSettings().
     * @private
     */
    onInvalidPrinterSettings_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.SETTINGS_INVALID);
    },

    /**
     * @param {{contentWidth: number, contentHeight: number, marginLeft: number,
     *          marginRight: number, marginTop: number, marginBottom: number,
     *          printableAreaX: number, printableAreaY: number,
     *          printableAreaWidth: number, printableAreaHeight: number}}
     *          pageLayout Specifies default page layout details in points.
     * @param {boolean} hasCustomPageSizeStyle Indicates whether the previewed
     *     document has a custom page size style.
     * @private
     */
    onDidGetDefaultPageLayout_: function(pageLayout, hasCustomPageSizeStyle) {
      var pageLayoutChangeEvent = new Event(
          NativeLayer.EventType.PAGE_LAYOUT_READY);
      pageLayoutChangeEvent.pageLayout = pageLayout;
      pageLayoutChangeEvent.hasCustomPageSizeStyle = hasCustomPageSizeStyle;
      this.dispatchEvent(pageLayoutChangeEvent);
    },

    /**
     * Update the page count and check the page range.
     * Called from PrintPreviewUI::OnDidGetPreviewPageCount().
     * @param {number} pageCount The number of pages.
     * @param {number} previewResponseId The preview request id that resulted in
     *      this response.
     * @private
     */
    onDidGetPreviewPageCount_: function(pageCount, previewResponseId) {
      var pageCountChangeEvent = new Event(
          NativeLayer.EventType.PAGE_COUNT_READY);
      pageCountChangeEvent.pageCount = pageCount;
      pageCountChangeEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pageCountChangeEvent);
    },

    /**
     * Notification that a print preview page has been rendered.
     * Check if the settings have changed and request a regeneration if needed.
     * Called from PrintPreviewUI::OnDidPreviewPage().
     * @param {number} pageNumber The page number, 0-based.
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onDidPreviewPage_: function(pageNumber, previewUid, previewResponseId) {
      var pagePreviewGenEvent = new Event(
          NativeLayer.EventType.PAGE_PREVIEW_READY);
      pagePreviewGenEvent.pageIndex = pageNumber;
      pagePreviewGenEvent.previewUid = previewUid;
      pagePreviewGenEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pagePreviewGenEvent);
    },

    /**
     * Notification that access token is ready.
     * @param {string} authType Type of access token.
     * @param {string} accessToken Access token.
     * @private
     */
    onDidGetAccessToken_: function(authType, accessToken) {
      var getAccessTokenEvent = new Event(
          NativeLayer.EventType.ACCESS_TOKEN_READY);
      getAccessTokenEvent.authType = authType;
      getAccessTokenEvent.accessToken = accessToken;
      this.dispatchEvent(getAccessTokenEvent);
    },

    /**
     * Update the print preview when new preview data is available.
     * Create the PDF plugin as needed.
     * Called from PrintPreviewUI::PreviewDataIsAvailable().
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onUpdatePrintPreview_: function(previewUid, previewResponseId) {
      var previewGenDoneEvent = new Event(
          NativeLayer.EventType.PREVIEW_GENERATION_DONE);
      previewGenDoneEvent.previewUid = previewUid;
      previewGenDoneEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(previewGenDoneEvent);
    },

    /**
     * Updates print preset options from source PDF document.
     * Called from PrintPreviewUI::OnSetOptionsFromDocument().
     * @param {{disableScaling: boolean, copies: number,
     *          duplex: number}} options Specifies
     *     printing options according to source document presets.
     * @private
     */
    onPrintPresetOptionsFromDocument_: function(options) {
      var printPresetOptionsEvent = new Event(
          NativeLayer.EventType.PRINT_PRESET_OPTIONS);
      printPresetOptionsEvent.optionsFromDocument = options;
      this.dispatchEvent(printPresetOptionsEvent);
    },

    /**
     * Simulates a user click on the print preview dialog cancel button. Used
     * only for testing.
     * @private
     */
    autoCancelForTesting_: function() {
      var properties = {view: window, bubbles: true, cancelable: true};
      var click = new MouseEvent('click', properties);
      document.querySelector('#print-header .cancel').dispatchEvent(click);
    },

    /**
     * @param {{serviceName: string, name: string}} printer Specifies
     *     information about the printer that was added.
     * @private
     */
    onPrivetPrinterChanged_: function(printer) {
      var privetPrinterChangedEvent =
            new Event(NativeLayer.EventType.PRIVET_PRINTER_CHANGED);
      privetPrinterChangedEvent.printer = printer;
      this.dispatchEvent(privetPrinterChangedEvent);
    },

    /**
     * @param {Object} printer Specifies information about the printer that was
     *    added.
     * @private
     */
    onPrivetCapabilitiesSet_: function(printer, capabilities) {
      var privetCapabilitiesSetEvent =
            new Event(NativeLayer.EventType.PRIVET_CAPABILITIES_SET);
      privetCapabilitiesSetEvent.printer = printer;
      privetCapabilitiesSetEvent.capabilities = capabilities;
      this.dispatchEvent(privetCapabilitiesSetEvent);
    },

    /**
     * @param {string} http_error The HTTP response code or -1 if not an HTTP
     *    error.
     * @private
     */
    onPrivetPrintFailed_: function(http_error) {
      var privetPrintFailedEvent =
            new Event(NativeLayer.EventType.PRIVET_PRINT_FAILED);
      privetPrintFailedEvent.httpError = http_error;
      this.dispatchEvent(privetPrintFailedEvent);
    },

    /**
     * @param {Array<!{extensionId: string,
     *                 extensionName: string,
     *                 id: string,
     *                 name: string,
     *                 description: (string|undefined),
     *                 provisional: (boolean|undefined)}>} printers The list
     *     containing information about printers added by an extension.
     * @param {boolean} done Whether this is the final list of extension
     *     managed printers.
     */
    onExtensionPrintersAdded_: function(printers, done) {
      var event = new Event(NativeLayer.EventType.EXTENSION_PRINTERS_ADDED);
      event.printers = printers;
      event.done = done;
      this.dispatchEvent(event);
    },

    /**
     * Called when an extension responds to a request for an extension printer
     * capabilities.
     * @param {string} printerId The printer's ID.
     * @param {!Object} capabilities The reported printer capabilities.
     */
    onExtensionCapabilitiesSet_: function(printerId,
                                          capabilities) {
      var event = new Event(NativeLayer.EventType.EXTENSION_CAPABILITIES_SET);
      event.printerId = printerId;
      event.capabilities = capabilities;
      this.dispatchEvent(event);
    },

    /**
     * Called when Chrome reports that attempt to resolve a provisional
     * destination failed.
     * @param {string} destinationId The provisional destination ID.
     * @private
     */
    failedToResolveProvisionalDestination_: function(destinationId) {
      var evt = new Event(
          NativeLayer.EventType.PROVISIONAL_DESTINATION_RESOLVED);
      evt.provisionalId = destinationId;
      evt.destination = null;
      this.dispatchEvent(evt);
    },

    /**
     * Called when Chrome reports that a provisional destination has been
     * successfully resolved.
     * Currently used only for extension provided destinations.
     * @param {string} provisionalDestinationId The provisional destination id.
     * @param {!{extensionId: string,
     *           extensionName: string,
     *           id: string,
     *           name: string,
     *           description: (string|undefined)}} destinationInfo The resolved
     *     destination info.
     * @private
     */
    onProvisionalDestinationResolved_: function(provisionalDestinationId,
                                                destinationInfo) {
      var evt = new Event(
          NativeLayer.EventType.PROVISIONAL_DESTINATION_RESOLVED);
      evt.provisionalId = provisionalDestinationId;
      evt.destination = destinationInfo;
      this.dispatchEvent(evt);
    },

   /**
     * Allows for onManipulateSettings to be called
     * from the native layer.
     * @private
     */
    onEnableManipulateSettingsForTest_: function() {
      global.onManipulateSettingsForTest =
          this.onManipulateSettingsForTest_.bind(this);
    },

    /**
     * Dispatches an event to print_preview.js to change
     * a particular setting for print preview.
     * @param {!print_preview.PreviewSettings} settings Object containing the
     *     value to be changed and that value should be set to.
     * @private
     */
    onManipulateSettingsForTest_: function(settings) {
      var manipulateSettingsEvent =
          new Event(NativeLayer.EventType.MANIPULATE_SETTINGS_FOR_TEST);
      manipulateSettingsEvent.settings = settings;
      this.dispatchEvent(manipulateSettingsEvent);
    },

    /**
     * Sends a message to the test, letting it know that an
     * option has been set to a particular value and that the change has
     * finished modifying the preview area.
     */
    previewReadyForTest: function() {
      if (global.onManipulateSettingsForTest)
        chrome.send('UILoadedForTest');
    },

    /**
     * Notifies the test that the option it tried to change
     * had not been changed successfully.
     */
    previewFailedForTest: function() {
      if (global.onManipulateSettingsForTest)
        chrome.send('UIFailedLoadingForTest');
    }
  };

  /**
   * Initial settings retrieved from the native layer.
   * @param {boolean} isInKioskAutoPrintMode Whether the print preview should be
   *     in auto-print mode.
   * @param {boolean} isInAppKioskMode Whether the print preview is in App Kiosk
   *     mode.
   * @param {string} thousandsDelimeter Character delimeter of thousands digits.
   * @param {string} decimalDelimeter Character delimeter of the decimal point.
   * @param {!print_preview.MeasurementSystem.UnitType} unitType Unit type of
   *     local machine's measurement system.
   * @param {boolean} isDocumentModifiable Whether the document to print is
   *     modifiable.
   * @param {string} documentTitle Title of the document.
   * @param {boolean} documentHasSelection Whether the document has selected
   *     content.
   * @param {boolean} selectionOnly Whether only selected content should be
   *     printed.
   * @param {?string} systemDefaultDestinationId ID of the system default
   *     destination.
   * @param {?string} serializedAppStateStr Serialized app state.
   * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
   *     default destination selection rules.
   * @constructor
   */
  function NativeInitialSettings(
      isInKioskAutoPrintMode,
      isInAppKioskMode,
      thousandsDelimeter,
      decimalDelimeter,
      unitType,
      isDocumentModifiable,
      documentTitle,
      documentHasSelection,
      selectionOnly,
      systemDefaultDestinationId,
      serializedAppStateStr,
      serializedDefaultDestinationSelectionRulesStr) {

    /**
     * Whether the print preview should be in auto-print mode.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = isInKioskAutoPrintMode;

    /**
     * Whether the print preview should switch to App Kiosk mode.
     * @type {boolean}
     * @private
     */
    this.isInAppKioskMode_ = isInAppKioskMode;

    /**
     * Character delimeter of thousands digits.
     * @type {string}
     * @private
     */
    this.thousandsDelimeter_ = thousandsDelimeter;

    /**
     * Character delimeter of the decimal point.
     * @type {string}
     * @private
     */
    this.decimalDelimeter_ = decimalDelimeter;

    /**
     * Unit type of local machine's measurement system.
     * @type {string}
     * @private
     */
    this.unitType_ = unitType;

    /**
     * Whether the document to print is modifiable.
     * @type {boolean}
     * @private
     */
    this.isDocumentModifiable_ = isDocumentModifiable;

    /**
     * Title of the document.
     * @type {string}
     * @private
     */
    this.documentTitle_ = documentTitle;

    /**
     * Whether the document has selection.
     * @type {string}
     * @private
     */
    this.documentHasSelection_ = documentHasSelection;

    /**
     * Whether selection only should be printed.
     * @type {string}
     * @private
     */
    this.selectionOnly_ = selectionOnly;

    /**
     * ID of the system default destination.
     * @type {?string}
     * @private
     */
    this.systemDefaultDestinationId_ = systemDefaultDestinationId;

    /**
     * Serialized app state.
     * @type {?string}
     * @private
     */
    this.serializedAppStateStr_ = serializedAppStateStr;

    /**
     * Serialized default destination selection rules.
     * @type {?string}
     * @private
     */
    this.serializedDefaultDestinationSelectionRulesStr_ =
        serializedDefaultDestinationSelectionRulesStr;
  };

  NativeInitialSettings.prototype = {
    /**
     * @return {boolean} Whether the print preview should be in auto-print mode.
     */
    get isInKioskAutoPrintMode() {
      return this.isInKioskAutoPrintMode_;
    },

    /**
     * @return {boolean} Whether the print preview should switch to App Kiosk
     *     mode.
     */
    get isInAppKioskMode() {
      return this.isInAppKioskMode_;
    },

    /** @return {string} Character delimeter of thousands digits. */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    },

    /** @return {string} Character delimeter of the decimal point. */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    },

    /**
     * @return {!print_preview.MeasurementSystem.UnitType} Unit type of local
     *     machine's measurement system.
     */
    get unitType() {
      return this.unitType_;
    },

    /** @return {boolean} Whether the document to print is modifiable. */
    get isDocumentModifiable() {
      return this.isDocumentModifiable_;
    },

    /** @return {string} Document title. */
    get documentTitle() {
      return this.documentTitle_;
    },

    /** @return {boolean} Whether the document has selection. */
    get documentHasSelection() {
      return this.documentHasSelection_;
    },

    /** @return {boolean} Whether selection only should be printed. */
    get selectionOnly() {
      return this.selectionOnly_;
    },

    /** @return {?string} ID of the system default destination. */
    get systemDefaultDestinationId() {
      return this.systemDefaultDestinationId_;
    },

    /** @return {?string} Serialized app state. */
    get serializedAppStateStr() {
      return this.serializedAppStateStr_;
    },

    /** @return {?string} Serialized default destination selection rules. */
    get serializedDefaultDestinationSelectionRulesStr() {
      return this.serializedDefaultDestinationSelectionRulesStr_;
    }
  };

  // Export
  return {
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer
  };
});
