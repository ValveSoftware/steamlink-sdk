// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the Import Data dialog to allow
 * users to import data (like bookmarks) from other web browsers.
 */
cr.exportPath('settings');

/**
 * An object describing a source browser profile that may be imported.
 * The structure of this data must be kept in sync with C++ ImportDataHandler.
 * @typedef {{
 *   name: string,
 *   index: number,
 *   history: boolean,
 *   favorites: boolean,
 *   passwords: boolean,
 *   search: boolean,
 *   autofillFormData: boolean,
 * }}
 */
settings.BrowserProfile;

/**
 * @enum {string}
 * These string values must be kept in sync with the C++ ImportDataHandler.
 */
settings.ImportDataStatus = {
  INITIAL: 'initial',
  IN_PROGRESS: 'inProgress',
  SUCCEEDED: 'succeeded',
  FAILED: 'failed',
};

cr.define('settings', function() {
  /** @interface */
  function ImportDataBrowserProxy() {}

  ImportDataBrowserProxy.prototype = {
    /**
     * Returns the source profiles available for importing from other browsers.
     * @return {!Promise<!Array<!settings.BrowserProfile>>}
     */
    initializeImportDialog: function() {},

    /**
     * Starts importing data for the specificed source browser profile. The C++
     * responds with the 'import-data-status-changed' WebUIListener event.
     * @param {number} sourceBrowserProfileIndex
     */
    importData: function(sourceBrowserProfileIndex) {},

    /**
     * Prompts the user to choose a bookmarks file to import bookmarks from.
     */
    importFromBookmarksFile: function() {},
  };

  /**
   * @constructor
   * @implements {settings.ImportDataBrowserProxy}
   */
  function ImportDataBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ImportDataBrowserProxyImpl);

  ImportDataBrowserProxyImpl.prototype = {
    /** @override */
    initializeImportDialog: function() {
      return cr.sendWithPromise('initializeImportDialog');
    },

    /** @override */
    importData: function(sourceBrowserProfileIndex) {
      chrome.send('importData', [sourceBrowserProfileIndex]);
    },

    /** @override */
    importFromBookmarksFile: function() {
      chrome.send('importFromBookmarksFile');
    },
  };

  return {
    ImportDataBrowserProxy: ImportDataBrowserProxy,
    ImportDataBrowserProxyImpl: ImportDataBrowserProxyImpl,
  };
});
