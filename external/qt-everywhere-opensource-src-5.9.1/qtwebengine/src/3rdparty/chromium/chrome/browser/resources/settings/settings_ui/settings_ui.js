// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
cr.exportPath('settings');
assert(!settings.defaultResourceLoaded,
       'settings_ui.js run twice. You probably have an invalid import.');
/** Global defined when the main Settings script runs. */
settings.defaultResourceLoaded = true;

Polymer({
  is: 'settings-ui',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,

    /** @type {?settings.DirectionDelegate} */
    directionDelegate: {
      observer: 'directionDelegateChanged_',
      type: Object,
      value: new settings.DirectionDelegateImpl(),
    },

    /** @private */
    advancedOpened_: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {boolean} */
    toolbarSpinnerActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * Dictionary defining page visibility.
     * @private {!GuestModePageVisibility}
     */
    pageVisibility_: Object,

    /** @private */
    lastSearchQuery_: {
      type: String,
      value: '',
    }
  },

  listeners: {
    'refresh-pref': 'onRefreshPref_',
  },

  /** @override */
  created: function() {
    settings.initializeRouteFromUrl();
  },

  /**
   * @override
   * @suppress {es5Strict} Object literals cannot contain duplicate keys in ES5
   *     strict mode.
   */
  ready: function() {
    // Lazy-create the drawer the first time it is opened or swiped into view.
    var drawer = assert(this.$$('app-drawer'));
    listenOnce(drawer, 'track opened-changed', function() {
      this.$.drawerTemplate.if = true;
    }.bind(this));

    window.addEventListener('popstate', function(e) {
      drawer.close();
    }.bind(this));

    if (loadTimeData.getBoolean('isGuest')) {
      this.pageVisibility_ = {
        people: false,
        onStartup: false,
        reset: false,
<if expr="not chromeos">
        appearance: false,
        defaultBrowser: false,
        advancedSettings: false,
</if>
<if expr="chromeos">
        appearance: {
          setWallpaper: false,
          setTheme: false,
          homeButton: false,
          bookmarksBar: false,
          pageZoom: false,
        },
        advancedSettings: true,
        privacy: {
          searchPrediction: false,
          networkPrediction: false,
        },
        passwordsAndForms: false,
        downloads: {
          googleDrive: false,
        },
</if>
      };
    }
  },

  /** @override */
  attached: function() {
    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    settings.setGlobalScrollTarget(this.$.headerPanel.scroller);
  },

  /** @param {!settings.Route} route */
  currentRouteChanged: function(route) {
    var urlSearchQuery = settings.getQueryParameters().get('search') || '';
    if (urlSearchQuery == this.lastSearchQuery_)
      return;

    this.lastSearchQuery_ = urlSearchQuery;

    var toolbar = /** @type {!CrToolbarElement} */ (this.$$('cr-toolbar'));
    var searchField = /** @type {CrToolbarSearchFieldElement} */ (
        toolbar.getSearchField());

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery != searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onRefreshPref_: function(e) {
    var prefName = /** @type {string} */(e.detail);
    return /** @type {SettingsPrefsElement} */(this.$.prefs).refresh(prefName);
  },

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * @param {!Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    // Trim leading whitespace only, to prevent searching for empty string. This
    // still allows the user to search for 'foo bar', while taking a long pause
    // after typing 'foo '.
    var query = e.detail.replace(/^\s+/, '');
    // Prevent duplicate history entries.
    if (query == this.lastSearchQuery_)
      return;

    settings.navigateTo(
        settings.Route.BASIC,
        query.length > 0 ? new URLSearchParams(`search=${query}`) : undefined,
        /* removeSearch */ true);
  },

  /**
   * @param {Event} event
   * @private
   */
  onIronActivate_: function(event) {
    if (event.detail.item.id != 'advancedPage')
      this.$$('app-drawer').close();
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.$$('app-drawer').toggle();
  },

  /** @private */
  directionDelegateChanged_: function() {
    this.$$('app-drawer').align = this.directionDelegate.isRtl() ?
        'right' : 'left';
  },
});
