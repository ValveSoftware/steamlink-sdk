// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{accessibility: Function,
 *            documentLoadComplete: Function,
 *            getHeight: Function,
 *            getHorizontalScrollbarThickness: Function,
 *            getPageLocationNormalized: Function,
 *            getVerticalScrollbarThickness: Function,
 *            getWidth: Function,
 *            getZoomLevel: Function,
 *            goToPage: Function,
 *            grayscale: Function,
 *            loadPreviewPage: Function,
 *            onload: Function,
 *            onPluginSizeChanged: Function,
 *            onScroll: Function,
 *            pageXOffset: Function,
 *            pageYOffset: Function,
 *            printPreviewPageCount: Function,
 *            reload: Function,
 *            removePrintButton: Function,
 *            resetPrintPreviewUrl: Function,
 *            sendKeyEvent: Function,
 *            setPageNumbers: Function,
 *            setPageXOffset: Function,
 *            setPageYOffset: Function,
 *            setZoomLevel: Function,
 *            fitToHeight: Function,
 *            fitToWidth: Function,
 *            zoomIn: Function,
 *            zoomOut: Function}}
 */
print_preview.PDFPlugin;

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PreviewArea object. It represents the area where the preview
   * document is displayed.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     currently selected destination.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to get
   *     information about how the preview should be displayed.
   * @param {!print_preview.NativeLayer} nativeLayer Needed to communicate with
   *     Chromium's preview generation system.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PreviewArea(
      destinationStore, printTicketStore, nativeLayer, documentInfo) {
    print_preview.Component.call(this);
    // TODO(rltoscano): Understand the dependencies of printTicketStore needed
    // here, and add only those here (not the entire print ticket store).

    /**
     * Used to get the currently selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Used to get information about how the preview should be displayed.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Used to contruct the preview generator.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Document data model.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Used to read generated page previews.
     * @type {print_preview.PreviewGenerator}
     * @private
     */
    this.previewGenerator_ = null;

    /**
     * The embedded pdf plugin object. It's value is null if not yet loaded.
     * @type {HTMLEmbedElement|print_preview.PDFPlugin}
     * @private
     */
    this.plugin_ = null;

    /**
     * Custom margins component superimposed on the preview plugin.
     * @type {!print_preview.MarginControlContainer}
     * @private
     */
    this.marginControlContainer_ = new print_preview.MarginControlContainer(
        this.documentInfo_,
        this.printTicketStore_.marginsType,
        this.printTicketStore_.customMargins,
        this.printTicketStore_.measurementSystem,
        this.onMarginDragChanged_.bind(this));
    this.addChild(this.marginControlContainer_);

    /**
     * Current zoom level as a percentage.
     * @type {?number}
     * @private
     */
    this.zoomLevel_ = null;

    /**
     * Current page offset which can be used to calculate scroll amount.
     * @type {print_preview.Coordinate2d}
     * @private
     */
    this.pageOffset_ = null;

    /**
     * Whether the plugin has finished reloading.
     * @type {boolean}
     * @private
     */
    this.isPluginReloaded_ = false;

    /**
     * Whether the document preview is ready.
     * @type {boolean}
     * @private
     */
    this.isDocumentReady_ = false;

    /**
     * Timeout object used to display a loading message if the preview is taking
     * a long time to generate.
     * @type {?number}
     * @private
     */
    this.loadingTimeout_ = null;

    /**
     * Overlay element.
     * @type {HTMLElement}
     * @private
     */
    this.overlayEl_ = null;

    /**
     * The "Open system dialog" button.
     * @type {HTMLButtonElement}
     * @private
     */
    this.openSystemDialogButton_ = null;
  };

  /**
   * Event types dispatched by the preview area.
   * @enum {string}
   */
  PreviewArea.EventType = {
    // Dispatched when the "Open system dialog" button is clicked.
    OPEN_SYSTEM_DIALOG_CLICK:
        'print_preview.PreviewArea.OPEN_SYSTEM_DIALOG_CLICK',

    // Dispatched when the document preview is complete.
    PREVIEW_GENERATION_DONE:
        'print_preview.PreviewArea.PREVIEW_GENERATION_DONE',

    // Dispatched when the document preview failed to be generated.
    PREVIEW_GENERATION_FAIL:
        'print_preview.PreviewArea.PREVIEW_GENERATION_FAIL',

    // Dispatched when a new document preview is being generated.
    PREVIEW_GENERATION_IN_PROGRESS:
        'print_preview.PreviewArea.PREVIEW_GENERATION_IN_PROGRESS'
  };

  /**
   * CSS classes used by the preview area.
   * @enum {string}
   * @private
   */
  PreviewArea.Classes_ = {
    OUT_OF_PROCESS_COMPATIBILITY_OBJECT:
        'preview-area-compatibility-object-out-of-process',
    CUSTOM_MESSAGE_TEXT: 'preview-area-custom-message-text',
    MESSAGE: 'preview-area-message',
    INVISIBLE: 'invisible',
    OPEN_SYSTEM_DIALOG_BUTTON: 'preview-area-open-system-dialog-button',
    OPEN_SYSTEM_DIALOG_BUTTON_THROBBER:
        'preview-area-open-system-dialog-button-throbber',
    OVERLAY: 'preview-area-overlay-layer',
    MARGIN_CONTROL: 'margin-control',
    PREVIEW_AREA: 'preview-area-plugin-wrapper'
  };

  /**
   * Enumeration of IDs shown in the preview area.
   * @enum {string}
   * @private
   */
  PreviewArea.MessageId_ = {
    CUSTOM: 'custom',
    LOADING: 'loading',
    PREVIEW_FAILED: 'preview-failed'
  };

  /**
   * Maps message IDs to the CSS class that contains them.
   * @type {Object<print_preview.PreviewArea.MessageId_, string>}
   * @private
   */
  PreviewArea.MessageIdClassMap_ = {};
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.CUSTOM] =
      'preview-area-custom-message';
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.LOADING] =
      'preview-area-loading-message';
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.PREVIEW_FAILED] =
      'preview-area-preview-failed-message';

  /**
   * Amount of time in milliseconds to wait after issueing a new preview before
   * the loading message is shown.
   * @type {number}
   * @const
   * @private
   */
  PreviewArea.LOADING_TIMEOUT_ = 200;

  PreviewArea.prototype = {
    __proto__: print_preview.Component.prototype,

    /**
     * Should only be called after calling this.render().
     * @return {boolean} Whether the preview area has a compatible plugin to
     *     display the print preview in.
     */
    get hasCompatiblePlugin() {
      return this.previewGenerator_ != null;
    },

    /**
     * Processes a keyboard event that could possibly be used to change state of
     * the preview plugin.
     * @param {KeyboardEvent} e Keyboard event to process.
     */
    handleDirectionalKeyEvent: function(e) {
      // Make sure the PDF plugin is there.
      // We only care about: PageUp, PageDown, Left, Up, Right, Down.
      // If the user is holding a modifier key, ignore.
      if (!this.plugin_ ||
          !arrayContains([33, 34, 37, 38, 39, 40], e.keyCode) ||
          e.metaKey || e.altKey || e.shiftKey || e.ctrlKey) {
        return;
      }

      // Don't handle the key event for these elements.
      var tagName = document.activeElement.tagName;
      if (arrayContains(['INPUT', 'SELECT', 'EMBED'], tagName)) {
        return;
      }

      // For the most part, if any div of header was the last clicked element,
      // then the active element is the body. Starting with the last clicked
      // element, and work up the DOM tree to see if any element has a
      // scrollbar. If there exists a scrollbar, do not handle the key event
      // here.
      var element = e.target;
      while (element) {
        if (element.scrollHeight > element.clientHeight ||
            element.scrollWidth > element.clientWidth) {
          return;
        }
        element = element.parentElement;
      }

      // No scroll bar anywhere, or the active element is something else, like a
      // button. Note: buttons have a bigger scrollHeight than clientHeight.
      this.plugin_.sendKeyEvent(e);
      e.preventDefault();
    },

    /**
     * Set a callback that gets called when a key event is received that
     * originates in the plugin.
     * @param {function(Event)} callback The callback to be called with a key
     *     event.
     */
    setPluginKeyEventCallback: function(callback) {
      this.keyEventCallback_ = callback;
    },

    /**
     * Shows a custom message on the preview area's overlay.
     * @param {string} message Custom message to show.
     */
    showCustomMessage: function(message) {
      this.showMessage_(PreviewArea.MessageId_.CUSTOM, message);
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          assert(this.openSystemDialogButton_),
          'click',
          this.onOpenSystemDialogButtonClick_.bind(this));

      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onTicketChange_.bind(this));

      this.tracker.add(
          this.printTicketStore_.color,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.cssBackground,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
        this.printTicketStore_.customMargins,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.fitToPage,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.headerFooter,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.landscape,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.marginsType,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.pageRange,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.selectionOnly,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));

      if (this.checkPluginCompatibility_()) {
        this.previewGenerator_ = new print_preview.PreviewGenerator(
            this.destinationStore_,
            this.printTicketStore_,
            this.nativeLayer_,
            this.documentInfo_);
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.PREVIEW_START,
            this.onPreviewStart_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.PAGE_READY,
            this.onPagePreviewReady_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.FAIL,
            this.onPreviewGenerationFail_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.DOCUMENT_READY,
            this.onDocumentReady_.bind(this));
      } else {
        this.showCustomMessage(loadTimeData.getString('noPlugin'));
      }
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      if (this.previewGenerator_) {
        this.previewGenerator_.removeEventListeners();
      }
      this.overlayEl_ = null;
      this.openSystemDialogButton_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.marginControlContainer_.decorate(this.getElement());
      this.overlayEl_ = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OVERLAY)[0];
      this.openSystemDialogButton_ = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OPEN_SYSTEM_DIALOG_BUTTON)[0];
    },

    /**
     * Checks to see if a suitable plugin for rendering the preview exists. If
     * one does not exist, then an error message will be displayed.
     * @return {boolean} true if Chromium has a plugin for rendering the
     *     the preview.
     * @private
     */
    checkPluginCompatibility_: function() {
      // TODO(raymes): It's harder to test compatibility of the out of process
      // plugin because it's asynchronous. We could do a better job at some
      // point.
      var oopCompatObj = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OUT_OF_PROCESS_COMPATIBILITY_OBJECT)[0];
      var isOOPCompatible = oopCompatObj.postMessage;
      oopCompatObj.parentElement.removeChild(oopCompatObj);

      return isOOPCompatible;
    },

    /**
     * Shows a given message on the overlay.
     * @param {!print_preview.PreviewArea.MessageId_} messageId ID of the
     *     message to show.
     * @param {string=} opt_message Optional message to show that can be used
     *     by some message IDs.
     * @private
     */
    showMessage_: function(messageId, opt_message) {
      // Hide all messages.
      var messageEls = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.MESSAGE);
      for (var i = 0, messageEl; messageEl = messageEls[i]; i++) {
        setIsVisible(messageEl, false);
      }
      // Disable jumping animation to conserve cycles.
      var jumpingDotsEl = this.getElement().querySelector(
          '.preview-area-loading-message-jumping-dots');
      jumpingDotsEl.classList.remove('jumping-dots');

      // Show specific message.
      if (messageId == PreviewArea.MessageId_.CUSTOM) {
        var customMessageTextEl = this.getElement().getElementsByClassName(
            PreviewArea.Classes_.CUSTOM_MESSAGE_TEXT)[0];
        customMessageTextEl.textContent = opt_message;
      } else if (messageId == PreviewArea.MessageId_.LOADING) {
        jumpingDotsEl.classList.add('jumping-dots');
      }
      var messageEl = this.getElement().getElementsByClassName(
            PreviewArea.MessageIdClassMap_[messageId])[0];
      setIsVisible(messageEl, true);

      this.setOverlayVisible_(true);
    },

    /**
     * Set the visibility of the message overlay.
     * @param {boolean} visible Whether to make the overlay visible or not
     * @private
     */
    setOverlayVisible_: function(visible) {
      this.overlayEl_.classList.toggle(
          PreviewArea.Classes_.INVISIBLE,
          !visible);
      this.overlayEl_.setAttribute('aria-hidden', !visible);

      // Hide/show all controls that will overlap when the overlay is visible.
      var marginControls = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.MARGIN_CONTROL);
      for (var i = 0; i < marginControls.length; ++i) {
        marginControls[i].setAttribute('aria-hidden', visible);
      }
      var previewAreaControls = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.PREVIEW_AREA);
      for (var i = 0; i < previewAreaControls.length; ++i) {
        previewAreaControls[i].setAttribute('aria-hidden', visible);
      }

      if (!visible) {
        // Disable jumping animation to conserve cycles.
        var jumpingDotsEl = this.getElement().querySelector(
            '.preview-area-loading-message-jumping-dots');
        jumpingDotsEl.classList.remove('jumping-dots');
      }
    },

    /**
     * Creates a preview plugin and adds it to the DOM.
     * @param {string} srcUrl Initial URL of the plugin.
     * @private
     */
    createPlugin_: function(srcUrl) {
      if (this.plugin_) {
        console.warn('Pdf preview plugin already created');
        return;
      }

      this.plugin_ = /** @type {print_preview.PDFPlugin} */(
          PDFCreateOutOfProcessPlugin(srcUrl));
      this.plugin_.setKeyEventCallback(this.keyEventCallback_);

      this.plugin_.setAttribute('class', 'preview-area-plugin');
      this.plugin_.setAttribute('aria-live', 'polite');
      this.plugin_.setAttribute('aria-atomic', 'true');
      // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
      // chrome/renderer/printing/print_web_view_helper.cc actually references
      // it.
      this.plugin_.setAttribute('id', 'pdf-viewer');
      this.getChildElement('.preview-area-plugin-wrapper').
          appendChild(/** @type {Node} */(this.plugin_));


      var pageNumbers =
          this.printTicketStore_.pageRange.getPageNumberSet().asArray();
      var grayscale = !this.printTicketStore_.color.getValue();
      this.plugin_.setLoadCallback(this.onPluginLoad_.bind(this));
      this.plugin_.setViewportChangedCallback(
          this.onPreviewVisualStateChange_.bind(this));
      this.plugin_.resetPrintPreviewMode(srcUrl, grayscale, pageNumbers,
                                         this.documentInfo_.isModifiable);
    },

    /**
     * Dispatches a PREVIEW_GENERATION_DONE event if all conditions are met.
     * @private
     */
    dispatchPreviewGenerationDoneIfReady_: function() {
      if (this.isDocumentReady_ && this.isPluginReloaded_) {
        cr.dispatchSimpleEvent(
            this, PreviewArea.EventType.PREVIEW_GENERATION_DONE);
        this.marginControlContainer_.showMarginControlsIfNeeded();
      }
    },

    /**
     * Called when the open-system-dialog button is clicked. Disables the
     * button, shows the throbber, and dispatches the OPEN_SYSTEM_DIALOG_CLICK
     * event.
     * @private
     */
    onOpenSystemDialogButtonClick_: function() {
      this.openSystemDialogButton_.disabled = true;
      var openSystemDialogThrobber = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OPEN_SYSTEM_DIALOG_BUTTON_THROBBER)[0];
      setIsVisible(openSystemDialogThrobber, true);
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.OPEN_SYSTEM_DIALOG_CLICK);
    },

    /**
     * Called when the print ticket changes. Updates the preview.
     * @private
     */
    onTicketChange_: function() {
      if (this.previewGenerator_ && this.previewGenerator_.requestPreview()) {
        cr.dispatchSimpleEvent(
            this, PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS);
        if (this.loadingTimeout_ == null) {
          this.loadingTimeout_ = setTimeout(
              this.showMessage_.bind(this, PreviewArea.MessageId_.LOADING),
              PreviewArea.LOADING_TIMEOUT_);
        }
      } else {
        this.marginControlContainer_.showMarginControlsIfNeeded();
      }
    },

    /**
     * Called when the preview generator begins loading the preview.
     * @param {Event} event Contains the URL to initialize the plugin to.
     * @private
     */
    onPreviewStart_: function(event) {
      this.isDocumentReady_ = false;
      this.isPluginReloaded_ = false;
      if (!this.plugin_) {
        this.createPlugin_(event.previewUrl);
      } else {
        var grayscale = !this.printTicketStore_.color.getValue();
        var pageNumbers =
            this.printTicketStore_.pageRange.getPageNumberSet().asArray();
        var url = event.previewUrl;
        this.plugin_.resetPrintPreviewMode(url, grayscale, pageNumbers,
                                           this.documentInfo_.isModifiable);
      }
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS);
    },

    /**
     * Called when a page preview has been generated. Updates the plugin with
     * the new page.
     * @param {Event} event Contains information about the page preview.
     * @private
     */
    onPagePreviewReady_: function(event) {
      this.plugin_.loadPreviewPage(event.previewUrl, event.previewIndex);
    },

    /**
     * Called when the preview generation is complete and the document is ready
     * to print.
     * @private
     */
    onDocumentReady_: function(event) {
      this.isDocumentReady_ = true;
      this.dispatchPreviewGenerationDoneIfReady_();
    },

    /**
     * Called when the generation of a preview fails. Shows an error message.
     * @private
     */
    onPreviewGenerationFail_: function() {
      if (this.loadingTimeout_) {
        clearTimeout(this.loadingTimeout_);
        this.loadingTimeout_ = null;
      }
      this.showMessage_(PreviewArea.MessageId_.PREVIEW_FAILED);
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.PREVIEW_GENERATION_FAIL);
    },

    /**
     * Called when the plugin loads. This is a consequence of calling
     * plugin.reload(). Certain plugin state can only be set after the plugin
     * has loaded.
     * @private
     */
    onPluginLoad_: function() {
      if (this.loadingTimeout_) {
        clearTimeout(this.loadingTimeout_);
        this.loadingTimeout_ = null;
      }

      this.setOverlayVisible_(false);
      this.isPluginReloaded_ = true;
      this.dispatchPreviewGenerationDoneIfReady_();
    },

    /**
     * Called when the preview plugin's visual state has changed. This is a
     * consequence of scrolling or zooming the plugin. Updates the custom
     * margins component if shown.
     * @private
     */
    onPreviewVisualStateChange_: function(pageX,
                                          pageY,
                                          pageWidth,
                                          viewportWidth,
                                          viewportHeight) {
      this.marginControlContainer_.updateTranslationTransform(
          new print_preview.Coordinate2d(pageX, pageY));
      this.marginControlContainer_.updateScaleTransform(
          pageWidth / this.documentInfo_.pageSize.width);
      this.marginControlContainer_.updateClippingMask(
          new print_preview.Size(viewportWidth, viewportHeight));
    },

    /**
     * Called when dragging margins starts or stops.
     * @param {boolean} isDragging True if the margin is currently being dragged
     *     and false otherwise.
     */
    onMarginDragChanged_: function(isDragging) {
      if (!this.plugin_)
        return;

      // When hovering over the plugin (which may be in a separate iframe)
      // pointer events will be sent to the frame. When dragging the margins,
      // we don't want this to happen as it can cause the margin to stop
      // being draggable.
      this.plugin_.style.pointerEvents = isDragging ? 'none' : 'auto';
    }
  };

  // Export
  return {
    PreviewArea: PreviewArea
  };
});
