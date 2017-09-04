// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SearchField = Polymer({
  is: 'cr-search-field',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    value_: String,
  },

  /** @return {!HTMLInputElement} */
  getSearchInput: function() {
    return this.$.searchInput;
  },

  /** @private */
  clearSearch_: function() {
    this.setValue('');
    this.getSearchInput().focus();
  },

  /** @private */
  toggleShowingSearch_: function() {
    this.showingSearch = !this.showingSearch;
  },
});
