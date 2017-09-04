// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send the history query immediately. This allows the query to process during
// the initial page startup.
chrome.send('queryHistory', ['', 0, 0, 0, RESULTS_PER_PAGE]);
chrome.send('getForeignSessions');

/** @type {Promise} */
var upgradePromise = null;
/** @type {boolean} */
var resultsRendered = false;

/**
 * @return {!Promise} Resolves once the history-app has been fully upgraded.
 */
function waitForAppUpgrade() {
  if (!upgradePromise) {
    upgradePromise = new Promise(function(resolve, reject) {
      if (window.Polymer && Polymer.isInstance &&
          Polymer.isInstance($('history-app'))) {
        resolve();
      } else {
        $('bundle').addEventListener('load', resolve);
      }
    });
  }
  return upgradePromise;
}

// Chrome Callbacks-------------------------------------------------------------

/**
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {!Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  waitForAppUpgrade().then(function() {
    var app = /** @type {HistoryAppElement} */($('history-app'));
    app.historyResult(info, results);
    document.body.classList.remove('loading');

    if (!resultsRendered) {
      resultsRendered = true;
      app.onFirstRender();
    }
  });
}

/**
 * Called by the history backend after receiving results and after discovering
 * the existence of other forms of browsing history.
 * @param {boolean} hasSyncedResults Whether there are synced results.
 * @param {boolean} includeOtherFormsOfBrowsingHistory Whether to include
 *     a sentence about the existence of other forms of browsing history.
 */
function showNotification(
    hasSyncedResults, includeOtherFormsOfBrowsingHistory) {
  waitForAppUpgrade().then(function() {
    var app = /** @type {HistoryAppElement} */ ($('history-app'));
    app.showSidebarFooter = includeOtherFormsOfBrowsingHistory;
    app.hasSyncedResults = hasSyncedResults;
  });
}

/**
 * Receives the synced history data. An empty list means that either there are
 * no foreign sessions, or tab sync is disabled for this profile.
 * |isTabSyncEnabled| makes it possible to distinguish between the cases.
 *
 * @param {!Array<!ForeignSession>} sessionList Array of objects describing the
 *     sessions from other devices.
 * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
 */
function setForeignSessions(sessionList, isTabSyncEnabled) {
  waitForAppUpgrade().then(function() {
    /** @type {HistoryAppElement} */($('history-app'))
        .setForeignSessions(sessionList, isTabSyncEnabled);
  });
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
  waitForAppUpgrade().then(function() {
    /** @type {HistoryAppElement} */($('history-app'))
        .historyDeleted();
  });
}

/**
 * Called by the history backend after user's sign in state changes.
 * @param {boolean} isUserSignedIn Whether user is signed in or not now.
 */
function updateSignInState(isUserSignedIn) {
  waitForAppUpgrade().then(function() {
    if ($('history-app')) {
      /** @type {HistoryAppElement} */($('history-app'))
          .updateSignInState(isUserSignedIn);
    }
  });
}
