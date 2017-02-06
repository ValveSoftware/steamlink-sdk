// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * The data of a peer connection update.
 * @param {number} pid The id of the renderer.
 * @param {number} lid The id of the peer conneciton inside a renderer.
 * @param {string} type The type of the update.
 * @param {string} value The details of the update.
 * @constructor
 */
var PeerConnectionUpdateEntry = function(pid, lid, type, value) {
  /**
   * @type {number}
   */
  this.pid = pid;

  /**
   * @type {number}
   */
  this.lid = lid;

  /**
   * @type {string}
   */
  this.type = type;

  /**
   * @type {string}
   */
  this.value = value;
};


/**
 * Maintains the peer connection update log table.
 */
var PeerConnectionUpdateTable = (function() {
  'use strict';

  /**
   * @constructor
   */
  function PeerConnectionUpdateTable() {
    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_ID_SUFFIX_ = '-update-log';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_CONTAINER_CLASS_ = 'update-log-container';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_TABLE_CLASS = 'update-log-table';
  }

  PeerConnectionUpdateTable.prototype = {
    /**
     * Adds the update to the update table as a new row. The type of the update
     * is set to the summary of the cell; clicking the cell will reveal or hide
     * the details as the content of a TextArea element.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!PeerConnectionUpdateEntry} update The update to add.
     */
    addPeerConnectionUpdate: function(peerConnectionElement, update) {
      var tableElement = this.ensureUpdateContainer_(peerConnectionElement);

      var row = document.createElement('tr');
      tableElement.firstChild.appendChild(row);

      var time = new Date(parseFloat(update.time));
      row.innerHTML = '<td>' + time.toLocaleString() + '</td>';

      if (update.value.length == 0) {
        row.innerHTML += '<td>' + update.type + '</td>';
        return;
      }

      row.innerHTML += '<td><details><summary>' + update.type +
          '</summary></details></td>';

      var valueContainer = document.createElement('pre');
      var details = row.cells[1].childNodes[0];
      details.appendChild(valueContainer);
      valueContainer.textContent = update.value;
    },

    /**
     * Makes sure the update log table of the peer connection is created.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @return {!Element} The log table element.
     * @private
     */
    ensureUpdateContainer_: function(peerConnectionElement) {
      var tableId = peerConnectionElement.id + this.UPDATE_LOG_ID_SUFFIX_;
      var tableElement = $(tableId);
      if (!tableElement) {
        var tableContainer = document.createElement('div');
        tableContainer.className = this.UPDATE_LOG_CONTAINER_CLASS_;
        peerConnectionElement.appendChild(tableContainer);

        tableElement = document.createElement('table');
        tableElement.className = this.UPDATE_LOG_TABLE_CLASS;
        tableElement.id = tableId;
        tableElement.border = 1;
        tableContainer.appendChild(tableElement);
        tableElement.innerHTML = '<tr><th>Time</th>' +
            '<th class="update-log-header-event">Event</th></tr>';
      }
      return tableElement;
    }
  };

  return PeerConnectionUpdateTable;
})();
