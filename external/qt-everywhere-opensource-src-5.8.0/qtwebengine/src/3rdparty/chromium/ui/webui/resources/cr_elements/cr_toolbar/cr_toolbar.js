// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-toolbar',

  properties: {
    // Name to display in the toolbar, in titlecase.
    pageName: String,

    // Prompt text to display in the search field.
    searchPrompt: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,

    // Value is proxied through to cr-toolbar-search-field. When true,
    // the search field will show a processing spinner.
    spinnerActive: Boolean,

    /** @private */
    narrow_: {
      type: Boolean,
      reflectToAttribute: true
    },

    /** @private */
    showingSearch_: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  /** @return {!CrToolbarSearchFieldElement} */
  getSearchField: function() {
    return this.$.search;
  },

  /** @private */
  onMenuTap_: function(e) {
    this.fire('cr-menu-tap');
  }
});
