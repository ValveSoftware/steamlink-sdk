// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses and credit cards for use in autofill.
 */
(function() {
  'use strict';

  Polymer({
    is: 'settings-autofill-section',

    behaviors: [I18nBehavior],

    properties: {
      /**
       * An array of saved addresses.
       * @type {!Array<!chrome.autofillPrivate.AddressEntry>}
       */
      addresses: Array,

      /**
       * An array of saved addresses.
       * @type {!Array<!chrome.autofillPrivate.CreditCardEntry>}
       */
      creditCards: Array,
    },

    listeners: {
      'addressList.scroll': 'closeMenu_',
      'creditCardList.scroll': 'closeMenu_',
      'tap': 'closeMenu_',
    },

    /**
     * Formats an AddressEntry so it's displayed as an address.
     * @param {!chrome.autofillPrivate.AddressEntry} item
     * @return {string}
     */
    address_: function(item) {
      return item.metadata.summaryLabel + item.metadata.summarySublabel;
    },

    /**
     * Formats the expiration date so it's displayed as MM/YYYY.
     * @param {!chrome.autofillPrivate.CreditCardEntry} item
     * @return {string}
     */
    expiration_: function(item) {
      return item.expirationMonth + '/' + item.expirationYear;
    },

    /**
     * Toggles the address overflow menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onAddressMenuTap_: function(e) {
      // Close the other menu.
      this.$.creditCardSharedMenu.closeMenu();

      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);
      var address = /** @type {!chrome.autofillPrivate.AddressEntry} */(
          menuEvent.model.item);
      this.$.menuRemoveAddress.hidden = !address.metadata.isLocal;
      this.$.addressSharedMenu.toggleMenu(Polymer.dom(e).localTarget, address);
      e.stopPropagation();  // Prevent the tap event from closing the menu.
    },

    /**
     * Handles tapping on the "Add address" button.
     * @param {!Event} e
     * @private
     */
    onAddAddressTap_: function(e) {
      // TODO(hcarmona): implement adding an address.
      e.preventDefault();
    },

    /**
     * Handles tapping on the "Edit" address button.
     * @private
     */
    onMenuEditAddressTap_: function() {
      var menu = this.$.addressSharedMenu;
      /** @type {chrome.autofillPrivate.AddressEntry} */
      var address = menu.itemData;

      // TODO(hcarmona): implement editing a local address.

      if (!address.metadata.isLocal)
        window.open(this.i18n('manageAddressesUrl'));

      menu.closeMenu();
    },

    /**
     * Handles tapping on the "Remove" address button.
     * @private
     */
    onMenuRemoveAddressTap_: function() {
      var menu = this.$.addressSharedMenu;
      this.fire('remove-address', menu.itemData);
      menu.closeMenu();
    },

    /**
     * Toggles the credit card overflow menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onCreditCardMenuTap_: function(e) {
      // Close the other menu.
      this.$.addressSharedMenu.closeMenu();

      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);
      var creditCard = /** @type {!chrome.autofillPrivate.CreditCardEntry} */(
          menuEvent.model.item);
      this.$.menuRemoveCreditCard.hidden = !creditCard.metadata.isLocal;
      this.$.menuClearCreditCard.hidden = !creditCard.metadata.isCached;
      this.$.creditCardSharedMenu.toggleMenu(
          Polymer.dom(e).localTarget, creditCard);
      e.stopPropagation();  // Prevent the tap event from closing the menu.
    },

    /**
     * Handles tapping on the "Add credit card" button.
     * @param {!Event} e
     * @private
     */
    onAddCreditCardTap_: function(e) {
      var date = new Date();  // Default to current month/year.
      var expirationMonth = date.getMonth() + 1;  // Months are 0 based.
      // Pass in a new object to edit.
      this.$.editCreditCardDialog.open({
        expirationMonth: expirationMonth.toString(),
        expirationYear: date.getFullYear().toString(),
      });
      e.preventDefault();
    },

    /**
     * Handles tapping on the "Edit" credit card button.
     * @private
     */
    onMenuEditCreditCardTap_: function() {
      var menu = this.$.creditCardSharedMenu;
      /** @type {chrome.autofillPrivate.CreditCardEntry} */
      var creditCard = menu.itemData;

      if (creditCard.metadata.isLocal)
        this.$.editCreditCardDialog.open(creditCard);
      else
        window.open(this.i18n('manageCreditCardsUrl'));

      menu.closeMenu();
    },

    /**
     * Handles tapping on the "Remove" credit card button.
     * @private
     */
    onMenuRemoveCreditCardTap_: function() {
      var menu = this.$.creditCardSharedMenu;
      this.fire('remove-credit-card', menu.itemData);
      menu.closeMenu();
    },

    /**
     * Handles tapping on the "Clear copy" button for cached credit cards.
     * @private
     */
    onMenuClearCreditCardTap_: function() {
      var menu = this.$.creditCardSharedMenu;
      this.fire('clear-credit-card', menu.itemData);
      menu.closeMenu();
    },

    /**
     * Closes the overflow menus.
     * @private
     */
    closeMenu_: function() {
      this.$.addressSharedMenu.closeMenu();
      this.$.creditCardSharedMenu.closeMenu();
    },
  });
})();
