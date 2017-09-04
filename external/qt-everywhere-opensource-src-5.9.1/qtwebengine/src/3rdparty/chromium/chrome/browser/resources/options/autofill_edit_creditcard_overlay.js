// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * AutofillEditCreditCardOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutofillEditCreditCardOverlay() {
    Page.call(this, 'autofillEditCreditCard',
              loadTimeData.getString('autofillEditCreditCardTitle'),
              'autofill-edit-credit-card-overlay');
  }

  cr.addSingletonGetter(AutofillEditCreditCardOverlay);

  AutofillEditCreditCardOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var self = this;
      $('autofill-edit-credit-card-cancel-button').onclick = function(event) {
        self.dismissOverlay_();
      };
      $('autofill-edit-credit-card-apply-button').onclick = function(event) {
        self.saveCreditCard_();
        self.dismissOverlay_();
      };

      self.guid_ = '';
      self.clearInputFields_();
      self.connectInputEvents_();
      self.setDefaultSelectOptions_();
    },

    /**
    * Specifically catch the situations in which the overlay is cancelled
    * externally (e.g. by pressing <Esc>), so that the input fields and
    * GUID can be properly cleared.
    * @override
    */
    handleCancel: function() {
      this.dismissOverlay_();
    },

    /**
     * Clears any uncommitted input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      this.guid_ = '';
      PageManager.closeOverlay();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the Autofill handler.
     * @private
     */
    saveCreditCard_: function() {
      var creditCard = new Array(5);
      creditCard[0] = this.guid_;
      creditCard[1] = $('name-on-card').value;
      creditCard[2] = $('credit-card-number').value;
      creditCard[3] = $('expiration-month').value;
      creditCard[4] = $('expiration-year').value;
      chrome.send('setCreditCard', creditCard);

      // If the GUID is empty, this form is being used to add a new card,
      // rather than edit an existing one.
      if (!this.guid_.length) {
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_AutofillCreditCardAdded']);
      }
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      var ccNumber = $('credit-card-number');
      $('name-on-card').oninput = ccNumber.oninput =
          $('expiration-month').onchange = $('expiration-year').onchange =
              this.inputFieldChanged_.bind(this);
    },

    /**
     * Checks the values of each of the input fields and disables the 'Ok'
     * button if all of the fields are empty.
     * @param {Event} opt_event Optional data for the 'input' event.
     * @private
     */
    inputFieldChanged_: function(opt_event) {
      var disabled = !$('name-on-card').value.trim() &&
              !$('credit-card-number').value.trim();
      $('autofill-edit-credit-card-apply-button').disabled = disabled;
    },

    /**
     * Sets the default values of the options in the 'Expiration date' select
     * controls.
     * @private
     */
    setDefaultSelectOptions_: function() {
      // Set the 'Expiration month' default options.
      var expirationMonth = $('expiration-month');
      expirationMonth.options.length = 0;
      for (var i = 1; i <= 12; ++i) {
        var text = (i < 10 ? '0' : '') + i;

        var option = document.createElement('option');
        option.text = option.value = text;
        expirationMonth.add(option, null);
      }

      // Set the 'Expiration year' default options.
      var expirationYear = $('expiration-year');
      expirationYear.options.length = 0;

      var date = new Date();
      var year = parseInt(date.getFullYear(), 10);
      for (var i = 0; i < 10; ++i) {
        var text = year + i;
        var option = document.createElement('option');
        option.text = String(text);
        option.value = text;
        expirationYear.add(option, null);
      }
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      $('name-on-card').value = '';
      $('credit-card-number').value = '';
      $('expiration-month').selectedIndex = 0;
      $('expiration-year').selectedIndex = 0;

      // Reset the enabled status of the 'Ok' button.
      this.inputFieldChanged_();
    },

    /**
     * Sets the value of each input field according to |creditCard|
     * @param {CreditCardData} creditCard
     * @private
     */
    setInputFields_: function(creditCard) {
      $('name-on-card').value = creditCard.nameOnCard;
      $('credit-card-number').value = creditCard.creditCardNumber;

      // The options for the year select control may be out-dated at this point,
      // e.g. the user opened the options page before midnight on New Year's Eve
      // and then loaded a credit card profile to edit in the new year, so
      // reload the select options just to be safe.
      this.setDefaultSelectOptions_();

      var idx = parseInt(creditCard.expirationMonth, 10);
      $('expiration-month').selectedIndex = idx - 1;

      var expYear = creditCard.expirationYear;
      var date = new Date();
      var year = parseInt(date.getFullYear(), 10);
      for (var i = 0; i < 10; ++i) {
        var text = year + i;
        if (expYear == String(text))
          $('expiration-year').selectedIndex = i;
      }
    },

    /**
     * Called to prepare the overlay when a new card is being added.
     * @private
     */
    prepForNewCard_: function() {
      // Focus the first element.
      this.pageDiv.querySelector('input').focus();
    },

    /**
     * Loads the credit card data from |creditCard|, sets the input fields based
     * on this data and stores the GUID of the credit card.
     * @param {CreditCardData} creditCard
     * @private
     */
    loadCreditCard_: function(creditCard) {
      this.setInputFields_(creditCard);
      this.inputFieldChanged_();
      this.guid_ = creditCard.guid;
    },
  };

  AutofillEditCreditCardOverlay.prepForNewCard = function() {
    AutofillEditCreditCardOverlay.getInstance().prepForNewCard_();
  };

  AutofillEditCreditCardOverlay.loadCreditCard = function(creditCard) {
    AutofillEditCreditCardOverlay.getInstance().loadCreditCard_(creditCard);
  };

  AutofillEditCreditCardOverlay.setTitle = function(title) {
    $('autofill-credit-card-title').textContent = title;
  };

  // Export
  return {
    AutofillEditCreditCardOverlay: AutofillEditCreditCardOverlay
  };
});
