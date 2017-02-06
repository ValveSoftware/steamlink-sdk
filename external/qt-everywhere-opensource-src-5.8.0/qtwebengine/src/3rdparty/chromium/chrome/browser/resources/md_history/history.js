// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send the history query immediately. This allows the query to process during
// the initial page startup.
chrome.send('queryHistory', ['', 0, 0, 0, RESULTS_PER_PAGE]);
chrome.send('getForeignSessions');

/**
 * @param {HTMLElement} element
 * @return {!Promise} Resolves once a Polymer element has been fully upgraded.
 */
function waitForUpgrade(element) {
  return new Promise(function(resolve, reject) {
    if (window.Polymer && Polymer.isInstance && Polymer.isInstance(element))
      resolve();
    else
      $('bundle').addEventListener('load', resolve);
  });
}

// Chrome Callbacks-------------------------------------------------------------

/**
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {!Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  var appElem = $('history-app');
  waitForUpgrade(appElem).then(function() {
    /** @type {HistoryAppElement} */(appElem).historyResult(info, results);
    // TODO(tsergeant): Showing everything as soon as the list is ready is not
    // ideal, as the sidebar can still pop in after. Fix this to show everything
    // at once.
    document.body.classList.remove('loading');
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
  // TODO(msramek): Implement the joint notification about web history and other
  // forms of browsing history for the MD history page.
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
  var appElem = $('history-app');
  waitForUpgrade(appElem).then(function() {
    /** @type {HistoryAppElement} */(appElem)
        .setForeignSessions(sessionList, isTabSyncEnabled);
  });
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
}

/**
 * Called by the history backend after user's sign in state changes.
 * @param {boolean} isUserSignedIn Whether user is signed in or not now.
 */
function updateSignInState(isUserSignedIn) {
  var appElem = $('history-app');
  waitForUpgrade(appElem).then(function() {
    /** @type {HistoryAppElement} */(appElem)
        .updateSignInState(isUserSignedIn);
  });
}
