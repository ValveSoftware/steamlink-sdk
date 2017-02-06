// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

define('main', [
    'mojo/public/js/connection',
    'chrome/browser/ui/webui/engagement/site_engagement.mojom',
    'content/public/renderer/frame_service_registry',
], function(connection, siteEngagementMojom, serviceProvider) {
  return function() {
    var uiHandler = connection.bindHandleToProxy(
        serviceProvider.connectToService(
            siteEngagementMojom.SiteEngagementUIHandler.name),
        siteEngagementMojom.SiteEngagementUIHandler);

    var engagementTableBody = $('engagement-table-body');
    var updateInterval = null;
    var info = null;
    var sortKey = 'score';
    var sortReverse = true;

    // Set table header sort handlers.
    var engagementTableHeader = $('engagement-table-header');
    var headers = engagementTableHeader.children;
    for (var i = 0; i < headers.length; i++) {
      headers[i].addEventListener('click', function(e) {
        var newSortKey = e.target.getAttribute('sort-key');
        if (sortKey == newSortKey) {
          sortReverse = !sortReverse;
        } else {
          sortKey = newSortKey;
          sortReverse = false;
        }
        var oldSortColumn = document.querySelector('.sort-column');
        oldSortColumn.classList.remove('sort-column');
        e.target.classList.add('sort-column');
        if (sortReverse)
          e.target.setAttribute('sort-reverse', '');
        else
          e.target.removeAttribute('sort-reverse');
        renderTable();
      });
    }

    /**
     * Creates a single row in the engagement table.
     * @param {SiteEngagementInfo} info The info to create the row from.
     * @return {HTMLElement}
     */
    function createRow(info) {
      var originCell = createElementWithClassName('td', 'origin-cell');
      originCell.textContent = info.origin.url;

      var scoreInput = createElementWithClassName('input', 'score-input');
      scoreInput.addEventListener(
          'change', handleScoreChange.bind(undefined, info.origin));
      scoreInput.addEventListener('focus', disableAutoupdate);
      scoreInput.addEventListener('blur', enableAutoupdate);
      scoreInput.value = info.score;

      var scoreCell = createElementWithClassName('td', 'score-cell');
      scoreCell.appendChild(scoreInput);

      var engagementBar = createElementWithClassName('div', 'engagement-bar');
      engagementBar.style.width = (info.score * 4) + 'px';

      var engagementBarCell =
          createElementWithClassName('td', 'engagement-bar-cell');
      engagementBarCell.appendChild(engagementBar);

      var row = document.createElement('tr');
      row.appendChild(originCell);
      row.appendChild(scoreCell);
      row.appendChild(engagementBarCell);
      return row;
    }

    function disableAutoupdate() {
      if (updateInterval)
        clearInterval(updateInterval);
      updateInterval = null;
    }

    function enableAutoupdate() {
      if (updateInterval)
        clearInterval(updateInterval);
      updateInterval = setInterval(updateEngagementTable, 5000);
    }

    /**
     * Sets the engagement score when a score input is changed. Also resets the
     * update interval.
     * @param {string} origin The origin of the engagement score to set.
     * @param {Event} e
     */
    function handleScoreChange(origin, e) {
      uiHandler.setSiteEngagementScoreForOrigin(origin, e.target.value);
      enableAutoupdate();
    }

    /**
     * Remove all rows from the engagement table.
     */
    function clearTable() {
      engagementTableBody.innerHTML = '';
    }

    /**
     * Sort the engagement info based on |sortKey| and |sortReverse|.
     */
    function sortInfo() {
      info.sort(function(a, b) {
        return (sortReverse ? -1 : 1) *
               compareTableItem(sortKey, a, b);
      });
    }

    /**
     * Compares two SiteEngagementInfo objects based on |sortKey|.
     * @param {string} sortKey The name of the property to sort by.
     * @return {number} A negative number if |a| should be ordered before |b|, a
     * positive number otherwise.
     */
    function compareTableItem(sortKey, a, b) {
      var val1 = a[sortKey];
      var val2 = b[sortKey];

      // Compare the hosts of the origin ignoring schemes.
      if (sortKey == 'origin')
        return new URL(val1.url).host > new URL(val2.url).host ? 1 : -1;

      if (sortKey == 'score')
        return val1 - val2;

      assertNotReached('Unsupported sort key: ' + sortKey);
      return 0;
    }

    /**
     * Regenerates the engagement table from |info|.
     */
    function renderTable() {
      clearTable();
      sortInfo();
      // Round each score to 2 decimal places.
      info.forEach(function(info) {
        info.score = Number(Math.round(info.score * 100) / 100);
        engagementTableBody.appendChild(createRow(info));
      });
    }

    /**
     * Retrieve site engagement info and render the engagement table.
     */
    function updateEngagementTable() {
      // Populate engagement table.
      uiHandler.getSiteEngagementInfo().then(function(response) {
        info = response.info;
        renderTable(info);
      });
    };

    updateEngagementTable();
    enableAutoupdate();
  };
});
