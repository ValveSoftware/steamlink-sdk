// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceTable UI, served from chrome://bluetooth-internals/.
 */

cr.define('device_table', function() {
  /**
   * A table that lists the devices and responds to changes in the given
   *     DeviceCollection.
   * @constructor
   * @extends {HTMLTableElement}
   */
  var DeviceTable = cr.ui.define(function() {
    /** @private {?Array<device_collection.Device>} */
    this.devices_ = null;

    return document.importNode($('table-template').content.children[0],
                               true /* deep */);
  });

  DeviceTable.prototype = {
    __proto__: HTMLTableElement.prototype,

    /**
     * Decorates an element as a UI element class. Caches references to the
     *    table body and headers.
     */
    decorate: function() {
      /** @private */
      this.body_ = this.tBodies[0];
      /** @private */
      this.headers_ = this.tHead.rows[0].cells;
    },

    /**
     * Sets the tables device collection.
     * @param {!device_collection.DeviceCollection} deviceCollection
     */
    setDevices: function(deviceCollection) {
      assert(!this.devices_, 'Devices can only be set once.');

      this.devices_ = deviceCollection;
      this.devices_.addEventListener('sorted', this.redraw_.bind(this));
      this.devices_.addEventListener('change', this.handleChange_.bind(this));
      this.devices_.addEventListener('splice', this.handleSplice_.bind(this));

      this.redraw_();
    },

    /**
     * Updates table row on change event of the device collection.
     * @private
     * @param {!CustomEvent} event
     */
    handleChange_: function(event) {
      this.updateRow_(this.devices_.item(event.index), event.index);
    },

    /**
     * Updates table row on splice event of the device collection.
     * @private
     * @param {!CustomEvent} event
     */
    handleSplice_: function(event) {
      event.removed.forEach(function() {
        this.body_.deleteRow(event.index);
      }, this);

      event.added.forEach(function(device, index) {
        this.insertRow_(device, event.index + index);
      }, this);
    },

    /**
     * Inserts a new row at |index| and updates it with info from |device|.
     * @private
     * @param {!device_collection.Device} device
     * @param {?number} index
     */
    insertRow_: function(device, index) {
      var row = this.body_.insertRow(index);
      row.id = device.info.address;

      for (var i = 0; i < this.headers_.length; i++) {
        row.insertCell();
      }

      this.updateRow_(device, row.sectionRowIndex);
    },

    /**
     * Deletes and recreates the table using the cached |devices_|.
     * @private
     */
    redraw_: function() {
      this.removeChild(this.body_);
      this.appendChild(document.createElement('tbody'));
      this.body_ = this.tBodies[0];
      this.body_.classList.add('table-body');

      for (var i = 0; i < this.devices_.length; i++) {
        this.insertRow_(this.devices_.item(i));
      }
    },

    /**
     * Updates the row at |index| with the info from |device|.
     * @private
     * @param {!device_collection.Device} device
     * @param {number} index
     */
    updateRow_: function(device, index) {
      assert(this.body_.rows[index], 'Row ' + index + ' is not in the table.');
      var row = this.body_.rows[index];

      row.classList.toggle('removed', device.removed);

      // Update the properties based on the header field path.
      for (var i = 0; i < this.headers_.length; i++) {
        var header = this.headers_[i];
        var propName = header.dataset.field;

        var parts = propName.split('.');
        var obj = device.info;
        while (obj != null && parts.length > 0) {
          var part = parts.shift();
          obj = obj[part];
        }

        var cell = row.cells[i];
        cell.textContent = obj || 'Unknown';
        cell.dataset.label = header.textContent;
      }
    },
  };

  return {
    DeviceTable: DeviceTable,
  };
});
