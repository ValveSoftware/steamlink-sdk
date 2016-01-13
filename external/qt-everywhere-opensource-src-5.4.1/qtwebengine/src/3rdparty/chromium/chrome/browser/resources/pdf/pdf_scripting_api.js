// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Create a new PDFScriptingAPI. This provides a scripting interface to
 * the PDF viewer so that it can be customized by things like print preview.
 * @param {Window} window the window of the page containing the pdf viewer.
 * @param {string} extensionUrl the url of the PDF extension.
 */
function PDFScriptingAPI(window, extensionUrl) {
  this.extensionUrl_ = extensionUrl;
  this.messageQueue_ = [];
  window.addEventListener('message', function(event) {
    if (event.origin != this.extensionUrl_) {
      console.error('Received message that was not from the extension: ' +
                    event);
      return;
    }
    switch (event.data.type) {
      case 'readyToReceive':
        this.setDestinationWindow(event.source);
        break;
      case 'viewport':
        if (this.viewportChangedCallback_)
          this.viewportChangedCallback_(event.data.pageX,
                                        event.data.pageY,
                                        event.data.pageWidth,
                                        event.data.viewportWidth,
                                        event.data.viewportHeight);
        break;
      case 'documentLoaded':
        if (this.loadCallback_)
          this.loadCallback_();
        break;
      case 'getAccessibilityJSONReply':
        if (this.accessibilityCallback_) {
          this.accessibilityCallback_(event.data.json);
          this.accessibilityCallback_ = null;
        }
        break;
    }
  }.bind(this), false);
}

PDFScriptingAPI.prototype = {
  /**
   * @private
   * Send a message to the extension. If we try to send messages prior to the
   * extension being ready to receive messages (i.e. before it has finished
   * loading) we queue up the messages and flush them later.
   * @param {Object} the message to send.
   */
  sendMessage_: function(message) {
    if (!this.pdfWindow_) {
      this.messageQueue_.push(message);
      return;
    }

    this.pdfWindow_.postMessage(message, this.extensionUrl_);
  },

  /**
   * Sets the destination window containing the PDF viewer. This will be called
   * when a 'readyToReceive' message is received from the PDF viewer or it can
   * be called during tests. It then flushes any pending messages to the window.
   * @param {Window} pdfWindow the window containing the PDF viewer.
   */
  setDestinationWindow: function(pdfWindow) {
    this.pdfWindow_ = pdfWindow;
    while (this.messageQueue_.length != 0) {
      this.pdfWindow_.postMessage(this.messageQueue_.shift(),
                                  this.extensionUrl_);
    }
  },

  /**
   * Sets the callback which will be run when the PDF viewport changes.
   * @param {Function} callback the callback to be called.
   */
  setViewportChangedCallback: function(callback) {
    this.viewportChangedCallback_ = callback;
  },

  /**
   * Sets the callback which will be run when the PDF document has finished
   * loading.
   * @param {Function} callback the callback to be called.
   */
  setLoadCallback: function(callback) {
    this.loadCallback_ = callback;
  },

  /**
   * Resets the PDF viewer into print preview mode.
   * @param {string} url the url of the PDF to load.
   * @param {boolean} grayscale whether or not to display the PDF in grayscale.
   * @param {Array.<number>} pageNumbers an array of the page numbers.
   * @param {boolean} modifiable whether or not the document is modifiable.
   */
  resetPrintPreviewMode: function(url, grayscale, pageNumbers, modifiable) {
    this.sendMessage_({
      type: 'resetPrintPreviewMode',
      url: url,
      grayscale: grayscale,
      pageNumbers: pageNumbers,
      modifiable: modifiable
    });
  },

  /**
   * Load a page into the document while in print preview mode.
   * @param {string} url the url of the pdf page to load.
   * @param {number} index the index of the page to load.
   */
  loadPreviewPage: function(url, index) {
    this.sendMessage_({
      type: 'loadPreviewPage',
      url: url,
      index: index
    });
  },

  /**
   * Get accessibility JSON for the document.
   * @param {Function} callback a callback to be called with the accessibility
   *     json that has been retrieved.
   * @param {number} [page] the 0-indexed page number to get accessibility data
   *     for. If this is not provided, data about the entire document is
   *     returned.
   * @return {boolean} true if the function is successful, false if there is an
   *     outstanding request for accessibility data that has not been answered.
   */
  getAccessibilityJSON: function(callback, page) {
    if (this.accessibilityCallback_)
      return false;
    this.accessibilityCallback_ = callback;
    var message = {
      type: 'getAccessibilityJSON',
    };
    if (page || page == 0)
      message.page = page;
    this.sendMessage_(message);
    return true;
  },

  /**
   * Send a key event to the extension.
   * @param {number} keyCode the key code to send to the extension.
   */
  sendKeyEvent: function(keyCode) {
    this.sendMessage_({
      type: 'sendKeyEvent',
      keyCode: keyCode
    });
  },
};

/**
 * Creates a PDF viewer with a scripting interface. This is basically 1) an
 * iframe which is navigated to the PDF viewer extension and 2) a scripting
 * interface which provides access to various features of the viewer for use
 * by print preview and accessbility.
 * @param {string} src the source URL of the PDF to load initially.
 * @return {HTMLIFrameElement} the iframe element containing the PDF viewer.
 */
function PDFCreateOutOfProcessPlugin(src) {
  var EXTENSION_URL = 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai';
  var iframe = window.document.createElement('iframe');
  iframe.setAttribute('src', EXTENSION_URL + '/index.html?' + src);
  var client = new PDFScriptingAPI(window, EXTENSION_URL);

  // Add the functions to the iframe so that they can be called directly.
  iframe.setViewportChangedCallback =
      client.setViewportChangedCallback.bind(client);
  iframe.setLoadCallback = client.setLoadCallback.bind(client);
  iframe.resetPrintPreviewMode = client.resetPrintPreviewMode.bind(client);
  iframe.loadPreviewPage = client.loadPreviewPage.bind(client);
  iframe.sendKeyEvent = client.sendKeyEvent.bind(client);
  return iframe;
}
