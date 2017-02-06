// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   creditCardNumber: string,
 *   expirationMonth: string,
 *   expirationYear: string,
 *   guid: string,
 *   nameOnCard: string
 * }}
 * @see chrome/browser/ui/webui/options/autofill_options_handler.cc
 */
var CreditCardData;

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // AutofillOptions class:

  /**
   * Encapsulated handling of Autofill options page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function AutofillOptions() {
    Page.call(this, 'autofill',
              loadTimeData.getString('autofillOptionsPageTabTitle'),
              'autofill-options');
  }

  cr.addSingletonGetter(AutofillOptions);

  AutofillOptions.prototype = {
    __proto__: Page.prototype,

    /**
     * The address list.
     * @type {options.DeletableItemList}
     * @private
     */
    addressList_: null,

    /**
     * The credit card list.
     * @type {options.DeletableItemList}
     * @private
     */
    creditCardList_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

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
        PageManager.closeOverlay();
      };

      $('autofill-help').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_AutofillShowAbout']);
        return true;  // Always follow the href
      };

      // TODO(jhawkins): What happens when Autofill is disabled whilst on the
      // Autofill options page?
    },

    /**
     * Creates, decorates and initializes the address list.
     * @private
     */
    createAddressList_: function() {
      var addressList = $('address-list');
      options.autofillOptions.AutofillAddressList.decorate(addressList);
      this.addressList_ = assertInstanceof(addressList,
                                           options.DeletableItemList);
      this.addressList_.autoExpands = true;
    },

    /**
     * Creates, decorates and initializes the credit card list.
     * @private
     */
    createCreditCardList_: function() {
      var creditCardList = $('creditcard-list');
      options.autofillOptions.AutofillCreditCardList.decorate(creditCardList);
      this.creditCardList_ = assertInstanceof(creditCardList,
                                              options.DeletableItemList);
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
      PageManager.showPageByName('autofillEditAddress');
      AutofillEditAddressOverlay.prepForNewAddress();
    },

    /**
     * Shows the 'Add credit card' overlay, specifically by loading the
     * 'Edit credit card' overlay and modifying the overlay title.
     * @private
     */
    showAddCreditCardOverlay_: function() {
      var title = loadTimeData.getString('addCreditCardTitle');
      AutofillEditCreditCardOverlay.setTitle(title);
      PageManager.showPageByName('autofillEditCreditCard');
      AutofillEditCreditCardOverlay.prepForNewCard();
    },

    /**
     * Updates the data model for the address list with the values from
     * |entries|.
     * @param {!Array} entries The list of addresses.
     */
    setAddressList_: function(entries) {
      this.addressList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Updates the data model for the credit card list with the values from
     * |entries|.
     * @param {!Array} entries The list of credit cards.
     */
    setCreditCardList_: function(entries) {
      this.creditCardList_.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Removes the Autofill address or credit card represented by |guid|.
     * @param {string} guid The GUID of the address to remove.
     * @param {string=} metricsAction The name of the action to log for metrics.
     * @private
     */
    removeData_: function(guid, metricsAction) {
      chrome.send('removeData', [guid]);
      if (metricsAction)
        chrome.send('coreOptionsUserMetricsAction', [metricsAction]);
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
      PageManager.showPageByName('autofillEditAddress');
    },

    /**
     * Shows the 'Edit credit card' overlay, using the data in |credit_card| to
     * fill the input fields. |creditCard| is a list with one item, an
     * associative array that contains the credit card data.
     * @param {CreditCardData} creditCard
     * @private
     */
    showEditCreditCardOverlay_: function(creditCard) {
      var title = loadTimeData.getString('editCreditCardTitle');
      AutofillEditCreditCardOverlay.setTitle(title);
      AutofillEditCreditCardOverlay.loadCreditCard(creditCard);
      PageManager.showPageByName('autofillEditCreditCard');
    },
  };

  AutofillOptions.setAddressList = function(entries) {
    AutofillOptions.getInstance().setAddressList_(entries);
  };

  AutofillOptions.setCreditCardList = function(entries) {
    AutofillOptions.getInstance().setCreditCardList_(entries);
  };

  AutofillOptions.removeData = function(guid, metricsAction) {
    AutofillOptions.getInstance().removeData_(guid, metricsAction);
  };

  AutofillOptions.editAddress = function(address) {
    AutofillOptions.getInstance().showEditAddressOverlay_(address);
  };

  /**
   * @param {CreditCardData} creditCard
   */
  AutofillOptions.editCreditCard = function(creditCard) {
    AutofillOptions.getInstance().showEditCreditCardOverlay_(creditCard);
  };

  // Export
  return {
    AutofillOptions: AutofillOptions
  };

});

