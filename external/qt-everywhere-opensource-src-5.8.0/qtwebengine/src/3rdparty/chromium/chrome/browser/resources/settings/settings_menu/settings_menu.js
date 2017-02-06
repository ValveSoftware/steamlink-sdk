// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'settings-menu',

  properties: {
    /** @private */
    advancedOpened_: Boolean,

    /**
     * The current active route.
     * @type {!SettingsRoute}
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },
  },

  attached: function() {
    document.addEventListener('toggle-advanced-page', function(e) {
      if (e.detail)
        this.$.advancedPage.open();
      else
        this.$.advancedPage.close();
    }.bind(this));

    this.$.advancedPage.addEventListener('paper-submenu-open', function() {
      this.fire('toggle-advanced-page', true);
    }.bind(this));

    this.$.advancedPage.addEventListener('paper-submenu-close', function() {
      this.fire('toggle-advanced-page', false);
    }.bind(this));

    this.fire('toggle-advanced-page', this.currentRoute.page == 'advanced');
  },

  /**
   * @param {!SettingsRoute} newRoute
   * @private
   */
  currentRouteChanged_: function(newRoute) {
    // Sync URL changes to the side nav menu.

    if (newRoute.page == 'advanced') {
      this.$.advancedMenu.selected = this.currentRoute.section;
      this.$.basicMenu.selected = null;
    } else if (newRoute.page == 'basic') {
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = this.currentRoute.section;
    } else {
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = null;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  openPage_: function(event) {
    var submenuRoute = event.currentTarget.parentNode.dataset.page;
    if (submenuRoute) {
      this.currentRoute = {
        page: submenuRoute,
        section: event.currentTarget.dataset.section,
        subpage: [],
        url: '',
      };
    }
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'cr:arrow-drop-down';
  },
});
