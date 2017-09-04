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
    hasSearchText_: {
      type: Boolean,
      reflectToAttribute: true
    },

    /** @private */
    isSpinnerShown_: {
      type: Boolean,
      computed: 'computeIsSpinnerShown_(spinnerActive, showingSearch)'
    },

    /** @private */
    searchFocused_: {
      type: Boolean,
      value: false
    },
  },

  listeners: {
    // Deliberately uses 'click' instead of 'tap' to fix crbug.com/624356.
    'click': 'showSearch_',
  },

  /** @return {!HTMLInputElement} */
  getSearchInput: function() {
    return this.$.searchInput;
  },

  /**
   * Sets the value of the search field. Overridden from CrSearchFieldBehavior.
   * @param {string} value
   * @param {boolean=} opt_noEvent Whether to prevent a 'search-changed' event
   *     firing for this change.
   */
  setValue: function(value, opt_noEvent) {
    CrSearchFieldBehavior.setValue.call(this, value, opt_noEvent);
    this.onSearchInput_();
  },

  /** @return {boolean} */
  isSearchFocused: function() {
    return this.searchFocused_;
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
   * @return {boolean}
   * @private
   */
  computeIsSpinnerShown_: function() {
    return this.spinnerActive && this.showingSearch;
  },

  /** @private */
  onInputFocus_: function() {
    this.searchFocused_ = true;
  },

  /** @private */
  onInputBlur_: function() {
    this.searchFocused_ = false;
    if (!this.hasSearchText_)
      this.showingSearch = false;
  },

  /**
   * Update the state of the search field whenever the underlying input value
   * changes. Unlike onsearch or onkeypress, this is reliably called immediately
   * after any change, whether the result of user input or JS modification.
   * @private
   */
  onSearchInput_: function() {
    var newValue = this.$.searchInput.value;
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
  clearSearch_: function(e) {
    this.setValue('');
    this.getSearchInput().focus();
  }
});
