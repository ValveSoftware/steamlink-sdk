// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-side-bar',

  behaviors: [Polymer.IronA11yKeysBehavior],

  properties: {
    selectedPage: {type: String, notify: true},

    showFooter: Boolean,

    // If true, the sidebar is contained within an app-drawer.
    drawer: {type: Boolean, reflectToAttribute: true},
  },

  keyBindings: {
    'space:keydown': 'onSpacePressed_',
  },

  /**
   * @param {CustomEvent} e
   * @private
   */
  onSpacePressed_: function(e) {
    e.detail.keyboardEvent.path[0].click();
  },

  /**
   * @private
   */
  onSelectorActivate_: function() { this.fire('history-close-drawer'); },

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   * @param {Event} e
   * @private
   */
  onClearBrowsingDataTap_: function(e) {
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('InitClearBrowsingData');
    browserService.openClearBrowsingData();
    /** @type {PaperRippleElement} */(this.$['cbd-ripple']).upAction();
    e.preventDefault();
  },

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   * @private
   */
  onItemClick_: function(e) {
    e.preventDefault();
  },
});
