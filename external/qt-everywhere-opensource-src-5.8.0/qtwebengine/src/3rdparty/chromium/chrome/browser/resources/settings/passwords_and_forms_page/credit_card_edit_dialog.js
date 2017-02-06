// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-credit-card-edit-dialog' is the dialog that allows
 * editing or creating a credit card entry.
 */

(function() {
'use strict';

Polymer({
  is: 'settings-credit-card-edit-dialog',

  properties: {
    /**
     * The credit card being edited
     * @type {!chrome.autofillPrivate.CreditCardEntry}
     */
    item: Object,

    /**
     * The actual title that's used for this dialog. Will be context sensitive
     * based on if |item| is being created or edited.
     * @private
     */
    title_: String,

    /**
     * The list of years to show in the dropdown.
     * @type {!Array<string>}
     */
    yearList_: Array,
  },

  behaviors: [
    I18nBehavior,
  ],

  /**
   * Needed to move from year to selected index.
   * @type {number}
   */
  firstYearInList_: 0,

  /**
   * Opens the dialog.
   * @param {!chrome.autofillPrivate.CreditCardEntry} item The card to edit.
   */
  open: function(item) {
    this.title_ =
        this.i18n(item.guid ? 'editCreditCardTitle' : 'addCreditCardTitle');

    // Add a leading '0' if a month is 1 char.
    if (item.expirationMonth.length == 1)
      item.expirationMonth = '0' + item.expirationMonth;

    var date = new Date();
    var firstYear = date.getFullYear();
    var lastYear = firstYear + 9;  // Show next 9 years (10 total).
    var selectedYear = parseInt(item.expirationYear, 10);

    // |selectedYear| must be valid and between first and last years.
    if (!selectedYear)
      selectedYear = firstYear;
    else if (selectedYear < firstYear)
      firstYear = selectedYear;
    else if (selectedYear > lastYear)
      lastYear = selectedYear;

    this.yearList_ = this.createYearList_(firstYear, lastYear);
    this.firstYearInList_ = firstYear;

    // Set |this.item| last because it has the selected year which won't be
    // valid until after the |this.yearList_| is set.
    this.item = item;

    this.$.dialog.open();
  },

  /** Closes the dialog. */
  close: function() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancelButtonTap_: function() {
    this.close();
  },

  /**
   * Handler for tapping the save button.
   * @private
   */
  onSaveButtonTap_: function() {
    this.fire('save-credit-card', this.item);
    this.close();
  },

  /**
   * Creates an array of years given a start and end (inclusive).
   * @param {number} firstYear
   * @param {number} lastYear
   * @return {!Array<string>}
   */
  createYearList_: function(firstYear, lastYear) {
    var yearList = [];
    for (var i = firstYear; i <= lastYear; ++i) {
      yearList.push(i.toString());
    }
    return yearList;
  },
});
})();
