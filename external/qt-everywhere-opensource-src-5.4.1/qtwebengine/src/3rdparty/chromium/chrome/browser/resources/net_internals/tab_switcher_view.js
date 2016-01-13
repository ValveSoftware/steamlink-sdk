// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller and View for switching between tabs.
 *
 * The View part of TabSwitcherView displays the contents of the currently
 * selected tab (only one tab can be active at a time).
 *
 * The controller part of TabSwitcherView hooks up a dropdown menu (i.e. HTML
 * SELECT) to control switching between tabs.
 */
var TabSwitcherView = (function() {
  'use strict';

  // We inherit from View.
  var superClass = View;

  /**
   * @constructor
   *
   * @param {DOMSelectNode} dropdownMenu The menu for switching between tabs.
   *                        The TabSwitcherView will attach an onchange event to
   *                        the dropdown menu, and control the selected index.
   * @param {?Function} opt_onTabSwitched Optional callback to run when the
   *                    active tab changes. Called as
   *                    opt_onTabSwitched(oldTabId, newTabId).
   */
  function TabSwitcherView(dropdownMenu, opt_onTabSwitched) {
    assertFirstConstructorCall(TabSwitcherView);

    this.tabIdToView_ = {};
    this.activeTabId_ = null;

    this.dropdownMenu_ = dropdownMenu;
    this.dropdownMenu_.onchange = this.onMenuSelectionChanged_.bind(this);

    this.onTabSwitched_ = opt_onTabSwitched;

    superClass.call(this);
  }

  TabSwitcherView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    // ---------------------------------------------
    // Override methods in View
    // ---------------------------------------------

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      // Position each of the tabs content areas.
      for (var tabId in this.tabIdToView_) {
        var view = this.tabIdToView_[tabId];
        view.setGeometry(left, top, width, height);
      }
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      var activeView = this.getActiveTabView();
      if (activeView)
        activeView.show(isVisible);
    },

    // ---------------------------------------------

    /**
     * Adds a new tab (initially hidden).
     *
     * @param {string} tabId The ID to refer to the tab by.
     * @param {!View} view The tab's actual contents.
     * @param {string} name The name for the menu item that selects the tab.
     */
    addTab: function(tabId, view, name) {
      if (!tabId) {
        throw Error('Must specify a non-false tabId');
      }

      this.tabIdToView_[tabId] = view;

      // Tab content views start off hidden.
      view.show(false);

      // Add it to the dropdown menu.
      var menuItem = addNode(this.dropdownMenu_, 'option');
      menuItem.value = tabId;
      menuItem.textContent = name;
    },

    showMenuItem: function(tabId, isVisible) {
      var wasActive = this.activeTabId_ == tabId;

      // Hide the menuitem from the list. Note it needs to be 'disabled' to
      // prevent it being selectable from keyboard.
      var menuitem = this.getMenuItemNode_(tabId);
      setNodeDisplay(menuitem, isVisible);
      menuitem.disabled = !isVisible;

      if (wasActive && !isVisible) {
        // If the active tab is being hidden in the dropdown menu, then
        // switch to the first tab which is still visible.
        for (var i = 0; i < this.dropdownMenu_.options.length; ++i) {
          var option = this.dropdownMenu_.options[i];
          if (option.style.display != 'none') {
            this.switchToTab(option.value);
            break;
          }
        }
      }
    },

    getAllTabViews: function() {
      return this.tabIdToView_;
    },

    getTabView: function(tabId) {
      return this.tabIdToView_[tabId];
    },

    getActiveTabView: function() {
      return this.tabIdToView_[this.activeTabId_];
    },

    getActiveTabId: function() {
      return this.activeTabId_;
    },

    /**
     * Changes the currently active tab to |tabId|. This has several effects:
     *   (1) Replace the tab contents view with that of the new tab.
     *   (2) Update the dropdown menu's current selection.
     *   (3) Invoke the optional onTabSwitched callback.
     */
    switchToTab: function(tabId) {
      var newView = this.getTabView(tabId);

      if (!newView) {
        throw Error('Invalid tabId');
      }

      var oldTabId = this.activeTabId_;
      this.activeTabId_ = tabId;

      this.dropdownMenu_.value = tabId;

      // Hide the previously visible tab contents.
      if (oldTabId)
        this.getTabView(oldTabId).show(false);

      newView.show(this.isVisible());

      if (this.onTabSwitched_)
        this.onTabSwitched_(oldTabId, tabId);
    },

    getMenuItemNode_: function(tabId) {
      for (var i = 0; i < this.dropdownMenu_.options.length; ++i) {
        var option = this.dropdownMenu_.options[i];
        if (option.value == tabId) {
          return option;
        }
      }
      return null;
    },

    onMenuSelectionChanged_: function(event) {
      var tabId = this.dropdownMenu_.value;
      this.switchToTab(tabId);
    },
  };

  return TabSwitcherView;
})();
