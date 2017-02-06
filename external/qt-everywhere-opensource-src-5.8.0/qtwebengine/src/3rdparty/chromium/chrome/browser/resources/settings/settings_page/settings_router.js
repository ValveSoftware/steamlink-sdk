// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   url: string,
 *   page: string,
 *   section: string,
 *   subpage: !Array<string>,
 *   dialog: (string|undefined),
 * }}
 */
var SettingsRoute;

/**
 * @fileoverview
 * 'settings-router' is a simple router for settings. Its responsibilities:
 *  - Update the URL when the routing state changes.
 *  - Initialize the routing state with the initial URL.
 *  - Process and validate all routing state changes.
 *
 * Example:
 *
 *    <settings-router current-route="{{currentRoute}}">
 *    </settings-router>
 */
Polymer({
  is: 'settings-router',

  properties: {
    /**
     * The current active route. This is reflected to the URL. Updates to this
     * property should replace the whole object.
     *
     * currentRoute.page refers to top-level pages such as Basic and Advanced.
     *
     * currentRoute.section is only non-empty when the user is on a subpage. If
     * the user is on Basic, for instance, this is an empty string.
     *
     * currentRoute.subpage is an Array. The last element is the actual subpage
     * the user is on. The previous elements are the ancestor subpages. This
     * enables support for multiple paths to the same subpage. This is used by
     * both the Back button and the Breadcrumb to determine ancestor subpages.
     * @type {SettingsRoute}
     */
    currentRoute: {
      notify: true,
      observer: 'currentRouteChanged_',
      type: Object,
      value: function() {
        // Take the current URL, find a matching pre-defined route, and
        // initialize the currentRoute to that pre-defined route.
        for (var i = 0; i < this.routes_.length; ++i) {
          var route = this.routes_[i];
          if (route.url == window.location.pathname) {
            return {
              url: route.url,
              page: route.page,
              section: route.section,
              subpage: route.subpage,
              dialog: route.dialog,
            };
          }
        }

        // As a fallback return the default route.
        return this.routes_[0];
      },
    },

    /**
     * Page titles for the currently active route. Updated by the currentRoute
     * property observer.
     * @type {{pageTitle: string}}
     */
    currentRouteTitles: {
      notify: true,
      type: Object,
      value: function() {
        return {
          pageTitle: '',
        };
      },
    },
  },


  /**
   * @private {!Array<!SettingsRoute>}
   * The 'url' property is not accessible to other elements.
   */
  routes_: [
    {
      url: '/',
      page: 'basic',
      section: '',
      subpage: [],
    },
    {
      url: '/help',
      page: 'about',
      section: '',
      subpage: [],
    },
<if expr="chromeos">
    {
      url: '/help/details',
      page: 'about',
      section: 'about',
      subpage: ['detailed-build-info'],
    },
</if>
    {
      url: '/advanced',
      page: 'advanced',
      section: '',
      subpage: [],
    },
<if expr="chromeos">
    {
      url: '/internet',
      page: 'basic',
      section: 'internet',
      subpage: [],
    },
    {
      url: '/networkDetail',
      page: 'basic',
      section: 'internet',
      subpage: ['network-detail'],
    },
    {
      url: '/knownNetworks',
      page: 'basic',
      section: 'internet',
      subpage: ['known-networks'],
    },
</if>
    {
      url: '/appearance',
      page: 'basic',
      section: 'appearance',
      subpage: [],
    },
    {
      url: '/fonts',
      page: 'basic',
      section: 'appearance',
      subpage: ['appearance-fonts'],
    },
    {
      url: '/defaultBrowser',
      page: 'basic',
      section: 'defaultBrowser',
      subpage: [],
    },
    {
      url: '/search',
      page: 'basic',
      section: 'search',
      subpage: [],
    },
    {
      url: '/searchEngines',
      page: 'basic',
      section: 'search',
      subpage: ['search-engines'],
    },
    {
      url: '/onStartup',
      page: 'basic',
      section: 'onStartup',
      subpage: [],
    },
    {
      url: '/people',
      page: 'basic',
      section: 'people',
      subpage: [],
    },
<if expr="chromeos">
    {
      url: '/changePicture',
      page: 'basic',
      section: 'people',
      subpage: ['changePicture'],
    },
</if>
<if expr="not chromeos">
    {
      url: '/manageProfile',
      page: 'basic',
      section: 'people',
      subpage: ['manageProfile'],
    },
</if>
    {
      url: '/syncSetup',
      page: 'basic',
      section: 'people',
      subpage: ['sync'],
    },
<if expr="chromeos">
    {
      url: '/quickUnlock/authenticate',
      page: 'basic',
      section: 'people',
      subpage: ['quick-unlock-authenticate'],
    },
    {
      url: '/accounts',
      page: 'basic',
      section: 'people',
      subpage: ['users'],
    },
</if>
    {
      url: '/advanced',
      page: 'advanced',
      section: 'privacy',
      subpage: [],
    },
    {
      url: '/certificates',
      page: 'advanced',
      section: 'privacy',
      subpage: ['manage-certificates'],
    },
    {
      url: '/siteSettings',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings'],
    },
    // Site Category routes.
    {
      url: '/siteSettings/all',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'all-sites'],
    },
    {
      url: '/siteSettings/automaticDownloads',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-automatic-downloads'],
    },
    {
      url: '/siteSettings/backgroundSync',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-background-sync'],
    },
    {
      url: '/siteSettings/camera',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-camera'],
    },
    {
      url: '/siteSettings/cookies',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-cookies'],
    },
    {
      url: '/siteSettings/fullscreen',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-fullscreen'],
    },
    {
      url: '/siteSettings/images',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-images'],
    },
    {
      url: '/siteSettings/keygen',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-keygen'],
    },
    {
      url: '/siteSettings/location',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location'],
    },
    {
      url: '/siteSettings/javascript',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-javascript'],
    },
    {
      url: '/siteSettings/microphone',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-microphone'],
    },
    {
      url: '/siteSettings/notifications',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-notifications'],
    },
    {
      url: '/siteSettings/plugins',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-plugins'],
    },
    {
      url: '/siteSettings/popups',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-popups'],
    },
    {
      url: '/siteSettings/unsandboxedPlugins',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-unsandboxed-plugins'],
    },
    // Site details routes.
    {
      url: '/siteSettings/all/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'all-sites', 'site-details'],
    },
    {
      url: '/siteSettings/automaticDownloads/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-automatic-downloads',
          'site-details'],
    },
    {
      url: '/siteSettings/backgroundSync/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-background-sync',
          'site-details'],
    },
    {
      url: '/siteSettings/camera/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-camera',
          'site-details'],
    },
    {
      url: '/siteSettings/cookies/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-cookies',
          'site-details'],
    },
    {
      url: '/siteSettings/fullscreen/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-fullscreen',
          'site-details'],
    },
    {
      url: '/siteSettings/images/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-images',
          'site-details'],
    },
    {
      url: '/siteSettings/keygen/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-keygen',
          'site-details'],
    },
    {
      url: '/siteSettings/location/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location',
          'site-details'],
    },
    {
      url: '/siteSettings/javascript/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-javascript',
          'site-details'],
    },
    {
      url: '/siteSettings/microphone/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-microphone',
          'site-details'],
    },
    {
      url: '/siteSettings/notifications/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-notifications',
          'site-details'],
    },
    {
      url: '/siteSettings/plugins/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-plugins',
          'site-details'],
    },
    {
      url: '/siteSettings/popups/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-popups',
          'site-details'],
    },
    {
      url: '/siteSettings/unsandboxedPlugins/details',
      page: 'advanced',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-unsandsboxed-plugins',
          'site-details'],
    },
    {
      url: '/clearBrowserData',
      page: 'advanced',
      section: 'privacy',
      subpage: [],
      dialog: 'clear-browsing-data',
    },
<if expr="chromeos">
    {
      url: '/dateTime',
      page: 'advanced',
      section: 'dateTime',
      subpage: [],
    },
    {
      url: '/bluetooth',
      page: 'advanced',
      section: 'bluetooth',
      subpage: [],
    },
    {
      url: '/bluetoothAddDevice',
      page: 'advanced',
      section: 'bluetooth',
      subpage: ['bluetooth-add-device'],
    },
    {
      url: '/bluetoothAddDevice/bluetoothPairDevice',
      page: 'advanced',
      section: 'bluetooth',
      subpage: ['bluetooth-add-device', 'bluetooth-pair-device'],
    },
</if>
    {
      url: '/autofill',
      page: 'advanced',
      section: 'passwordsAndForms',
      subpage: ['manage-autofill'],
    },
    {
      url: '/passwords',
      page: 'advanced',
      section: 'passwordsAndForms',
      subpage: [],
    },
    {
      url: '/managePasswords',
      page: 'advanced',
      section: 'passwordsAndForms',
      subpage: ['manage-passwords'],
    },
    {
      url: '/languages',
      page: 'advanced',
      section: 'languages',
      subpage: [],
    },
    {
      url: '/manageLanguages',
      page: 'advanced',
      section: 'languages',
      subpage: ['manage-languages'],
    },
    {
      url: '/languages/edit',
      page: 'advanced',
      section: 'languages',
      subpage: ['language-detail'],
    },
<if expr="chromeos">
    {
      url: '/inputMethods',
      page: 'advanced',
      section: 'languages',
      subpage: ['manage-input-methods'],
    },
</if>
<if expr="not is_macosx">
    {
      url: '/editDictionary',
      page: 'advanced',
      section: 'languages',
      subpage: ['edit-dictionary'],
    },
</if>
    {
      url: '/downloadsDirectory',
      page: 'advanced',
      section: 'downloads',
      subpage: [],
    },
    {
      url: '/printing',
      page: 'advanced',
      section: 'printing',
      subpage: [],
    },
    {
      url: '/accessibility',
      page: 'advanced',
      section: 'a11y',
      subpage: [],
    },
    {
      url: '/system',
      page: 'advanced',
      section: 'system',
      subpage: [],
    },
    {
      url: '/reset',
      page: 'advanced',
      section: 'reset',
      subpage: [],
    },
<if expr="chromeos">
    {
      url: '/device',
      page: 'basic',
      section: 'device',
      subpage: [],
    },
    {
      url: '/pointer-overlay',
      page: 'basic',
      section: 'device',
      subpage: ['touchpad'],
    },
    {
      url: '/keyboard-overlay',
      page: 'basic',
      section: 'device',
      subpage: ['keyboard'],
    },
    {
      url: '/display',
      page: 'basic',
      section: 'device',
      subpage: ['display'],
    },
</if>
  ],

  /**
   * Sets up a history popstate observer.
   */
  created: function() {
    window.addEventListener('popstate', function(event) {
      if (event.state && event.state.page)
        this.currentRoute = event.state;
    }.bind(this));
  },

  /**
   * Is called when another element modifies the route. This observer validates
   * the route change against the pre-defined list of routes, and updates the
   * URL appropriately.
   * @param {!SettingsRoute} newRoute Where we're headed.
   * @param {!SettingsRoute|undefined} oldRoute Where we've been.
   * @private
   */
  currentRouteChanged_: function(newRoute, oldRoute) {
    for (var i = 0; i < this.routes_.length; ++i) {
      var route = this.routes_[i];
      if (route.page == newRoute.page &&
          route.section == newRoute.section &&
          route.dialog == newRoute.dialog &&
          route.subpage.length == newRoute.subpage.length &&
          newRoute.subpage.every(function(value, index) {
            return value == route.subpage[index];
          })) {
        // Update the property containing the titles for the current route.
        this.currentRouteTitles = {
          pageTitle: loadTimeData.getString(route.page + 'PageTitle'),
        };

        // If we are restoring a state from history, don't push it again.
        if (newRoute.inHistory)
          return;

        // Mark routes persisted in history as already stored in history.
        var historicState = {
          inHistory: true,
          page: newRoute.page,
          section: newRoute.section,
          subpage: newRoute.subpage,
          dialog: newRoute.dialog,
        };

        // Push the current route to the history state, so when the user
        // navigates with the browser back button, we can recall the route.
        if (oldRoute) {
          window.history.pushState(historicState, document.title, route.url);
        } else {
          // For the very first route (oldRoute will be undefined), we replace
          // the existing state instead of pushing a new one. This is to allow
          // the user to use the browser back button to exit Settings entirely.
          window.history.replaceState(historicState, document.title);
        }

        return;
      }
    }

    assertNotReached('Route not found: ' + JSON.stringify(newRoute));
  },
});
