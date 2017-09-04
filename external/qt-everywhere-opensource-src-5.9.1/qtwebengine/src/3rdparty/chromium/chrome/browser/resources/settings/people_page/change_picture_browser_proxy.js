// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Change Picture" subpage of
 * the People section to interact with the browser. ChromeOS only.
 */
cr.exportPath('settings');

/**
 * An object describing a default image.
 * @typedef {{
 *   author: string,
 *   title: string,
 *   url: string,
 *   website: string
 * }}
 */
settings.DefaultImage;

cr.define('settings', function() {
  /** @interface */
  function ChangePictureBrowserProxy() {}

  ChangePictureBrowserProxy.prototype = {
    /**
     * Retrieves the initial set of default images, profile image, etc. As a
     * response, the C++ sends these WebUIListener events:
     * 'default-images-changed', 'profile-image-changed', 'old-image-changed',
     * and 'selected-image-changed'
     */
    initialize: function() {},

    /**
     * Sets the user image to one of the default images. As a response, the C++
     * sends the 'default-images-changed' WebUIListener event.
     * @param {string} imageUrl
     */
    selectDefaultImage: function(imageUrl) {},

    /**
     * Sets the user image to the 'old' image. As a response, the C++ sends the
     * 'old-image-changed' WebUIListener event.
     */
    selectOldImage: function() {},

    /**
     * Sets the user image to the profile image. As a response, the C++ sends
     * the 'profile-image-changed' WebUIListener event.
     */
    selectProfileImage: function() {},

    /**
     * Provides the taken photo as a data URL to the C++. No response is
     * expected.
     * @param {string} photoDataUrl
     */
    photoTaken: function(photoDataUrl) {},

    /**
     * Requests a file chooser to select a new user image. No response is
     * expected.
     */
    chooseFile: function() {},
  };

  /**
   * @constructor
   * @implements {settings.ChangePictureBrowserProxy}
   */
  function ChangePictureBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ChangePictureBrowserProxyImpl);

  ChangePictureBrowserProxyImpl.prototype = {
    /** @override */
    initialize: function() {
      chrome.send('onChangePicturePageInitialized');
    },

    /** @override */
    selectDefaultImage: function(imageUrl) {
      chrome.send('selectImage', [imageUrl, 'default']);
    },

    /** @override */
    selectOldImage: function() {
      chrome.send('selectImage', ['', 'old']);
    },

    /** @override */
    selectProfileImage: function() {
      chrome.send('selectImage', ['', 'profile']);
    },

    /** @override */
    photoTaken: function(photoDataUrl) {
      chrome.send('photoTaken', [photoDataUrl]);
    },

    /** @override */
    chooseFile: function() {
      chrome.send('chooseFile');
    },
  };

  return {
    ChangePictureBrowserProxy: ChangePictureBrowserProxy,
    ChangePictureBrowserProxyImpl: ChangePictureBrowserProxyImpl,
  };
});
