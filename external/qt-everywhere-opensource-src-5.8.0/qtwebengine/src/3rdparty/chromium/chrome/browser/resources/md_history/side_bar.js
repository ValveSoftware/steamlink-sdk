// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-side-bar',

  properties: {
    selectedPage: {
      type: String,
      notify: true
    },
  },

  toggle: function() {
    this.$.drawer.toggle();
  },

  /** @private */
  onDrawerFocus_: function() {
    // The desired behavior is for the app-drawer to focus the currently
    // selected menu item on opening. However, it will always focus the first
    // focusable child. Therefore, we set tabindex=0 on the app-drawer so that
    // it will focus itself and then immediately delegate focus to the selected
    // item in this listener.
    this.$.menu.selectedItem.focus();
  },

  /** @private */
  onSelectorActivate_: function() {
    this.$.drawer.close();
  },

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   * @private
   */
  onClearBrowsingDataTap_: function() {
    window.location.href = 'chrome://settings/clearBrowserData';
  },
});
