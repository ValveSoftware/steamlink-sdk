// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * All possible contentSettingsTypes that we currently support configuring in
 * the UI. Both top-level categories and content settings that represent
 * individual permissions under Site Details should appear here. This is a
 * subset of the constants found in site_settings_helper.cc and the values
 * should be kept in sync.
 * @enum {string}
 */
settings.ContentSettingsTypes = {
  COOKIES: 'cookies',
  IMAGES: 'images',
  JAVASCRIPT: 'javascript',
  PLUGINS: 'plugins',
  POPUPS: 'popups',
  GEOLOCATION: 'location',
  NOTIFICATIONS: 'notifications',
  MIC: 'media-stream-mic',
  CAMERA: 'media-stream-camera',
  PROTOCOL_HANDLERS: 'register-protocol-handler',
  UNSANDBOXED_PLUGINS: 'ppapi-broker',
  AUTOMATIC_DOWNLOADS: 'multiple-automatic-downloads',
  KEYGEN: 'keygen',
  BACKGROUND_SYNC: 'background-sync',
  USB_DEVICES: 'usb-chooser-data',
  ZOOM_LEVELS: 'zoom-levels',
};

/**
 * Contains the possible string values for a given contentSettingsType.
 * @enum {string}
 *
 * TODO(dschuyler): This should be rename as ContentSetting to maintain
 * nomenclature with C++.
 */
settings.PermissionValues = {
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
settings.ALL_SITES = 'all-sites';

/**
 * An invalid subtype value.
 * @const {string}
 */
settings.INVALID_CATEGORY_SUBTYPE = '';
