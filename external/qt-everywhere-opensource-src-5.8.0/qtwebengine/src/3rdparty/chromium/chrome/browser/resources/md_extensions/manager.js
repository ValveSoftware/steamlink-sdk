// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The different pages that can be shown at a time.
 * Note: This must remain in sync with the order in manager.html!
 * @enum {string}
 */
var Page = {
  ITEM_LIST: '0',
  DETAIL_VIEW: '1',
  KEYBOARD_SHORTCUTS: '2',
};

cr.define('extensions', function() {
  'use strict';

  /**
   * Compares two extensions to determine which should come first in the list.
   * @param {chrome.developerPrivate.ExtensionInfo} a
   * @param {chrome.developerPrivate.ExtensionInfo} b
   * @return {number}
   */
  var compareExtensions = function(a, b) {
    function compare(x, y) {
      return x < y ? -1 : (x > y ? 1 : 0);
    }
    function compareLocation(x, y) {
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
           compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
           compare(a.id, b.id);
  };

  var Manager = Polymer({
    is: 'extensions-manager',

    properties: {
      /** @type {extensions.Sidebar} */
      sidebar: Object,

      /** @type {extensions.ItemDelegate} */
      itemDelegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      filter: {
        type: String,
        value: '',
      },

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      extensions: {
        type: Array,
        value: function() { return []; },
      },

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      apps: {
        type: Array,
        value: function() { return []; },
      },
    },

    behaviors: [
      I18nBehavior,
    ],

    listeners: {
      'items-list.extension-item-show-details': 'showItemDetails_',
    },

    created: function() {
      this.readyPromiseResolver = new PromiseResolver();
    },

    ready: function() {
      /** @type {extensions.Sidebar} */
      this.sidebar =
          /** @type {extensions.Sidebar} */(this.$$('extensions-sidebar'));
      this.listHelper_ = new ListHelper(this);
      this.sidebar.setListDelegate(this.listHelper_);
      this.readyPromiseResolver.resolve();
    },

    /**
     * @param {!CustomEvent} event
     * @private
     */
    onFilterChanged_: function(event) {
      this.filter = /** @type {string} */ (event.detail);
    },

    /**
     * @param {chrome.developerPrivate.ExtensionType} type The type of item.
     * @return {string} The ID of the list that the item belongs in.
     * @private
     */
    getListId_: function(type) {
      var listId;
      var ExtensionType = chrome.developerPrivate.ExtensionType;
      switch (type) {
        case ExtensionType.HOSTED_APP:
        case ExtensionType.LEGACY_PACKAGED_APP:
        case ExtensionType.PLATFORM_APP:
          listId = 'apps';
          break;
        case ExtensionType.EXTENSION:
        case ExtensionType.SHARED_MODULE:
          listId = 'extensions';
          break;
        case ExtensionType.THEME:
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
          break;
      }
      assert(listId);
      return listId;
    },

    /**
     * @param {string} listId The list to look for the item in.
     * @param {string} itemId The id of the item to look for.
     * @return {number} The index of the item in the list, or -1 if not found.
     * @private
     */
    getIndexInList_: function(listId, itemId) {
      return this[listId].findIndex(function(item) {
        return item.id == itemId;
      });
    },

    /**
     * @return {boolean} Whether the list should be visible.
     * @private
     */
    computeListHidden_: function() {
      return this.$['items-list'].items.length == 0;
    },

    /**
     * Creates and adds a new extensions-item element to the list, inserting it
     * into its sorted position in the relevant section.
     * @param {!chrome.developerPrivate.ExtensionInfo} item The extension
     *     the new element is representing.
     */
    addItem: function(item) {
      var listId = this.getListId_(item.type);
      // We should never try and add an existing item.
      assert(this.getIndexInList_(listId, item.id) == -1);
      var insertBeforeChild = this[listId].findIndex(function(listEl) {
        return compareExtensions(listEl, item) > 0;
      });
      if (insertBeforeChild == -1)
        insertBeforeChild = this[listId].length;
      this.splice(listId, insertBeforeChild, 0, item);
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to update.
     */
    updateItem: function(item) {
      var listId = this.getListId_(item.type);
      var index = this.getIndexInList_(listId, item.id);
      // We should never try and update a non-existent item.
      assert(index >= 0);
      this.set([listId, index], item);
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to remove.
     */
    removeItem: function(item) {
      var listId = this.getListId_(item.type);
      var index = this.getIndexInList_(listId, item.id);
      // We should never try and remove a non-existent item.
      assert(index >= 0);
      this.splice(listId, index, 1);
    },

    /**
     * @param {Page} page
     * @return {!(extensions.KeyboardShortcuts |
     *            extensions.DetailView |
     *            extensions.ItemList)}
     * @private
     */
    getPage_: function(page) {
      switch (page) {
        case Page.ITEM_LIST:
          return this.$['items-list'];
        case Page.DETAIL_VIEW:
          return this.$['details-view'];
        case Page.KEYBOARD_SHORTCUTS:
          return this.$['keyboard-shortcuts'];
      }
      assertNotReached();
    },

    /**
     * Changes the active page selection.
     * @param {Page} toPage
     */
    changePage: function(toPage) {
      var fromPage = this.$.pages.selected;
      if (fromPage == toPage)
        return;
      var entry;
      var exit;
      if (fromPage == Page.ITEM_LIST && toPage == Page.DETAIL_VIEW) {
        entry = extensions.Animation.HERO;
        exit = extensions.Animation.HERO;
      } else if (toPage == Page.ITEM_LIST) {
        entry = extensions.Animation.FADE_IN;
        exit = extensions.Animation.SCALE_DOWN;
      } else {
        assert(toPage == Page.DETAIL_VIEW ||
               toPage == Page.KEYBOARD_SHORTCUTS);
        entry = extensions.Animation.FADE_IN;
        exit = extensions.Animation.FADE_OUT;
      }
      this.getPage_(fromPage).animationHelper.setExitAnimation(exit);
      this.getPage_(toPage).animationHelper.setEntryAnimation(entry);
      this.$.pages.selected = toPage;
    },

    /**
     * Shows the detailed view for a given item.
     * @param {CustomEvent} e
     * @private
     */
    showItemDetails_: function(e) {
      this.$['details-view'].set('data', assert(e.detail.element.data));
      this.changePage(Page.DETAIL_VIEW);
    },

    /** @private */
    onDetailsViewClose_: function() {
      this.changePage(Page.ITEM_LIST);
    }
  });

  /**
   * @param {extensions.Manager} manager
   * @constructor
   * @implements {extensions.SidebarListDelegate}
   */
  function ListHelper(manager) {
    this.manager_ = manager;
  }

  ListHelper.prototype = {
    /** @override */
    showType: function(type) {
      var items;
      switch (type) {
        case extensions.ShowingType.EXTENSIONS:
          items = this.manager_.extensions;
          break;
        case extensions.ShowingType.APPS:
          items = this.manager_.apps;
          break;
      }

      this.manager_.$['items-list'].set('items', assert(items));
      this.manager_.changePage(Page.ITEM_LIST);
    },

    /** @override */
    showKeyboardShortcuts: function() {
      this.manager_.changePage(Page.KEYBOARD_SHORTCUTS);
    },

    /** @override */
    showPackDialog: function() {
      this.manager_.$['pack-dialog'].show();
    }
  };

  return {Manager: Manager};
});
