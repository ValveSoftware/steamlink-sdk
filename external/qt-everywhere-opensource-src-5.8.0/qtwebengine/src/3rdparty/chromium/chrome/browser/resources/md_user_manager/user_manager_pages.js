// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'user-manager-pages' is the element that controls paging in the
 * user manager screen.
 */
Polymer({
  is: 'user-manager-pages',

  properties: {
    /**
     * ID of the currently selected page.
     * @private {string}
     */
    selectedPage_: {
      type: String,
      value: 'user-pods-page'
    },

    /**
     * Data passed to the currently selected page.
     * @private {?Object}
     */
    pageData_: {
      type: Object,
      value: null
    }
  },

  listeners: {
    'change-page': 'changePage_'
  },

  /**
   * Changes the currently selected page.
   * @param {Event} e The event containing ID of the selected page.
   * @private
   */
  changePage_: function(e) {
    this.pageData_ = e.detail.data || null;
    this.selectedPage_ = e.detail.page;
  },

  /**
   * Returns True if the first argument is present in the given set of values.
   * @param {string} selectedPage ID of the currently selected page.
   * @param {...string} var_args Pages IDs to check the first argument against.
   * @return {boolean}
   */
  isPresentIn_: function(selectedPage, var_args) {
    var pages = Array.prototype.slice.call(arguments, 1);
    return pages.indexOf(selectedPage) !== -1;
  }
});
