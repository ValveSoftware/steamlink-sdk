// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

/** @polymerBehavior */
var SiteSettingsBehaviorImpl = {
  properties: {
    /**
     * The string ID of the category this element is displaying data for.
     * See site_settings/constants.js for possible values.
     */
    category: String,

    /**
     * The browser proxy used to retrieve and change information about site
     * settings categories and the sites within.
     * @type {settings.SiteSettingsPrefsBrowserProxy}
     */
    browserProxy: Object,
  },

  created: function() {
    this.browserProxy =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  ready: function() {
    this.PermissionValues = settings.PermissionValues;
  },

  /**
   * Re-sets the category permission for a given origin.
   * @param {string} primaryPattern The primary pattern to reset the permission
   *     for.
   * @param {string} secondaryPattern The secondary pattern to reset the
   *     permission for.
   * @param {string} category The category permission to change.
   * @protected
   */
  resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, category) {
    this.browserProxy.resetCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, category);
  },

  /**
   * Sets the category permission for a given origin.
   * @param {string} primaryPattern The primary pattern to change the permission
   *     for.
   * @param {string} secondaryPattern The secondary pattern to change the
   *     permission for.
   * @param {string} category The category permission to change.
   * @param {string} value What value to set the permission to.
   * @protected
   */
  setCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, category, value) {
    this.browserProxy.setCategoryPermissionForOrigin(
        primaryPattern, secondaryPattern, category, value);
  },

  /**
   * A utility function to lookup a category name from its enum. Note: The
   * category name is visible to the user as part of the URL.
   * @param {string} category The category ID to look up.
   * @return {string} The category found or blank string if not found.
   * @protected
   */
  computeCategoryTextId: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
        return 'automatic-downloads';
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
        return 'background-sync';
      case settings.ContentSettingsTypes.CAMERA:
        return 'camera';
      case settings.ContentSettingsTypes.COOKIES:
        return 'cookies';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'location';
      case settings.ContentSettingsTypes.IMAGES:
        return 'images';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'javascript';
      case settings.ContentSettingsTypes.KEYGEN:
        return 'keygen';
      case settings.ContentSettingsTypes.MIC:
        return 'microphone';
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return 'notifications';
      case settings.ContentSettingsTypes.PLUGINS:
        return 'plugins';
      case settings.ContentSettingsTypes.POPUPS:
        return 'popups';
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        return 'unsandboxed-plugins';
      default:
        return '';
    }
  },

  /**
   * A utility function to compute the icon to use for the category, both for
   * the overall category as well as the individual permission in the details
   * for a site.
   * @param {string} category The category to show the icon for.
   * @return {string} The id of the icon for the given category.
   * @protected
   */
  computeIconForContentCategory: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
        return 'cr:file-download';
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
        return 'settings:sync';
      case settings.ContentSettingsTypes.CAMERA:
        return 'settings:videocam';
      case settings.ContentSettingsTypes.COOKIES:
        return 'settings:cookie';
      case settings.ContentSettingsTypes.FULLSCREEN:
        return 'cr:fullscreen';
      case settings.ContentSettingsTypes.GEOLOCATION:
        return 'settings:location-on';
      case settings.ContentSettingsTypes.IMAGES:
        return 'settings:photo';
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return 'settings:input';
      case settings.ContentSettingsTypes.KEYGEN:
        return 'settings:code';
      case settings.ContentSettingsTypes.MIC:
        return 'settings:mic';
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return 'settings:notifications';
      case settings.ContentSettingsTypes.PLUGINS:
        return 'cr:extension';
      case settings.ContentSettingsTypes.POPUPS:
        return 'settings:open-in-new';
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        return 'cr:extension';
      default:
        assertNotReached('Invalid category: ' + category);
        return '';
    }
  },

  /**
   * A utility function to compute the title of the category, both for
   * the overall category as well as the individual permission in the details
   * for a site.
   * @param {string} category The category to show the title for.
   * @return {string} The title for the given category.
   * @protected
   */
  computeTitleForContentCategory: function(category) {
    switch (category) {
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
        return loadTimeData.getString('siteSettingsAutomaticDownloads');
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
        return loadTimeData.getString('siteSettingsBackgroundSync');
      case settings.ContentSettingsTypes.CAMERA:
        return loadTimeData.getString('siteSettingsCamera');
      case settings.ContentSettingsTypes.COOKIES:
        return loadTimeData.getString('siteSettingsCookies');
      case settings.ContentSettingsTypes.FULLSCREEN:
        return loadTimeData.getString('siteSettingsFullscreen');
      case settings.ContentSettingsTypes.GEOLOCATION:
        return loadTimeData.getString('siteSettingsLocation');
      case settings.ContentSettingsTypes.IMAGES:
        return loadTimeData.getString('siteSettingsImages');
      case settings.ContentSettingsTypes.JAVASCRIPT:
        return loadTimeData.getString('siteSettingsJavascript');
      case settings.ContentSettingsTypes.KEYGEN:
        return loadTimeData.getString('siteSettingsKeygen');
      case settings.ContentSettingsTypes.MIC:
        return loadTimeData.getString('siteSettingsMic');
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        return loadTimeData.getString('siteSettingsNotifications');
      case settings.ContentSettingsTypes.PLUGINS:
        return loadTimeData.getString('siteSettingsPlugins');
      case settings.ContentSettingsTypes.POPUPS:
        return loadTimeData.getString('siteSettingsPopups');
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        return loadTimeData.getString('siteSettingsUnsandboxedPlugins');
      default:
        assertNotReached('Invalid category: ' + category);
        return '';
    }
  },

  /**
   * A utility function to compute the description for the category.
   * @param {string} category The category to show the description for.
   * @param {boolean} categoryEnabled The state of the global toggle.
   * @param {boolean} showRecommendation Whether to show the '(recommended)'
   *     label prefix.
   * @return {string} The category description.
   * @protected
   */
  computeCategoryDesc: function(category, categoryEnabled, showRecommendation) {
    switch (category) {
      case settings.ContentSettingsTypes.JAVASCRIPT:
        // "Allowed (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAllowedRecommended') :
            loadTimeData.getString('siteSettingsAllowed');
      case settings.ContentSettingsTypes.POPUPS:
        // "Allowed" vs "Blocked (recommended)".
        if (categoryEnabled) {
          return loadTimeData.getString('siteSettingsAllowed');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsBlockedRecommended') :
            loadTimeData.getString('siteSettingsBlocked');
      case settings.ContentSettingsTypes.NOTIFICATIONS:
        // "Ask before sending (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAskBeforeSendingRecommended') :
            loadTimeData.getString('siteSettingsAskBeforeSending');
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.MIC:
        // "Ask before accessing (recommended)" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString(
                'siteSettingsAskBeforeAccessingRecommended') :
            loadTimeData.getString('siteSettingsAskBeforeAccessing');
      case settings.ContentSettingsTypes.COOKIES:
        // "Allow sites to save and read cookie data" vs "Blocked".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsCookiesAllowedRecommended') :
            loadTimeData.getString('siteSettingsCookiesAllowed');
      case settings.ContentSettingsTypes.IMAGES:
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsDontShowImages');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsShowAllRecommended') :
            loadTimeData.getString('siteSettingsShowAll');
      case settings.ContentSettingsTypes.PLUGINS:
        // "Detect and run important content (recommended)" vs "Let me choose".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsLetMeChoose');
        }
        return showRecommendation ?
            loadTimeData.getString(
                 'siteSettingsDetectAndRunImportantRecommended') :
            loadTimeData.getString('siteSettingsDetectAndRunImportant');
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
        // "Allow sites to finish sending and receiving data" vs "Do not allow".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsBackgroundSyncBlocked');
        }
        return showRecommendation ?
            loadTimeData.getString(
                 'siteSettingsAllowRecentlyClosedSitesRecommended') :
            loadTimeData.getString('siteSettingsAllowRecentlyClosedSites');
      case settings.ContentSettingsTypes.KEYGEN:
        // "Allow sites to use keygen" vs "Do not allow".
        if (categoryEnabled) {
          return loadTimeData.getString('siteSettingsKeygenAllow');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsKeygenBlockRecommended') :
            loadTimeData.getString('siteSettingsKeygenBlock');
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
        // "Ask when a site wants to auto-download multiple" vs "Do not allow".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsAutoDownloadBlock');
        }
        return showRecommendation ?
            loadTimeData.getString('siteSettingsAutoDownloadAskRecommended') :
            loadTimeData.getString('siteSettingsAutoDownloadAsk');
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
        // "Ask when a plugin accesses your computer" vs "Do not allow".
        if (!categoryEnabled) {
          return loadTimeData.getString('siteSettingsUnsandboxedPluginsBlock');
        }
        return showRecommendation ?
            loadTimeData.getString(
                'siteSettingsUnsandboxedPluginsAskRecommended') :
            loadTimeData.getString('siteSettingsUnsandboxedPluginsAsk');
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   */
  ensureUrlHasScheme: function(url) {
    if (url.length == 0) return url;
    return url.includes('://') ? url : 'http://' + url;
  },

  /**
   * Removes redundant ports, such as port 80 for http and 443 for https.
   * @param {string} url The URL to sanitize.
   * @return {string} The URL without redundant ports, if any.
   */
  sanitizePort: function(url) {
    var urlWithScheme = this.ensureUrlHasScheme(url);
    if (urlWithScheme.startsWith('https://') &&
        urlWithScheme.endsWith(':443')) {
      return url.slice(0, -4);
    }
    if (urlWithScheme.startsWith('http://') &&
        urlWithScheme.endsWith(':80')) {
      return url.slice(0, -3);
    }
    return url;
  },

  /**
   * Adds the wildcard prefix to a pattern string.
   * @param {string} pattern The pattern to add the wildcard to.
   * @return {string} The resulting pattern.
   * @private
   */
  addPatternWildcard_: function(pattern) {
    if (pattern.startsWith('http://'))
      return pattern.replace('http://', 'http://[*.]');
    else if (pattern.startsWith('https://'))
      return pattern.replace('https://', 'https://[*.]');
    else
      return '[*.]' + pattern;
  },

  /**
   * Removes the wildcard prefix from a pattern string.
   * @param {string} pattern The pattern to remove the wildcard from.
   * @return {string} The resulting pattern.
   * @private
   */
  removePatternWildcard_: function(pattern) {
    if (pattern.startsWith('http://[*.]'))
      return pattern.replace('http://[*.]', 'http://');
    else if (pattern.startsWith('https://[*.]'))
      return pattern.replace('https://[*.]', 'https://');
    else if (pattern.startsWith('[*.]'))
      return pattern.substring(4, pattern.length);
    return pattern;
  },

  /**
   * Returns the icon to use for a given site.
   * @param {SiteException} site The url of the site to fetch the icon for.
   * @return {string} The background-image style with the favicon.
   * @private
   */
  computeSiteIcon: function(site) {
    var url = this.ensureUrlHasScheme(site.originForDisplay);
    return 'background-image: ' + cr.icon.getFaviconImageSet(url);
  },
};

/** @polymerBehavior */
var SiteSettingsBehavior = [SiteSettingsBehaviorImpl];
