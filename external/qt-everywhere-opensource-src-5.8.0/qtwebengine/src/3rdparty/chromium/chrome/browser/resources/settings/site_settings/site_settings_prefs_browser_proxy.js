// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

/**
 * @typedef {{embeddingOrigin: string,
 *            origin: string,
 *            originForDisplay: string,
 *            setting: string,
 *            source: string}}
 */
var SiteException;

/**
 * @typedef {{location: string,
 *            notifications: string}}
 */
var CategoryDefaultsPref;

/**
 * @typedef {{location: Array<SiteException>,
 *            notifications: Array<SiteException>}}
 */
var ExceptionListPref;

/**
 * @typedef {{defaults: CategoryDefaultsPref,
 *            exceptions: ExceptionListPref}}
 */
var SiteSettingsPref;

/**
 * @typedef {{name: string,
 *            id: string}}
 */
var MediaPickerEntry;

cr.define('settings', function() {
  /** @interface */
  function SiteSettingsPrefsBrowserProxy() {}

  SiteSettingsPrefsBrowserProxy.prototype = {
    /**
     * Sets the default value for a site settings category.
     * @param {string} contentType The name of the category to change.
     * @param {string} defaultValue The name of the value to set as default.
     */
    setDefaultValueForContentType: function(contentType, defaultValue) {},

    /**
     * Gets the default value for a site settings category.
     * @param {string} contentType The name of the category to query.
     * @return {Promise<boolean>}
     */
    getDefaultValueForContentType: function(contentType) {},

    /**
     * Gets the exceptions (site list) for a particular category.
     * @param {string} contentType The name of the category to query.
     * @return {Promise<Array<SiteException>>}
     */
    getExceptionList: function(contentType) {},

    /**
     * Resets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to reset.
     */
    resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType) {},

    /**
     * Sets the category permission for a given origin (expressed as primary
     *    and secondary patterns).
     * @param {string} primaryPattern The origin to change (primary pattern).
     * @param {string} secondaryPattern The embedding origin to change
     *    (secondary pattern).
     * @param {string} contentType The name of the category to change.
     * @param {string} value The value to change the permission to.
     */
    setCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType, value) {},

    /**
     * Checks whether a pattern is valid.
     * @param {string} pattern The pattern to check
     * @return {!Promise<boolean>} True if the pattern is valid.
     */
    isPatternValid: function(pattern) {},

    /**
     * Gets the list of default capture devices for a given type of media. List
     * is returned through a JS call to updateDevicesMenu.
     * @param {string} type The type to look up.
     */
    getDefaultCaptureDevices: function(type) {},

    /**
     * Sets a default devices for a given type of media.
     * @param {string} type The type of media to configure.
     * @param {string} defaultValue The id of the media device to set.
     */
    setDefaultCaptureDevice: function(type, defaultValue) {},
  };

  /**
   * @constructor
   * @implements {settings.SiteSettingsPrefsBrowserProxy}
   */
  function SiteSettingsPrefsBrowserProxyImpl() {}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SiteSettingsPrefsBrowserProxyImpl);

  SiteSettingsPrefsBrowserProxyImpl.prototype = {
    /** @override */
    setDefaultValueForContentType: function(contentType, defaultValue) {
      chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
    },

    /** @override */
    getDefaultValueForContentType: function(contentType) {
      return cr.sendWithPromise('getDefaultValueForContentType', contentType);
    },

    /** @override */
    getExceptionList: function(contentType) {
      return cr.sendWithPromise('getExceptionList', contentType);
    },

    /** @override */
    resetCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType) {
      chrome.send('resetCategoryPermissionForOrigin',
          [primaryPattern, secondaryPattern, contentType]);
    },

    /** @override */
    setCategoryPermissionForOrigin: function(
        primaryPattern, secondaryPattern, contentType, value) {
      chrome.send('setCategoryPermissionForOrigin',
          [primaryPattern, secondaryPattern, contentType, value]);
    },

    /** @override */
    isPatternValid: function(pattern) {
      return cr.sendWithPromise('isPatternValid', pattern);
    },

    /** @override */
    getDefaultCaptureDevices: function(type) {
      chrome.send('getDefaultCaptureDevices', [type]);
    },

    /** @override */
    setDefaultCaptureDevice: function(type, defaultValue) {
      chrome.send('setDefaultCaptureDevice', [type, defaultValue]);
    },
  };

  return {
    SiteSettingsPrefsBrowserProxy: SiteSettingsPrefsBrowserProxy,
    SiteSettingsPrefsBrowserProxyImpl: SiteSettingsPrefsBrowserProxyImpl,
  };
});
