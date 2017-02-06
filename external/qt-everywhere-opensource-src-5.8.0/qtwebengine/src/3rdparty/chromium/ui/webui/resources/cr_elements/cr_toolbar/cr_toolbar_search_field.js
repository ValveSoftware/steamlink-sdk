// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(tsergeant): Add tests for cr-toolbar-search-field.
Polymer({
  is: 'cr-toolbar-search-field',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // Prompt text to display in the search field.
    label: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,

    // When true, show a loading spinner to indicate that the backend is
    // processing the search. Will only show if the search field is open.
    spinnerActive: {
      type: Boolean,
      reflectToAttribute: true
    },

    /** @private */
    hasSearchText_: Boolean,
  },

  listeners: {
    'tap': 'showSearch_',
    'searchInput.bind-value-changed': 'onBindValueChanged_',
  },

  /**
   * @param {boolean} narrow
   * @return {number}
   * @private
   */
  computeIconTabIndex_: function(narrow) {
    return narrow ? 0 : -1;
  },

  /**
   * @param {boolean} spinnerActive
   * @param {boolean} showingSearch
   * @return {boolean}
   * @private
   */
  isSpinnerShown_: function(spinnerActive, showingSearch) {
    return spinnerActive && showingSearch;
  },

  /** @private */
  onInputBlur_: function() {
    if (!this.hasSearchText_)
      this.showingSearch = false;
  },

  /**
   * Update the state of the search field whenever the underlying input value
   * changes. Unlike onsearch or onkeypress, this is reliably called immediately
   * after any change, whether the result of user input or JS modification.
   * @private
   */
  onBindValueChanged_: function() {
    var newValue = this.$.searchInput.bindValue;
    this.hasSearchText_ = newValue != '';
    if (newValue != '')
      this.showingSearch = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  showSearch_: function(e) {
    if (e.target != this.$.clearSearch)
      this.showingSearch = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  hideSearch_: function(e) {
    this.showingSearch = false;
    e.stopPropagation();
  }
});
