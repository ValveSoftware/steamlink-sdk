// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

<include src="../../../../ui/webui/resources/js/util.js">
<include src="pdf_scripting_api.js">
<include src="viewport.js">

/**
 * @return {number} Width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  var div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  var result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * The minimum number of pixels to offset the toolbar by from the bottom and
 * right side of the screen.
 */
PDFViewer.MIN_TOOLBAR_OFFSET = 15;

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 */
function PDFViewer() {
  this.loaded = false;

  // The sizer element is placed behind the plugin element to cause scrollbars
  // to be displayed in the window. It is sized according to the document size
  // of the pdf and zoom level.
  this.sizer_ = $('sizer');
  this.toolbar_ = $('toolbar');
  this.pageIndicator_ = $('page-indicator');
  this.progressBar_ = $('progress-bar');
  this.passwordScreen_ = $('password-screen');
  this.passwordScreen_.addEventListener('password-submitted',
                                        this.onPasswordSubmitted_.bind(this));
  this.errorScreen_ = $('error-screen');

  // Create the viewport.
  this.viewport_ = new Viewport(window,
                                this.sizer_,
                                this.viewportChangedCallback_.bind(this),
                                getScrollbarWidth());

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('object');
  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_web_view_helper.cc actually references it.
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';
  this.plugin_.addEventListener('message', this.handlePluginMessage_.bind(this),
                                false);

  // Handle scripting messages from outside the extension that wish to interact
  // with it. We also send a message indicating that extension has loaded and
  // is ready to receive messages.
  window.addEventListener('message', this.handleScriptingMessage_.bind(this),
                          false);
  this.sendScriptingMessage_({type: 'readyToReceive'});

  // If the viewer is started from a MIME type request, there will be a
  // background page and stream details object with the details of the request.
  // Otherwise, we take the query string of the URL to indicate the URL of the
  // PDF to load. This is used for print preview in particular.
  if (chrome.extension.getBackgroundPage &&
      chrome.extension.getBackgroundPage()) {
    this.streamDetails =
        chrome.extension.getBackgroundPage().popStreamDetails();
  }

  if (!this.streamDetails) {
    // The URL of this page will be of the form
    // "chrome-extension://<extension id>?<pdf url>". We pull out the <pdf url>
    // part here.
    var url = window.location.search.substring(1);
    this.streamDetails = {
      streamUrl: url,
      originalUrl: url,
      responseHeaders: ''
    };
  }

  this.plugin_.setAttribute('src', this.streamDetails.originalUrl);
  this.plugin_.setAttribute('stream-url', this.streamDetails.streamUrl);
  var headers = '';
  for (var header in this.streamDetails.responseHeaders) {
    headers += header + ': ' +
        this.streamDetails.responseHeaders[header] + '\n';
  }
  this.plugin_.setAttribute('headers', headers);

  if (window.top == window)
    this.plugin_.setAttribute('full-frame', '');
  document.body.appendChild(this.plugin_);

  // Setup the button event listeners.
  $('fit-to-width-button').addEventListener('click',
      this.viewport_.fitToWidth.bind(this.viewport_));
  $('fit-to-page-button').addEventListener('click',
      this.viewport_.fitToPage.bind(this.viewport_));
  $('zoom-in-button').addEventListener('click',
      this.viewport_.zoomIn.bind(this.viewport_));
  $('zoom-out-button').addEventListener('click',
      this.viewport_.zoomOut.bind(this.viewport_));
  $('save-button-link').href = this.streamDetails.originalUrl;
  $('print-button').addEventListener('click', this.print_.bind(this));

  // Setup the keyboard event listener.
  document.onkeydown = this.handleKeyEvent_.bind(this);
}

PDFViewer.prototype = {
  /**
   * @private
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {KeyboardEvent} e the event to handle.
   */
  handleKeyEvent_: function(e) {
    var position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    var fromScriptingAPI = e.type == 'scriptingKeypress';

    switch (e.keyCode) {
      case 33:  // Page up key.
        // Go to the previous page if we are fit-to-page.
        if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.y -= this.viewport.size.height;
          this.viewport.position = position;
        }
        return;
      case 34:  // Page down key.
        // Go to the next page if we are fit-to-page.
        if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.y += this.viewport.size.height;
          this.viewport.position = position;
        }
        return;
      case 37:  // Left arrow key.
        // Go to the previous page if there are no horizontal scrollbars.
        if (!this.viewport_.documentHasScrollbars().x) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.x -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 39:  // Right arrow key.
        // Go to the next page if there are no horizontal scrollbars.
        if (!this.viewport_.documentHasScrollbars().x) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.x += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 187:  // +/= key.
      case 107:  // Numpad + key.
        if (e.ctrlKey || e.metaKey) {
          this.viewport_.zoomIn();
          // Since we do the zooming of the page.
          e.preventDefault();
        }
        return;
      case 189:  // -/_ key.
      case 109:  // Numpad - key.
        if (e.ctrlKey || e.metaKey) {
          this.viewport_.zoomOut();
          // Since we do the zooming of the page.
          e.preventDefault();
        }
        return;
      case 83:  // s key.
        if (e.ctrlKey || e.metaKey) {
          // Simulate a click on the button so that the <a download ...>
          // attribute is used.
          $('save-button-link').click();
          // Since we do the saving of the page.
          e.preventDefault();
        }
        return;
      case 80:  // p key.
        if (e.ctrlKey || e.metaKey) {
          this.print_();
          // Since we do the printing of the page.
          e.preventDefault();
        }
        return;
    }
  },

  /**
   * @private
   * Notify the plugin to print.
   */
  print_: function() {
    this.plugin_.postMessage({
      type: 'print',
    });
  },

  /**
   * @private
   * Update the loading progress of the document in response to a progress
   * message being received from the plugin.
   * @param {number} progress the progress as a percentage.
   */
  updateProgress_: function(progress) {
    this.progressBar_.progress = progress;
    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.style.visibility = 'visible';
      this.sizer_.style.display = 'none';
      this.toolbar_.style.visibility = 'hidden';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.active = false;
      }
    } else if (progress == 100) {
      // Document load complete.
      this.loaded = true;
      var loadEvent = new Event('pdfload');
      window.dispatchEvent(loadEvent);
      this.sendScriptingMessage_({
        type: 'documentLoaded'
      });
      if (this.lastViewportPosition_)
        this.viewport_.position = this.lastViewportPosition_;
    }
  },

  /**
   * @private
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   * @param {Object} event a password-submitted event.
   */
  onPasswordSubmitted_: function(event) {
    this.plugin_.postMessage({
      type: 'getPasswordComplete',
      password: event.detail.password
    });
  },

  /**
   * @private
   * An event handler for handling message events received from the plugin.
   * @param {MessageObject} message a message event.
   */
  handlePluginMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'documentDimensions':
        this.documentDimensions_ = message.data;
        this.viewport_.setDocumentDimensions(this.documentDimensions_);
        this.toolbar_.style.visibility = 'visible';
        // If we received the document dimensions, the password was good so we
        // can dismiss the password screen.
        if (this.passwordScreen_.active)
          this.passwordScreen_.accept();

        this.pageIndicator_.initialFadeIn();
        this.toolbar_.initialFadeIn();
        break;
      case 'email':
        var href = 'mailto:' + message.data.to + '?cc=' + message.data.cc +
            '&bcc=' + message.data.bcc + '&subject=' + message.data.subject +
            '&body=' + message.data.body;
        var w = window.open(href, '_blank', 'width=1,height=1');
        if (w)
          w.close();
        break;
      case 'getAccessibilityJSONReply':
        this.sendScriptingMessage_(message.data);
        break;
      case 'getPassword':
        // If the password screen isn't up, put it up. Otherwise we're
        // responding to an incorrect password so deny it.
        if (!this.passwordScreen_.active)
          this.passwordScreen_.active = true;
        else
          this.passwordScreen_.deny();
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'loadProgress':
        this.updateProgress_(message.data.progress);
        break;
      case 'navigate':
        if (message.data.newTab)
          window.open(message.data.url);
        else
          window.location.href = message.data.url;
        break;
      case 'setScrollPosition':
        var position = this.viewport_.position;
        if (message.data.x != undefined)
          position.x = message.data.x;
        if (message.data.y != undefined)
          position.y = message.data.y;
        this.viewport_.position = position;
        break;
      case 'setTranslatedStrings':
        this.passwordScreen_.text = message.data.getPasswordString;
        this.progressBar_.text = message.data.loadingString;
        this.errorScreen_.text = message.data.loadFailedString;
        break;
      case 'cancelStreamUrl':
        chrome.streamsPrivate.abort(this.streamDetails.streamUrl);
        break;
    }
  },

  /**
   * @private
   * A callback that's called when the viewport changes.
   */
  viewportChangedCallback_: function() {
    if (!this.documentDimensions_)
      return;

    // Update the buttons selected.
    $('fit-to-page-button').classList.remove('polymer-selected');
    $('fit-to-width-button').classList.remove('polymer-selected');
    if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
      $('fit-to-page-button').classList.add('polymer-selected');
    } else if (this.viewport_.fittingType ==
               Viewport.FittingType.FIT_TO_WIDTH) {
      $('fit-to-width-button').classList.add('polymer-selected');
    }

    var hasScrollbars = this.viewport_.documentHasScrollbars();
    var scrollbarWidth = this.viewport_.scrollbarWidth;
    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    var toolbarRight = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    var toolbarBottom = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    if (hasScrollbars.vertical)
      toolbarRight -= scrollbarWidth;
    if (hasScrollbars.horizontal)
      toolbarBottom -= scrollbarWidth;
    this.toolbar_.style.right = toolbarRight + 'px';
    this.toolbar_.style.bottom = toolbarBottom + 'px';

    // Update the page indicator.
    var visiblePage = this.viewport_.getMostVisiblePage();
    this.pageIndicator_.index = visiblePage;
    if (this.documentDimensions_.pageDimensions.length > 1 &&
        hasScrollbars.vertical) {
      this.pageIndicator_.style.visibility = 'visible';
    } else {
      this.pageIndicator_.style.visibility = 'hidden';
    }

    var position = this.viewport_.position;
    var zoom = this.viewport_.zoom;
    // Notify the plugin of the viewport change.
    this.plugin_.postMessage({
      type: 'viewport',
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y
    });

    var visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    var size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height,
    });
  },

  /**
   * @private
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   * @param {MessageObject} message the message to handle.
   */
  handleScriptingMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'getAccessibilityJSON':
      case 'loadPreviewPage':
        this.plugin_.postMessage(message.data);
        break;
      case 'resetPrintPreviewMode':
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.viewport_.fitToPage();
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition_ = this.viewport_.position;

        // TODO(raymes): Disable these properly in the plugin.
        var printButton = $('print-button');
        if (printButton)
          printButton.parentNode.removeChild(printButton);
        var saveButton = $('save-button');
        if (saveButton)
          saveButton.parentNode.removeChild(saveButton);

        this.pageIndicator_.pageLabels = message.data.pageNumbers;

        this.plugin_.postMessage({
          type: 'resetPrintPreviewMode',
          url: message.data.url,
          grayscale: message.data.grayscale,
          // If the PDF isn't modifiable we send 0 as the page count so that no
          // blank placeholder pages get appended to the PDF.
          pageCount: (message.data.modifiable ?
                      message.data.pageNumbers.length : 0)
        });
        break;
      case 'sendKeyEvent':
        var e = document.createEvent('Event');
        e.initEvent('scriptingKeypress');
        e.keyCode = message.data.keyCode;
        this.handleKeyEvent_(e);
        break;
    }

  },

  /**
   * @private
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   * @param {Object} message the message to send.
   */
  sendScriptingMessage_: function(message) {
    window.parent.postMessage(message, '*');
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  }
};

var viewer = new PDFViewer();
