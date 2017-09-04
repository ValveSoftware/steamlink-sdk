// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Toolbar = Polymer({
    is: 'downloads-toolbar',

    properties: {
      downloadsShowing: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: 'downloadsShowingChanged_',
      },

      spinnerActive: {
        type: Boolean,
        notify: true,
      },
    },

    listeners: {
      'paper-dropdown-close': 'onPaperDropdownClose_',
      'paper-dropdown-open': 'onPaperDropdownOpen_',
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return !this.$.toolbar.getSearchField().isSearchFocused();
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return !this.$.toolbar.getSearchField().getValue() &&
          this.downloadsShowing;
    },

    onFindCommand: function() {
      this.$.toolbar.getSearchField().showAndFocus();
    },

    /** @private */
    closeMoreActions_: function() {
      this.$.more.close();
    },

    /** @private */
    downloadsShowingChanged_: function() {
      this.updateClearAll_();
    },

    /** @private */
    onClearAllTap_: function() {
      assert(this.canClearAll());
      downloads.ActionService.getInstance().clearAll();
    },

    /** @private */
    onPaperDropdownClose_: function() {
      window.removeEventListener('resize', assert(this.boundClose_));
    },

    /**
     * @param {!Event} e
     * @private
     */
    onItemBlur_: function(e) {
      var menu = /** @type {PaperMenuElement} */(this.$$('paper-menu'));
      if (menu.items.indexOf(e.relatedTarget) >= 0)
        return;

      this.$.more.restoreFocusOnClose = false;
      this.closeMoreActions_();
      this.$.more.restoreFocusOnClose = true;
    },

    /** @private */
    onPaperDropdownOpen_: function() {
      this.boundClose_ = this.boundClose_ || this.closeMoreActions_.bind(this);
      window.addEventListener('resize', this.boundClose_);
    },

    /**
     * @param {!CustomEvent} event
     * @private
     */
    onSearchChanged_: function(event) {
      var actionService = downloads.ActionService.getInstance();
      if (actionService.search(/** @type {string} */ (event.detail)))
        this.spinnerActive = actionService.isSearching();
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderTap_: function() {
      downloads.ActionService.getInstance().openDownloadsFolder();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('paper-menu .clear-all').hidden = !this.canClearAll();
    },
  });

  return {Toolbar: Toolbar};
});
