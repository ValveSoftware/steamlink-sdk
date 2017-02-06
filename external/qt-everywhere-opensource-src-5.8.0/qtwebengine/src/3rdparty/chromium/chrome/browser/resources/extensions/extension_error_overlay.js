// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @typedef {chrome.developerPrivate.RuntimeError} */
var RuntimeError;
/** @typedef {chrome.developerPrivate.ManifestError} */
var ManifestError;

cr.define('extensions', function() {
  'use strict';

  /**
   * Clear all the content of a given element.
   * @param {HTMLElement} element The element to be cleared.
   */
  function clearElement(element) {
    while (element.firstChild)
      element.removeChild(element.firstChild);
  }

  /**
   * Get the url relative to the main extension url. If the url is
   * unassociated with the extension, this will be the full url.
   * @param {string} url The url to make relative.
   * @param {string} extensionUrl The url for the extension resources, in the
   *     form "chrome-etxension://<extension_id>/".
   * @return {string} The url relative to the host.
   */
  function getRelativeUrl(url, extensionUrl) {
    return url.substring(0, extensionUrl.length) == extensionUrl ?
        url.substring(extensionUrl.length) : url;
  }

  /**
   * The RuntimeErrorContent manages all content specifically associated with
   * runtime errors; this includes stack frames and the context url.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function RuntimeErrorContent() {
    var contentArea = $('template-collection-extension-error-overlay').
        querySelector('.extension-error-overlay-runtime-content').
        cloneNode(true);
    contentArea.__proto__ = RuntimeErrorContent.prototype;
    contentArea.init();
    return contentArea;
  }

  /**
   * The name of the "active" class specific to extension errors (so as to
   * not conflict with other rules).
   * @type {string}
   * @const
   */
  RuntimeErrorContent.ACTIVE_CLASS_NAME = 'extension-error-active';

  /**
   * Determine whether or not we should display the url to the user. We don't
   * want to include any of our own code in stack traces.
   * @param {string} url The url in question.
   * @return {boolean} True if the url should be displayed, and false
   *     otherwise (i.e., if it is an internal script).
   */
  RuntimeErrorContent.shouldDisplayForUrl = function(url) {
    // All our internal scripts are in the 'extensions::' namespace.
    return !/^extensions::/.test(url);
  };

  RuntimeErrorContent.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The underlying error whose details are being displayed.
     * @type {?(RuntimeError|ManifestError)}
     * @private
     */
    error_: null,

    /**
     * The URL associated with this extension, i.e. chrome-extension://<id>/.
     * @type {?string}
     * @private
     */
    extensionUrl_: null,

    /**
     * The node of the stack trace which is currently active.
     * @type {?HTMLElement}
     * @private
     */
    currentFrameNode_: null,

    /**
     * Initialize the RuntimeErrorContent for the first time.
     */
    init: function() {
      /**
       * The stack trace element in the overlay.
       * @type {HTMLElement}
       * @private
       */
      this.stackTrace_ = /** @type {HTMLElement} */(
          this.querySelector('.extension-error-overlay-stack-trace-list'));
      assert(this.stackTrace_);

      /**
       * The context URL element in the overlay.
       * @type {HTMLElement}
       * @private
       */
      this.contextUrl_ = /** @type {HTMLElement} */(
          this.querySelector('.extension-error-overlay-context-url'));
      assert(this.contextUrl_);
    },

    /**
     * Sets the error for the content.
     * @param {(RuntimeError|ManifestError)} error The error whose content
     *     should be displayed.
     * @param {string} extensionUrl The URL associated with this extension.
     */
    setError: function(error, extensionUrl) {
      this.clearError();

      this.error_ = error;
      this.extensionUrl_ = extensionUrl;
      this.contextUrl_.textContent = error.contextUrl ?
          getRelativeUrl(error.contextUrl, this.extensionUrl_) :
          loadTimeData.getString('extensionErrorOverlayContextUnknown');
      this.initStackTrace_();
    },

    /**
     * Wipe content associated with a specific error.
     */
    clearError: function() {
      this.error_ = null;
      this.extensionUrl_ = null;
      this.currentFrameNode_ = null;
      clearElement(this.stackTrace_);
      this.stackTrace_.hidden = true;
    },

    /**
     * Makes |frame| active and deactivates the previously active frame (if
     * there was one).
     * @param {HTMLElement} frameNode The frame to activate.
     * @private
     */
    setActiveFrame_: function(frameNode) {
      if (this.currentFrameNode_) {
        this.currentFrameNode_.classList.remove(
            RuntimeErrorContent.ACTIVE_CLASS_NAME);
      }

      this.currentFrameNode_ = frameNode;
      this.currentFrameNode_.classList.add(
          RuntimeErrorContent.ACTIVE_CLASS_NAME);
    },

    /**
     * Initialize the stack trace element of the overlay.
     * @private
     */
    initStackTrace_: function() {
      for (var i = 0; i < this.error_.stackTrace.length; ++i) {
        var frame = this.error_.stackTrace[i];
        // Don't include any internal calls (e.g., schemaBindings) in the
        // stack trace.
        if (!RuntimeErrorContent.shouldDisplayForUrl(frame.url))
          continue;

        var frameNode = document.createElement('li');
        // Attach the index of the frame to which this node refers (since we
        // may skip some, this isn't a 1-to-1 match).
        frameNode.indexIntoTrace = i;

        // The description is a human-readable summation of the frame, in the
        // form "<relative_url>:<line_number> (function)", e.g.
        // "myfile.js:25 (myFunction)".
        var description = getRelativeUrl(frame.url,
            assert(this.extensionUrl_)) + ':' + frame.lineNumber;
        if (frame.functionName) {
          var functionName = frame.functionName == '(anonymous function)' ?
              loadTimeData.getString('extensionErrorOverlayAnonymousFunction') :
              frame.functionName;
          description += ' (' + functionName + ')';
        }
        frameNode.textContent = description;

        // When the user clicks on a frame in the stack trace, we should
        // highlight that overlay in the list, display the appropriate source
        // code with the line highlighted, and link the "Open DevTools" button
        // with that frame.
        frameNode.addEventListener('click', function(frame, frameNode, e) {
          this.setActiveFrame_(frameNode);

          // Request the file source with the section highlighted.
          extensions.ExtensionErrorOverlay.getInstance().requestFileSource(
              {extensionId: this.error_.extensionId,
               message: this.error_.message,
               pathSuffix: getRelativeUrl(frame.url,
                                          assert(this.extensionUrl_)),
               lineNumber: frame.lineNumber});
        }.bind(this, frame, frameNode));

        this.stackTrace_.appendChild(frameNode);
      }

      // Set the current stack frame to the first stack frame and show the
      // trace, if one exists. (We can't just check error.stackTrace, because
      // it's possible the trace was purely internal, and we don't show
      // internal frames.)
      if (this.stackTrace_.children.length > 0) {
        this.stackTrace_.hidden = false;
        this.setActiveFrame_(assertInstanceof(this.stackTrace_.firstChild,
            HTMLElement));
      }
    },

    /**
     * Open the developer tools for the active stack frame.
     */
    openDevtools: function() {
      var stackFrame =
          this.error_.stackTrace[this.currentFrameNode_.indexIntoTrace];

      chrome.developerPrivate.openDevTools(
          {renderProcessId: this.error_.renderProcessId || -1,
           renderViewId: this.error_.renderViewId || -1,
           url: stackFrame.url,
           lineNumber: stackFrame.lineNumber || 0,
           columnNumber: stackFrame.columnNumber || 0});
    }
  };

  /**
   * The ExtensionErrorOverlay will show the contents of a file which pertains
   * to the ExtensionError; this is either the manifest file (for manifest
   * errors) or a source file (for runtime errors). If possible, the portion
   * of the file which caused the error will be highlighted.
   * @constructor
   */
  function ExtensionErrorOverlay() {
    /**
     * The content section for runtime errors; this is re-used for all
     * runtime errors and attached/detached from the overlay as needed.
     * @type {RuntimeErrorContent}
     * @private
     */
    this.runtimeErrorContent_ = new RuntimeErrorContent();
  }

  /**
   * The manifest filename.
   * @type {string}
   * @const
   * @private
   */
  ExtensionErrorOverlay.MANIFEST_FILENAME_ = 'manifest.json';

  /**
   * Determine whether or not chrome can load the source for a given file; this
   * can only be done if the file belongs to the extension.
   * @param {string} file The file to load.
   * @param {string} extensionUrl The url for the extension, in the form
   *     chrome-extension://<extension-id>/.
   * @return {boolean} True if the file can be loaded, false otherwise.
   * @private
   */
  ExtensionErrorOverlay.canLoadFileSource = function(file, extensionUrl) {
    return file.substr(0, extensionUrl.length) == extensionUrl ||
           file.toLowerCase() == ExtensionErrorOverlay.MANIFEST_FILENAME_;
  };

  cr.addSingletonGetter(ExtensionErrorOverlay);

  ExtensionErrorOverlay.prototype = {
    /**
     * The underlying error whose details are being displayed.
     * @type {?(RuntimeError|ManifestError)}
     * @private
     */
    selectedError_: null,

    /**
     * Initialize the page.
     * @param {function(HTMLDivElement)} showOverlay The function to show or
     *     hide the ExtensionErrorOverlay; this should take a single parameter
     *     which is either the overlay Div if the overlay should be displayed,
     *     or null if the overlay should be hidden.
     */
    initializePage: function(showOverlay) {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('extension-error-overlay-dismiss').addEventListener('click',
          function() {
        cr.dispatchSimpleEvent(overlay, 'cancelOverlay');
      });

      /**
       * The element of the full overlay.
       * @type {HTMLDivElement}
       * @private
       */
      this.overlayDiv_ = /** @type {HTMLDivElement} */(
          $('extension-error-overlay'));

      /**
       * The portion of the overlay which shows the code relating to the error
       * and the corresponding line numbers.
       * @type {extensions.ExtensionCode}
       * @private
       */
      this.codeDiv_ =
          new extensions.ExtensionCode($('extension-error-overlay-code'));

      /**
       * The function to show or hide the ExtensionErrorOverlay.
       * @param {boolean} isVisible Whether the overlay should be visible.
       */
      this.setVisible = function(isVisible) {
        showOverlay(isVisible ? this.overlayDiv_ : null);
        if (isVisible)
          this.codeDiv_.scrollToError();
      };

      /**
       * The button to open the developer tools (only available for runtime
       * errors).
       * @type {HTMLButtonElement}
       * @private
       */
      this.openDevtoolsButton_ = /** @type {HTMLButtonElement} */(
          $('extension-error-overlay-devtools-button'));
      this.openDevtoolsButton_.addEventListener('click', function() {
          this.runtimeErrorContent_.openDevtools();
      }.bind(this));
    },

    /**
     * Handles a click on the dismiss ("OK" or close) buttons.
     * @param {Event} e The click event.
     * @private
     */
    handleDismiss_: function(e) {
      this.setVisible(false);

      // There's a chance that the overlay receives multiple dismiss events; in
      // this case, handle it gracefully and return (since all necessary work
      // will already have been done).
      if (!this.selectedError_)
        return;

      // Remove all previous content.
      this.codeDiv_.clear();

      this.overlayDiv_.querySelector('.extension-error-list').onRemoved();

      this.clearRuntimeContent_();

      this.selectedError_ = null;
    },

    /**
     * Clears the current content.
     * @private
     */
    clearRuntimeContent_: function() {
      if (this.runtimeErrorContent_.parentNode) {
        this.runtimeErrorContent_.parentNode.removeChild(
            this.runtimeErrorContent_);
        this.runtimeErrorContent_.clearError();
      }
      this.openDevtoolsButton_.hidden = true;
    },

    /**
     * Sets the active error for the overlay.
     * @param {?(ManifestError|RuntimeError)} error The error to make active.
     * @private
     */
    setActiveError_: function(error) {
      this.selectedError_ = error;

      // If there is no error (this can happen if, e.g., the user deleted all
      // the errors), then clear the content.
      if (!error) {
        this.codeDiv_.populate(
            null, loadTimeData.getString('extensionErrorNoErrorsCodeMessage'));
        this.clearRuntimeContent_();
        return;
      }

      var extensionUrl = 'chrome-extension://' + error.extensionId + '/';
      // Set or hide runtime content.
      if (error.type == chrome.developerPrivate.ErrorType.RUNTIME) {
        this.runtimeErrorContent_.setError(error, extensionUrl);
        this.overlayDiv_.querySelector('.content-area').insertBefore(
            this.runtimeErrorContent_,
            this.codeDiv_.nextSibling);
        this.openDevtoolsButton_.hidden = false;
        this.openDevtoolsButton_.disabled = !error.canInspect;
      } else {
        this.clearRuntimeContent_();
      }

      // Read the file source to populate the code section, or set it to null if
      // the file is unreadable.
      if (ExtensionErrorOverlay.canLoadFileSource(error.source, extensionUrl)) {
        // Use pathname instead of relativeUrl.
        var requestFileSourceArgs = {extensionId: error.extensionId,
                                     message: error.message};
        switch (error.type) {
          case chrome.developerPrivate.ErrorType.MANIFEST:
            requestFileSourceArgs.pathSuffix = error.source;
            requestFileSourceArgs.manifestKey = error.manifestKey;
            requestFileSourceArgs.manifestSpecific = error.manifestSpecific;
            break;
          case chrome.developerPrivate.ErrorType.RUNTIME:
            // slice(1) because pathname starts with a /.
            var pathname = new URL(error.source).pathname.slice(1);
            requestFileSourceArgs.pathSuffix = pathname;
            requestFileSourceArgs.lineNumber =
                error.stackTrace && error.stackTrace[0] ?
                    error.stackTrace[0].lineNumber : 0;
            break;
          default:
            assertNotReached();
        }
        this.requestFileSource(requestFileSourceArgs);
      } else {
        this.onFileSourceResponse_(null);
      }
    },

    /**
     * Associate an error with the overlay. This will set the error for the
     * overlay, and, if possible, will populate the code section of the overlay
     * with the relevant file, load the stack trace, and generate links for
     * opening devtools (the latter two only happen for runtime errors).
     * @param {Array<(RuntimeError|ManifestError)>} errors The error to show in
     *     the overlay.
     * @param {string} extensionId The id of the extension.
     * @param {string} extensionName The name of the extension.
     */
    setErrorsAndShowOverlay: function(errors, extensionId, extensionName) {
      document.querySelector(
          '#extension-error-overlay .extension-error-overlay-title').
              textContent = extensionName;
      var errorsDiv = this.overlayDiv_.querySelector('.extension-error-list');
      var extensionErrors =
          new extensions.ExtensionErrorList(errors, extensionId);
      errorsDiv.parentNode.replaceChild(extensionErrors, errorsDiv);
      extensionErrors.addEventListener('activeExtensionErrorChanged',
                                       function(e) {
        this.setActiveError_(e.detail);
      }.bind(this));

      if (errors.length > 0)
        this.setActiveError_(errors[0]);
      this.setVisible(true);
    },

    /**
     * Requests a file's source.
     * @param {chrome.developerPrivate.RequestFileSourceProperties} args The
     *     arguments for the call.
     */
    requestFileSource: function(args) {
      chrome.developerPrivate.requestFileSource(
          args, this.onFileSourceResponse_.bind(this));
    },

    /**
     * Set the code to be displayed in the code portion of the overlay.
     * @see ExtensionErrorOverlay.requestFileSourceResponse().
     * @param {?chrome.developerPrivate.RequestFileSourceResponse} response The
     *     response from the request file source call, which will be shown as
     *     code. If |response| is null, then a "Could not display code" message
     *     will be displayed instead.
     */
    onFileSourceResponse_: function(response) {
      this.codeDiv_.populate(
          response,  // ExtensionCode can handle a null response.
          loadTimeData.getString('extensionErrorOverlayNoCodeToDisplay'));
      this.setVisible(true);
    },
  };

  // Export
  return {
    ExtensionErrorOverlay: ExtensionErrorOverlay
  };
});
