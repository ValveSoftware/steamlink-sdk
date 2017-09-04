// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'settings-menu',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    advancedOpened: {
      type: Boolean,
      notify: true,
    },

    /** @private */
    aboutSelected_: Boolean,

    /**
     * Dictionary defining page visibility.
     * @type {!GuestModePageVisibility}
     */
    pageVisibility: {
      type: Object,
    },
  },

  ready: function() {
    // When a <paper-submenu> is created with the [opened] attribute as true,
    // its _active member isn't correctly initialized. See this bug for more
    // info: https://github.com/PolymerElements/paper-menu/issues/88. This means
    // the first tap to close an opened Advanced section does nothing (because
    // it calls .open() on an opened menu instead of .close(). This is a fix for
    // that bug without changing that code through its public API.
    //
    // TODO(dbeam): we're currently deciding whether <paper-{,sub}menu> are
    // right for our needs (there have been minor a11y problems). If we decide
    // to keep <paper-{,sub}menu>, fix this bug with a local Chrome CL (ex:
    // https://codereview.chromium.org/2412343004) or a Polymer PR (ex:
    // https://github.com/PolymerElements/paper-menu/pull/107).
    if (this.advancedOpened)
      this.$.advancedPage.open();
  },

  /**
   * @param {!settings.Route} newRoute
   */
  currentRouteChanged: function(newRoute) {
    // Make the three menus mutually exclusive.
    if (settings.Route.ABOUT.contains(newRoute)) {
      this.aboutSelected_ = true;
      this.$.advancedMenu.selected = null;
      this.$.basicMenu.selected = null;
    } else if (settings.Route.ADVANCED.contains(newRoute)) {
      this.aboutSelected_ = false;
      // For routes from URL entry, we need to set selected.
      this.$.advancedMenu.selected = newRoute.path;
      this.$.basicMenu.selected = null;
    } else if (settings.Route.BASIC.contains(newRoute)) {
      this.aboutSelected_ = false;
      this.$.advancedMenu.selected = null;
      // For routes from URL entry, we need to set selected.
      this.$.basicMenu.selected = newRoute.path;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  openPage_: function(event) {
    var route = settings.getRouteForPath(event.currentTarget.dataset.path);
    assert(route, 'settings-menu has an an entry with an invalid path');
    settings.navigateTo(
        route, /* dynamicParams */ null, /* removeSearch */ true);
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
