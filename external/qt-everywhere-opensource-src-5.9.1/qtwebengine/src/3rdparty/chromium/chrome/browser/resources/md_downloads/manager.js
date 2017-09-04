// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Manager = Polymer({
    is: 'downloads-manager',

    properties: {
      /** @private */
      hasDownloads_: {
        observer: 'hasDownloadsChanged_',
        type: Boolean,
      },

      /** @private */
      hasShadow_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      inSearchMode_: {
        type: Boolean,
        value: false,
      },

      /** @private {!Array<!downloads.Data>} */
      items_: {
        type: Array,
        value: function() { return []; },
      },

      /** @private */
      spinnerActive_: {
        type: Boolean,
        notify: true,
      },
    },

    hostAttributes: {
      loading: true,
    },

    listeners: {
      'downloads-list.scroll': 'onListScroll_',
      'toolbar.search-changed': 'onSearchChanged_',
    },

    observers: [
      'itemsChanged_(items_.*)',
    ],

    /** @private */
    clearAll_: function() {
      this.set('items_', []);
    },

    /** @private */
    hasDownloadsChanged_: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory'))
        this.$.toolbar.downloadsShowing = this.hasDownloads_;

      if (this.hasDownloads_)
        this.$['downloads-list'].fire('iron-resize');
    },

    /**
     * @param {number} index
     * @param {!Array<!downloads.Data>} list
     * @private
     */
    insertItems_: function(index, list) {
      this.splice.apply(this, ['items_', index, 0].concat(list));
      this.updateHideDates_(index, index + list.length);
      this.removeAttribute('loading');
      this.spinnerActive_ = false;
    },

    /** @private */
    itemsChanged_: function() {
      this.hasDownloads_ = this.items_.length > 0;
    },

    /**
     * @return {string} The text to show when no download items are showing.
     * @private
     */
    noDownloadsText_: function() {
      return loadTimeData.getString(
          this.inSearchMode_ ? 'noSearchResults' : 'noDownloads');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
      switch (e.command.id) {
        case 'undo-command':
          e.canExecute = this.$.toolbar.canUndo();
          break;
        case 'clear-all-command':
          e.canExecute = this.$.toolbar.canClearAll();
          break;
        case 'find-command':
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'clear-all-command')
        downloads.ActionService.getInstance().clearAll();
      else if (e.command.id == 'undo-command')
        downloads.ActionService.getInstance().undo();
      else if (e.command.id == 'find-command')
        this.$.toolbar.onFindCommand();
    },

    /** @private */
    onListScroll_: function() {
      var list = this.$['downloads-list'];
      if (list.scrollHeight - list.scrollTop - list.offsetHeight <= 100) {
        // Approaching the end of the scrollback. Attempt to load more items.
        downloads.ActionService.getInstance().loadMore();
      }
      this.hasShadow_ = list.scrollTop > 0;
    },

    /** @private */
    onLoad_: function() {
      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      downloads.ActionService.getInstance().loadMore();
    },

    /** @private */
    onSearchChanged_: function() {
      this.inSearchMode_ = downloads.ActionService.getInstance().isSearching();
    },

    /**
     * @param {number} index
     * @private
     */
    removeItem_: function(index) {
      this.splice('items_', index, 1);
      this.updateHideDates_(index, index);
      this.onListScroll_();
    },

    /**
     * @param {number} start
     * @param {number} end
     * @private
     */
    updateHideDates_: function(start, end) {
      for (var i = start; i <= end; ++i) {
        var current = this.items_[i];
        if (!current)
          continue;
        var prev = this.items_[i - 1];
        var hideDate = !!prev && prev.date_string == current.date_string;
        this.set('items_.' + i + '.hideDate', hideDate);
      }
    },

    /**
     * @param {number} index
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(index, data) {
      this.set('items_.' + index, data);
      this.updateHideDates_(index, index);
      var list = /** @type {!IronListElement} */(this.$['downloads-list']);
      list.updateSizeForItem(index);
    },
  });

  Manager.clearAll = function() {
    Manager.get().clearAll_();
  };

  /** @return {!downloads.Manager} */
  Manager.get = function() {
    return /** @type {!downloads.Manager} */(
        queryRequiredElement('downloads-manager'));
  };

  Manager.insertItems = function(index, list) {
    Manager.get().insertItems_(index, list);
  };

  Manager.onLoad = function() {
    Manager.get().onLoad_();
  };

  Manager.removeItem = function(index) {
    Manager.get().removeItem_(index);
  };

  Manager.updateItem = function(index, data) {
    Manager.get().updateItem_(index, data);
  };

  return {Manager: Manager};
});
