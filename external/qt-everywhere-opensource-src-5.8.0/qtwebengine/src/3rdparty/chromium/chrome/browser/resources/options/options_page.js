// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var FocusOutlineManager = cr.ui.FocusOutlineManager;

  var OptionsPage = {
    /**
     * This is the absolute difference maintained between standard and
     * fixed-width font sizes. Refer http://crbug.com/91922.
     * @const
     */
    SIZE_DIFFERENCE_FIXED_STANDARD: 3,

    /**
     * Initializes the complete options page. This will cause all C++ handlers
     * to be invoked to do final setup.
     */
    initialize: function() {
      chrome.send('coreOptionsInitialize');
    },

    /**
     * Shows the tab contents for the given navigation tab.
     * @param {Node} tab The tab that the user clicked.
     */
    showTab: function(tab) {
      // Search parents until we find a tab, or the nav bar itself. This allows
      // tabs to have child nodes, e.g. labels in separately-styled spans.
      while (tab && tab.classList &&
             !tab.classList.contains('subpages-nav-tabs') &&
             !tab.classList.contains('tab')) {
        tab = tab.parentNode;
      }
      if (!tab || !tab.classList || !tab.classList.contains('tab'))
        return;

      // Find tab bar of the tab.
      var tabBar = tab;
      while (tabBar && tabBar.classList &&
             !tabBar.classList.contains('subpages-nav-tabs')) {
        tabBar = tabBar.parentNode;
      }
      if (!tabBar || !tabBar.classList)
        return;

      if (tabBar.activeNavTab != null) {
        tabBar.activeNavTab.classList.remove('active-tab');
        $(tabBar.activeNavTab.getAttribute('tab-contents')).classList.
            remove('active-tab-contents');
      }

      tab.classList.add('active-tab');
      $(tab.getAttribute('tab-contents')).classList.add('active-tab-contents');
      tabBar.activeNavTab = tab;
    },

    /**
     * Shows or hides options for clearing Flash LSOs.
     * @param {boolean} enabled Whether plugin data can be cleared.
     */
    setClearPluginLSODataEnabled: function(enabled) {
      if (enabled) {
        document.documentElement.setAttribute(
            'flashPluginSupportsClearSiteData', '');
      } else {
        document.documentElement.removeAttribute(
            'flashPluginSupportsClearSiteData');
      }
      if (navigator.plugins['Shockwave Flash'])
        document.documentElement.setAttribute('hasFlashPlugin', '');
    },

    /**
     * Shows or hides Pepper Flash settings.
     * @param {boolean} enabled Whether Pepper Flash settings should be enabled.
     */
    setPepperFlashSettingsEnabled: function(enabled) {
      if (enabled) {
        document.documentElement.setAttribute(
            'enablePepperFlashSettings', '');
      } else {
        document.documentElement.removeAttribute(
            'enablePepperFlashSettings');
      }
    },

    /**
     * Sets whether Settings is shown as a standalone page in a window for the
     * app launcher settings "app".
     * @param {boolean} isSettingsApp Whether this page is shown standalone.
     */
    setIsSettingsApp: function(isSettingsApp) {
      document.documentElement.classList.toggle('settings-app', isSettingsApp);
    },

    /**
     * Returns true if Settings is shown as an "app" (in a window by itself)
     * for the app launcher settings "app".
     * @return {boolean} Whether this page is shown standalone.
     */
    isSettingsApp: function() {
      return document.documentElement.classList.contains('settings-app');
    },
  };

  // Export
  return {
    OptionsPage: OptionsPage
  };
});
