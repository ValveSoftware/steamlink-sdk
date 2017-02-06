// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('offlineInternals', function() {
  'use strict';

  /** @type {!Array<OfflinePage>} */
  var offlinePages = [];

  /** @type {!Array<SavePageRequest>} */
  var savePageRequests = [];

  /** @type {!offlineInternals.OfflineInternalsBrowserProxy} */
  var browserProxy_ =
      offlineInternals.OfflineInternalsBrowserProxyImpl.getInstance();

  /**
   * Fill stored pages table.
   * @param {!Array<OfflinePage>} pages An array object representing
   *     stored offline pages.
   */
  function fillStoredPages(pages) {
    var storedPagesTable = $('stored-pages');
    storedPagesTable.textContent = '';

    for (var i = 0; i < pages.length; i++) {
      var row = document.createElement('tr');

      var checkboxCell = document.createElement('td');
      var checkbox = document.createElement('input');
      checkbox.setAttribute('type', 'checkbox');
      checkbox.setAttribute('name', 'stored');
      checkbox.setAttribute('value', pages[i].id);

      checkboxCell.appendChild(checkbox);
      row.appendChild(checkboxCell);

      var cell = document.createElement('td');
      cell.textContent = pages[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].namespace;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = Math.round(pages[i].size / 1024);
      row.appendChild(cell);

      storedPagesTable.appendChild(row);
    }
    offlinePages = pages;
  }

  /**
   * Fill requests table.
   * @param {!Array<SavePageRequest>} requests An array object representing
   *     the request queue.
   */
  function fillRequestQueue(requests) {
    var requestQueueTable = $('request-queue');
    requestQueueTable.textContent = '';

    for (var i = 0; i < requests.length; i++) {
      var row = document.createElement('tr');

      var cell = document.createElement('td');
      cell.textContent = requests[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = new Date(requests[i].creationTime);
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = requests[i].status;
      row.appendChild(cell);

      requestQueueTable.appendChild(row);
    }
    savePageRequests = requests;
  }

  /**
   * Fills the event logs section.
   * @param {!Array<string>} logs A list of log strings.
   */
  function fillEventLog(logs) {
    var element = $('logs');
    element.textContent = '';
    for (let log of logs) {
      var logItem = document.createElement('li');
      logItem.textContent = log;
      element.appendChild(logItem);
    }
  }

  /**
   * Refresh all displayed information.
   */
  function refreshAll() {
    browserProxy_.getStoredPages().then(fillStoredPages);
    browserProxy_.getRequestQueue().then(fillRequestQueue);
    refreshLog();
  }

  /**
   * Delete all pages in the offline store.
   */
  function deleteAllPages() {
    browserProxy_.deleteAllPages().then(pagesDeleted);
  }

  /**
   * Callback when pages are deleted.
   * @param {string} status The status of the request.
   */
  function pagesDeleted(status) {
    $('page-actions-info').textContent = status;
    browserProxy_.getStoredPages().then(fillStoredPages);
  }

  /**
   * Downloads all the stored page and request queue information into a file.
   * TODO(chili): Create a CSV writer that can abstract out the line joining.
   */
  function download() {
    var json = JSON.stringify({
      offlinePages: offlinePages,
      savePageRequests: savePageRequests
    }, null, 2);

    window.open(
        'data:application/json,' + encodeURIComponent(json),
        'dump.json');
  }

  /**
   * Updates the status strings.
   * @param {!IsLogging} logStatus Status of logging.
   */
  function updateLogStatus(logStatus) {
    $('model-status').textContent = logStatus.modelIsLogging ? 'On' : 'Off';
    $('request-status').textContent = logStatus.queueIsLogging ? 'On' : 'Off';
  }

  /**
   * Delete selected pages from the offline store.
   */
  function deleteSelectedPages() {
    var checkboxes = document.getElementsByName('stored');
    var selectedIds = [];

    for (var i = 0; i < checkboxes.length; i++) {
      if (checkboxes[i].checked)
        selectedIds.push(checkboxes[i].value);
    }

    browserProxy_.deleteSelectedPages(selectedIds).then(pagesDeleted);
  }

  /**
   * Refreshes the logs.
   */
  function refreshLog() {
    browserProxy_.getEventLogs().then(fillEventLog);
    browserProxy_.getLoggingState().then(updateLogStatus);
  }

  function initialize() {
    $('clear-all').onclick = deleteAllPages;
    $('clear-selected').onclick = deleteSelectedPages;
    $('refresh').onclick = refreshAll;
    $('download').onclick = download;
    $('log-model-on').onclick =
        browserProxy_.setRecordPageModel.bind(browserProxy_, true);
    $('log-model-off').onclick =
        browserProxy_.setRecordPageModel.bind(browserProxy_, false);
    $('log-request-on').onclick =
        browserProxy_.setRecordRequestQueue.bind(browserProxy_, true);
    $('log-request-off').onclick =
        browserProxy_.setRecordRequestQueue.bind(browserProxy_, false);
    $('refresh-logs').onclick = refreshLog;
    refreshAll();
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', offlineInternals.initialize);
