// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Construct an ExtensionLoadError around the given |div|.
   * @param {HTMLDivElement} div The HTML div for the extension load error.
   * @constructor
   */
  function ExtensionLoadError(div) {
    div.__proto__ = ExtensionLoadError.prototype;
    div.init();
    return div;
  }

  ExtensionLoadError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialize the ExtensionLoadError div.
     */
    init: function() {
      /**
       * The element which displays the path of the extension.
       * @type {HTMLSpanElement}
       * @private
       */
      this.path_ = this.querySelector('#extension-load-error-path');

      /**
       * The element which displays the reason the extension failed to load.
       * @type {HTMLSpanElement}
       * @private
       */
      this.reason_ = this.querySelector('#extension-load-error-reason');

      /**
       * The element which displays the manifest code.
       * @type {ExtensionCode}
       * @private
       */
      this.manifest_ = new extensions.ExtensionCode(
          this.querySelector('#extension-load-error-manifest'));

      this.querySelector('#extension-load-error-retry-button').addEventListener(
          'click', function(e) {
        chrome.send('extensionLoaderRetry');
        this.hide_();
      }.bind(this));

      this.querySelector('#extension-load-error-give-up-button').
          addEventListener('click', function(e) {
        this.hide_();
      }.bind(this));
    },

    /**
     * Display the load error to the user.
     * @param {string} path The path from which the extension was loaded.
     * @param {string} reason The reason the extension failed to load.
     * @param {string} manifest The manifest object, with highlighted regions.
     */
    show: function(path, reason, manifest) {
      this.path_.textContent = path;
      this.reason_.textContent = reason;

      manifest.message = reason;
      this.manifest_.populate(
          manifest,
          loadTimeData.getString('extensionLoadCouldNotLoadManifest'));
      this.hidden = false;
      this.manifest_.scrollToError();
    },

    /**
     * Hide the extension load error.
     * @private
     */
    hide_: function() {
      this.hidden = true;
    }
  };

  /**
   * The ExtensionLoader is the class in charge of loading unpacked extensions.
   * @constructor
   */
  function ExtensionLoader() {
    /**
     * The ExtensionLoadError to show any errors from loading an unpacked
     * extension.
     * @type {ExtensionLoadError}
     * @private
     */
    this.loadError_ = new ExtensionLoadError($('extension-load-error'));
  }

  cr.addSingletonGetter(ExtensionLoader);

  ExtensionLoader.prototype = {
    /**
     * Begin the sequence of loading an unpacked extension. If an error is
     * encountered, this object will get notified via notifyFailed().
     */
    loadUnpacked: function() {
      chrome.send('extensionLoaderLoadUnpacked');
    },

    /**
     * Notify the ExtensionLoader that loading an unpacked extension failed.
     * Show the ExtensionLoadError.
     * @param {string} filePath The path to the unpacked extension.
     * @param {string} reason The reason the extension failed to load.
     * @param {Object} manifest An object with three strings: beforeHighlight,
     *     afterHighlight, and highlight. These represent three portions of the
     *     file's content to display - the portion which is most relevant and
     *     should be emphasized (highlight), and the parts both before and after
     *     this portion. These may be empty.
     */
    notifyFailed: function(filePath, reason, manifest) {
      this.loadError_.show(filePath, reason, manifest);
    }
  };

  /*
   * A static forwarding function for ExtensionLoader.notifyFailed.
   * @param {string} filePath The path to the unpacked extension.
   * @param {string} reason The reason the extension failed to load.
   * @param {Object} manifest The manifest of the failed extension.
   * @see ExtensionLoader.notifyFailed
   */
  ExtensionLoader.notifyLoadFailed = function(filePath, reason, manifest) {
    ExtensionLoader.getInstance().notifyFailed(filePath, reason, manifest);
  };

  return {
    ExtensionLoader: ExtensionLoader
  };
});
