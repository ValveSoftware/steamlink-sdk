// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    global['setInitialSettings'] = this.onSetInitialSettings_.bind(this);
    global['setUseCloudPrint'] = this.onSetUseCloudPrint_.bind(this);
    global['setPrinters'] = this.onSetPrinters_.bind(this);
    global['updateWithPrinterCapabilities'] =
        this.onUpdateWithPrinterCapabilities_.bind(this);
    global['failedToGetPrinterCapabilities'] =
        this.onFailedToGetPrinterCapabilities_.bind(this);
    global['failedToGetPrivetPrinterCapabilities'] =
      this.onFailedToGetPrivetPrinterCapabilities_.bind(this);
    global['reloadPrintersList'] = this.onReloadPrintersList_.bind(this);
    global['printToCloud'] = this.onPrintToCloud_.bind(this);
    global['fileSelectionCancelled'] =
        this.onFileSelectionCancelled_.bind(this);
    global['fileSelectionCompleted'] =
        this.onFileSelectionCompleted_.bind(this);
    global['printPreviewFailed'] = this.onPrintPreviewFailed_.bind(this);
    global['invalidPrinterSettings'] =
        this.onInvalidPrinterSettings_.bind(this);
    global['onDidGetDefaultPageLayout'] =
        this.onDidGetDefaultPageLayout_.bind(this);
    global['onDidGetPreviewPageCount'] =
        this.onDidGetPreviewPageCount_.bind(this);
    global['onDidPreviewPage'] = this.onDidPreviewPage_.bind(this);
    global['updatePrintPreview'] = this.onUpdatePrintPreview_.bind(this);
    global['printScalingDisabledForSourcePDF'] =
        this.onPrintScalingDisabledForSourcePDF_.bind(this);
    global['onDidGetAccessToken'] = this.onDidGetAccessToken_.bind(this);
    global['autoCancelForTesting'] = this.autoCancelForTesting_.bind(this);
    global['onPrivetPrinterChanged'] = this.onPrivetPrinterChanged_.bind(this);
    global['onPrivetCapabilitiesSet'] =
        this.onPrivetCapabilitiesSet_.bind(this);
    global['onPrivetPrintFailed'] = this.onPrivetPrintFailed_.bind(this);
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
    PRIVET_PRINT_FAILED: 'print_preview.NativeLayer.PRIVET_PRINT_FAILED'
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
     * Requests the destination's printing capabilities. A CAPABILITIES_SET
     * event will be dispatched in response.
     * @param {string} destinationId ID of the destination.
     */
    startGetLocalDestinationCapabilities: function(destinationId) {
      chrome.send('getPrinterCapabilities', [destinationId]);
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
      var nativeColorModel = parseInt(option ? option.vendor_id : null);
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
     * @param {number} ID of the preview request.
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
        'deviceName': destination == null ? 'foo' : destination.id,
        'generateDraftData': documentInfo.isModifiable,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),

        // NOTE: Even though the following fields don't directly relate to the
        // preview, they still need to be included.
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.copies.getValueAsNumber(),
        'collate': printTicketStore.collate.getValue(),
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
     * @param {print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {boolean=} opt_isOpenPdfInPreview Whether to open the PDF in the
     *     system's preview application.
     */
    startPrint: function(destination, printTicketStore, cloudPrintInterface,
                         documentInfo, opt_isOpenPdfInPreview) {
      assert(printTicketStore.isTicketValid(),
             'Trying to print when ticket is not valid');

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
        'deviceName': destination.id,
        'isFirstRequest': false,
        'requestID': -1,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),
        'pageWidth': documentInfo.pageSize.width,
        'pageHeight': documentInfo.pageSize.height
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

      if (destination.isPrivet) {
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
      chrome.send('showSystemDialog');
    },

    /** Shows Google Cloud Print's web-based print dialog.
     * @param {number} pageCount Number of pages to print.
     */
    startShowCloudPrintDialog: function(pageCount) {
      chrome.send('printWithCloudPrintDialog', [pageCount]);
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

    /** Navigates the user to the Google Cloud Print management page. */
    startManageCloudDestinations: function() {
      chrome.send('manageCloudPrinters');
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
          initialSettings['hidePrintWithSystemDialogLink'] || false,
          numberFormatSymbols[0] || ',',
          numberFormatSymbols[1] || '.',
          unitType,
          initialSettings['previewModifiable'] || false,
          initialSettings['initiatorTitle'] || '',
          initialSettings['documentHasSelection'] || false,
          initialSettings['shouldPrintSelectionOnly'] || false,
          initialSettings['printerName'] || null,
          initialSettings['appState'] || null);

      var initialSettingsSetEvent = new Event(
          NativeLayer.EventType.INITIAL_SETTINGS_SET);
      initialSettingsSetEvent.initialSettings = nativeInitialSettings;
      this.dispatchEvent(initialSettingsSetEvent);
    },

    /**
     * Turn on the integration of Cloud Print.
     * @param {string} cloudPrintURL The URL to use for cloud print servers.
     * @private
     */
    onSetUseCloudPrint_: function(cloudPrintURL) {
      var cloudPrintEnableEvent = new Event(
          NativeLayer.EventType.CLOUD_PRINT_ENABLE);
      cloudPrintEnableEvent.baseCloudPrintUrl = cloudPrintURL;
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
     * @param {string} printerId printer affected by error.
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
     * @param {string} printerId printer affected by error.
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
     * Updates the fit to page option state based on the print scaling option of
     * source pdf. PDF's have an option to enable/disable print scaling. When we
     * find out that the print scaling option is disabled for the source pdf, we
     * uncheck the fitToPage_ to page checkbox. This function is called from C++
     * code.
     * @private
     */
    onPrintScalingDisabledForSourcePDF_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DISABLE_SCALING);
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
    }
  };

  /**
   * Initial settings retrieved from the native layer.
   * @param {boolean} isInKioskAutoPrintMode Whether the print preview should be
   *     in auto-print mode.
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
   * @constructor
   */
  function NativeInitialSettings(
      isInKioskAutoPrintMode,
      hidePrintWithSystemDialogLink,
      thousandsDelimeter,
      decimalDelimeter,
      unitType,
      isDocumentModifiable,
      documentTitle,
      documentHasSelection,
      selectionOnly,
      systemDefaultDestinationId,
      serializedAppStateStr) {

    /**
     * Whether the print preview should be in auto-print mode.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = isInKioskAutoPrintMode;

    /**
     * Whether we should hide the link which shows the system print dialog.
     * @type {boolean}
     * @private
     */
    this.hidePrintWithSystemDialogLink_ = hidePrintWithSystemDialogLink;

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
  };

  NativeInitialSettings.prototype = {
    /**
     * @return {boolean} Whether the print preview should be in auto-print mode.
     */
    get isInKioskAutoPrintMode() {
      return this.isInKioskAutoPrintMode_;
    },

    /**
     * @return {boolean} Whether we should hide the link which shows the
           system print dialog.
     */
    get hidePrintWithSystemDialogLink() {
      return this.hidePrintWithSystemDialogLink_;
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

    /** @return {bool} Whether the document has selection. */
    get documentHasSelection() {
      return this.documentHasSelection_;
    },

    /** @return {bool} Whether selection only should be printed. */
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
    }
  };

  // Export
  return {
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer
  };
});
