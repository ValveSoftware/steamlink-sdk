// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the 'DisplayOverscan' page.
   * @constructor
   */
  function DisplayOverscan() {
    OptionsPage.call(this, 'displayOverscan',
                     loadTimeData.getString('displayOverscanPageTabTitle'),
                     'display-overscan-page');
  }

  cr.addSingletonGetter(DisplayOverscan);

  DisplayOverscan.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The ID of the target display.
     * @private
     */
    id_: null,

    /**
     * The keyboard event handler function.
     * @private
     */
    keyHandler_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.keyHandler_ = this.handleKeyevent_.bind(this);
      $('display-overscan-operation-reset').onclick = function() {
        chrome.send('reset');
      };
      $('display-overscan-operation-ok').onclick = function() {
        chrome.send('commit');
        OptionsPage.closeOverlay();
      };
      $('display-overscan-operation-cancel').onclick = function() {
        OptionsPage.cancelOverlay();
      };
    },

    /** @override */
    handleCancel: function() {
      // signals the cancel event.
      chrome.send('cancel');
      OptionsPage.closeOverlay();
    },

    /** @override */
    didShowPage: function() {
      if (this.id_ == null) {
        OptionsPage.cancelOverlay();
        return;
      }

      window.addEventListener('keydown', this.keyHandler_);
      // Sets up the size of the overscan dialog based on DisplayOptions dialog.
      var displayOptionsPage = $('display-options-page');
      var displayOverscanPage = $('display-overscan-page');
      displayOverscanPage.style.width =
          displayOptionsPage.offsetWidth - 20 + 'px';
      displayOverscanPage.style.minWidth = displayOverscanPage.style.width;
      displayOverscanPage.style.height =
          displayOptionsPage.offsetHeight - 50 + 'px';

      // Moves the table to describe operation at the middle of the contents
      // vertically.
      var operationsTable = $('display-overscan-operations-table');
      var buttonsContainer = $('display-overscan-button-strip');
      operationsTable.style.top = buttonsContainer.offsetTop / 2 -
          operationsTable.offsetHeight / 2 + 'px';

      $('display-overscan-operation-cancel').focus();
      chrome.send('start', [this.id_]);
    },

    /** @override */
    didClosePage: function() {
      window.removeEventListener('keydown', this.keyHandler_);
    },

    /**
     * Called when the overscan calibration is canceled at the system level,
     * such like the display is disconnected.
     * @private
     */
    onOverscanCanceled_: function() {
      if (OptionsPage.getTopmostVisiblePage() == this)
        OptionsPage.cancelOverlay();
    },

    /**
     * Sets the target display id. This method has to be called before
     * navigating to this page.
     * @param {string} id The target display id.
     */
    setDisplayId: function(id) {
      this.id_ = id;
    },

    /**
     * Key event handler to make the effect of display rectangle.
     * @param {Event} event The keyboard event.
     * @private
     */
    handleKeyevent_: function(event) {
      switch (event.keyCode) {
        case 37: // left arrow
          if (event.shiftKey)
            chrome.send('move', ['horizontal', -1]);
          else
            chrome.send('resize', ['horizontal', -1]);
          event.preventDefault();
          break;
        case 38: // up arrow
          if (event.shiftKey)
            chrome.send('move', ['vertical', -1]);
          else
            chrome.send('resize', ['vertical', -1]);
          event.preventDefault();
          break;
        case 39: // right arrow
          if (event.shiftKey)
            chrome.send('move', ['horizontal', 1]);
          else
            chrome.send('resize', ['horizontal', 1]);
          event.preventDefault();
          break;
        case 40: // bottom arrow
          if (event.shiftKey)
            chrome.send('move', ['vertical', 1]);
          else
            chrome.send('resize', ['vertical', 1]);
          event.preventDefault();
          break;
      }
    }
  };

  DisplayOverscan.onOverscanCanceled = function() {
    DisplayOverscan.getInstance().onOverscanCanceled_();
  };

  // Export
  return {
    DisplayOverscan: DisplayOverscan
  };
});
