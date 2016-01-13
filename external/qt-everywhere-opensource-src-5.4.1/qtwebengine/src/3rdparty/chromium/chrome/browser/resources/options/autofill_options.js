// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // AutofillOptions class:

  /**
   * Encapsulated handling of Autofill options page.
   * @constructor
   */
  function AutofillOptions() {
    OptionsPage.call(this,
                     'autofill',
                     loadTimeData.getString('autofillOptionsPageTabTitle'),
                     'autofill-options');
  }

  cr.addSingletonGetter(AutofillOptions);

  AutofillOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The address list.
     * @type {DeletableItemList}
     * @private
     */
    addressList_: null,

    /**
     * The credit card list.
     * @type {DeletableItemList}
     * @private
     */
    creditCardList_: null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createAddressList_();
      this.createCreditCardList_();

      var self = this;
      $('autofill-add-address').onclick = function(event) {
        self.showAddAddressOverlay_();
      };
      $('autofill-add-creditcard').onclick = function(event) {
        self.showAddCreditCardOverlay_();
      };
      $('autofill-options-confirm').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
<if expr="is_macosx">
      $('autofill-use-mac-address-book-checkbox').onchange = function(event) {
        if (this.checked) {
          setTimeout(function() {
            // Prompt the user to give Chrome access to the user's Address
            // Book, if the user was not previously prompted. The dialog that
            // appears blocks the Chrome process, so wait for a small period of
            // time to allow the checkbox to appear filled in.
            chrome.send('accessAddressBook');
          }, 10);
        }
      };
</if>

      // TODO(jhawkins): What happens when Autofill is disabled whilst on the
      // Autofill options page?
    },

    /**
     * Creates, decorates and initializes the address list.
     * @private
     */
    createAddressList_: function() {
      this.addressList_ = $('address-list');
      options.autofillOptions.AutofillAddressList.decorate(this.addressList_);
      this.addressList_.autoExpands = true;
    },

    /**
     * Creates, decorates and initializes the credit card list.
     * @private
     */
    createCreditCardList_: function() {
      this.creditCardList_ = $('creditcard-list');
      options.autofillOptions.AutofillCreditCardList.decorate(
          this.creditCardList_);
      this.creditCardList_.autoExpands = true;
    },

    /**
     * Shows the 'Add address' overlay, specifically by loading the
     * 'Edit address' overlay and modifying the overlay title.
     * @private
     */
    showAddAddressOverlay_: function() {
      var title = loadTimeData.getString('addAddressTitle');
      AutofillEditAddressOverlay.setTitle(title);
      OptionsPage.navigateToPage('autofillEditAddress');
    },

    /**
     * Shows the 'Add credit card' overlay, specifically by loading the
     * 'Edit credit card' overlay and modifying the overlay title.
     * @private
     */
    showAddCreditCardOverlay_: function() {
      var title = loadTimeData.getString('addCreditCardTitle');
      AutofillEditCreditCardOverlay.setTitle(title);
      OptionsPage.navigateToPage('autofillEditCreditCard');
    },

    /**
     * Updates the data model for the address list with the values from
     * |entries|.
     * @param {Array} entries The list of addresses.
     */
    setAddressList_: function(entries) {
      this.addressList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Updates the data model for the credit card list with the values from
     * |entries|.
     * @param {Array} entries The list of credit cards.
     */
    setCreditCardList_: function(entries) {
      this.creditCardList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Removes the Autofill address or credit card represented by |guid|.
     * @param {string} guid The GUID of the address to remove.
     * @private
     */
    removeData_: function(guid) {
      chrome.send('removeData', [guid]);
    },

    /**
     * Requests profile data for the address represented by |guid| from the
     * PersonalDataManager. Once the data is loaded, the AutofillOptionsHandler
     * calls showEditAddressOverlay().
     * @param {string} guid The GUID of the address to edit.
     * @private
     */
    loadAddressEditor_: function(guid) {
      chrome.send('loadAddressEditor', [guid]);
    },

    /**
     * Requests profile data for the credit card represented by |guid| from the
     * PersonalDataManager. Once the data is loaded, the AutofillOptionsHandler
     * calls showEditCreditCardOverlay().
     * @param {string} guid The GUID of the credit card to edit.
     * @private
     */
    loadCreditCardEditor_: function(guid) {
      chrome.send('loadCreditCardEditor', [guid]);
    },

    /**
     * Shows the 'Edit address' overlay, using the data in |address| to fill the
     * input fields. |address| is a list with one item, an associative array
     * that contains the address data.
     * @private
     */
    showEditAddressOverlay_: function(address) {
      var title = loadTimeData.getString('editAddressTitle');
      AutofillEditAddressOverlay.setTitle(title);
      AutofillEditAddressOverlay.loadAddress(address);
      OptionsPage.navigateToPage('autofillEditAddress');
    },

    /**
     * Shows the 'Edit credit card' overlay, using the data in |credit_card| to
     * fill the input fields. |address| is a list with one item, an associative
     * array that contains the credit card data.
     * @private
     */
    showEditCreditCardOverlay_: function(creditCard) {
      var title = loadTimeData.getString('editCreditCardTitle');
      AutofillEditCreditCardOverlay.setTitle(title);
      AutofillEditCreditCardOverlay.loadCreditCard(creditCard);
      OptionsPage.navigateToPage('autofillEditCreditCard');
    },
  };

  AutofillOptions.setAddressList = function(entries) {
    AutofillOptions.getInstance().setAddressList_(entries);
  };

  AutofillOptions.setCreditCardList = function(entries) {
    AutofillOptions.getInstance().setCreditCardList_(entries);
  };

  AutofillOptions.removeData = function(guid) {
    AutofillOptions.getInstance().removeData_(guid);
  };

  AutofillOptions.loadAddressEditor = function(guid) {
    AutofillOptions.getInstance().loadAddressEditor_(guid);
  };

  AutofillOptions.loadCreditCardEditor = function(guid) {
    AutofillOptions.getInstance().loadCreditCardEditor_(guid);
  };

  AutofillOptions.editAddress = function(address) {
    AutofillOptions.getInstance().showEditAddressOverlay_(address);
  };

  AutofillOptions.editCreditCard = function(creditCard) {
    AutofillOptions.getInstance().showEditCreditCardOverlay_(creditCard);
  };

  // Export
  return {
    AutofillOptions: AutofillOptions
  };

});

