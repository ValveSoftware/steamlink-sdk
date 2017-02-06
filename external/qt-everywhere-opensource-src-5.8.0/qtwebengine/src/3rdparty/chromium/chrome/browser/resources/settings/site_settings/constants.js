// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * All possible contentSettingsTypes that we currently support configuring in
   * the UI. Both top-level categories and content settings that represent
   * individual permissions under Site Details should appear here. This is a
   * subset of the constants found in site_settings_helper.cc and the values
   * should be kept in sync.
   * @enum {string}
   */
  var ContentSettingsTypes = {
    COOKIES: 'cookies',
    IMAGES: 'images',
    JAVASCRIPT: 'javascript',
    PLUGINS: 'plugins',
    POPUPS: 'popups',
    GEOLOCATION: 'location',
    NOTIFICATIONS: 'notifications',
    FULLSCREEN: 'fullscreen',
    MIC: 'media-stream-mic',
    CAMERA: 'media-stream-camera',
    UNSANDBOXED_PLUGINS: 'ppapi-broker',
    AUTOMATIC_DOWNLOADS: 'multiple-automatic-downloads',
    KEYGEN: 'keygen',
    BACKGROUND_SYNC: 'background-sync',
  };

  /**
   * Contains the possible string values for a given contentSettingsType.
   * @enum {string}
   */
  var PermissionValues = {
    DEFAULT: 'default',
    ALLOW: 'allow',
    BLOCK: 'block',
    ASK: 'ask',
    SESSION_ONLY: 'session_only',
    IMPORTANT_CONTENT: 'detect_important_content',
  };

  /**
   * A category value to use for the All Sites list.
   * @const {string}
   */
  var ALL_SITES = 'all-sites';

  /**
   * An invalid subtype value.
   * @const {string}
   */
  var INVALID_CATEGORY_SUBTYPE = '';

  return {
    ContentSettingsTypes: ContentSettingsTypes,
    PermissionValues: PermissionValues,
    ALL_SITES: ALL_SITES,
    INVALID_CATEGORY_SUBTYPE: INVALID_CATEGORY_SUBTYPE,
  };
});
