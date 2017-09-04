// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  // UI state of the turn off overlay.
  // @enum {string}
  var UIState = {
    UNKNOWN: 'unknown',
    OFFLINE: 'offline',
    IDLE: 'idle',
    PENDING: 'pending',
    SERVER_ERROR: 'server-error',
  };

  /**
   * EasyUnlockTurnOffOverlay class
   * Encapsulated handling of the Factory Reset confirmation overlay page.
   * @class
   */
  function EasyUnlockTurnOffOverlay() {
    Page.call(this, 'easyUnlockTurnOffOverlay',
              loadTimeData.getString('easyUnlockTurnOffTitle'),
              'easy-unlock-turn-off-overlay');
  }

  cr.addSingletonGetter(EasyUnlockTurnOffOverlay);

  EasyUnlockTurnOffOverlay.prototype = {
    // Inherit EasyUnlockTurnOffOverlay from Page.
    __proto__: Page.prototype,

    /** Current UI state */
    uiState_: UIState.UNKNOWN,
    get uiState() {
      return this.uiState_;
    },
    set uiState(newUiState) {
      if (newUiState == this.uiState_)
        return;

      this.uiState_ = newUiState;
      switch (this.uiState_) {
        case UIState.OFFLINE:
          this.setUpOfflineUI_();
          break;
        case UIState.IDLE:
          this.setUpTurnOffUI_(false);
          break;
        case UIState.PENDING:
          this.setUpTurnOffUI_(true);
          break;
        case UIState.SERVER_ERROR:
          this.setUpServerErrorUI_();
          break;
        default:
          console.error('Unknow Easy unlock turn off UI state: ' +
                        this.uiState_);
          this.setUpTurnOffUI_(false);
          break;
      }
    },

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('easy-unlock-turn-off-dismiss').onclick = function(event) {
        EasyUnlockTurnOffOverlay.dismiss();
      };
      $('easy-unlock-turn-off-confirm').onclick = function(event) {
        this.uiState = UIState.PENDING;
        chrome.send('easyUnlockRequestTurnOff');
      }.bind(this);
    },

    /** @override */
    didShowPage: function() {
      if (navigator.onLine) {
        this.uiState = UIState.IDLE;
        chrome.send('easyUnlockGetTurnOffFlowStatus');
      } else {
        this.uiState = UIState.OFFLINE;
      }
    },

    /** @override */
    didClosePage: function() {
      chrome.send('easyUnlockTurnOffOverlayDismissed');
    },

    /**
     * Returns the button strip element.
     * @return {HTMLDivElement} The container div of action buttons.
     */
    get buttonStrip() {
      return this.pageDiv.querySelector('.button-strip');
    },

    /**
     * Set visibility of action buttons in button strip.
     * @private
     */
    setActionButtonsVisible_: function(visible) {
      var buttons = this.buttonStrip.querySelectorAll('button');
      for (var i = 0; i < buttons.length; ++i) {
        buttons[i].hidden = !visible;
      }
    },

    /**
     * Set visibility of spinner.
     * @private
     */
    setSpinnerVisible_: function(visible) {
      $('easy-unlock-turn-off-spinner').hidden = !visible;
    },

    /**
     * Set up UI for showing offline message.
     * @private
     */
    setUpOfflineUI_: function() {
      $('easy-unlock-turn-off-title').textContent =
          loadTimeData.getString('easyUnlockTurnOffOfflineTitle');
      $('easy-unlock-turn-off-messagee').textContent =
          loadTimeData.getString('easyUnlockTurnOffOfflineMessage');

      this.setActionButtonsVisible_(false);
      this.setSpinnerVisible_(false);
    },

    /**
     * Set up UI for turning off Easy Unlock.
     * @param {boolean} pending Whether there is a pending turn-off call.
     * @private
     */
    setUpTurnOffUI_: function(pending) {
      $('easy-unlock-turn-off-title').textContent =
          loadTimeData.getString('easyUnlockTurnOffTitle');
      $('easy-unlock-turn-off-messagee').textContent =
          loadTimeData.getString('easyUnlockTurnOffDescription');
      $('easy-unlock-turn-off-confirm').textContent =
          loadTimeData.getString('easyUnlockTurnOffButton');

      this.setActionButtonsVisible_(true);
      this.setSpinnerVisible_(pending);
      $('easy-unlock-turn-off-confirm').disabled = pending;
      $('easy-unlock-turn-off-dismiss').hidden = false;
    },

    /**
     * Set up UI for showing server error.
     * @private
     */
    setUpServerErrorUI_: function() {
      $('easy-unlock-turn-off-title').textContent =
          loadTimeData.getString('easyUnlockTurnOffErrorTitle');
      $('easy-unlock-turn-off-messagee').textContent =
          loadTimeData.getString('easyUnlockTurnOffErrorMessage');
      $('easy-unlock-turn-off-confirm').textContent =
          loadTimeData.getString('easyUnlockTurnOffRetryButton');

      this.setActionButtonsVisible_(true);
      this.setSpinnerVisible_(false);
      $('easy-unlock-turn-off-confirm').disabled = false;
      $('easy-unlock-turn-off-dismiss').hidden = true;
    },
  };

  /**
   * Closes the Easy unlock turn off overlay.
   */
  EasyUnlockTurnOffOverlay.dismiss = function() {
    PageManager.closeOverlay();
  };

  /**
   * Update UI to reflect the turn off operation status.
   * @param {string} newState The UIState string representing the new state.
   */
  EasyUnlockTurnOffOverlay.updateUIState = function(newState) {
    EasyUnlockTurnOffOverlay.getInstance().uiState = newState;
  };

  // Export
  return {
    EasyUnlockTurnOffOverlay: EasyUnlockTurnOffOverlay
  };
});
