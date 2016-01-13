// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.system.bluetooth', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Bluetooth settings constants.
   */
  function Constants() {}

  /**
   * Creates a new bluetooth list item.
   * @param {{name: string,
   *          address: string,
   *          paired: boolean,
   *          connected: boolean,
   *          connecting: boolean,
   *          connectable: boolean,
   *          pairing: string|undefined,
   *          passkey: number|undefined,
   *          pincode: string|undefined,
   *          entered: number|undefined}} device
   *    Description of the Bluetooth device.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function BluetoothListItem(device) {
    var el = cr.doc.createElement('div');
    el.__proto__ = BluetoothListItem.prototype;
    el.data = {};
    for (var key in device)
      el.data[key] = device[key];
    el.decorate();
    // Only show the close button for paired devices, but not for connecting
    // devices.
    el.deletable = device.paired && !device.connecting;
    return el;
  }

  BluetoothListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Description of the Bluetooth device.
     * @type {{name: string,
     *         address: string,
     *         paired: boolean,
     *         connected: boolean,
     *         connecting: boolean,
     *         connectable: boolean,
     *         pairing: string|undefined,
     *         passkey: number|undefined,
     *         pincode: string|undefined,
     *         entered: number|undefined}}
     */
    data: null,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);
      var label = this.ownerDocument.createElement('div');
      label.className = 'bluetooth-device-label';
      this.classList.add('bluetooth-device');
      // There are four kinds of devices we want to distinguish:
      //  * Connecting devices: in bold with a "connecting" label,
      //  * Connected devices: in bold,
      //  * Paired, not connected but connectable devices: regular and
      //  * Paired, not connected and not connectable devices: grayed out.
      this.connected = this.data.connecting ||
          (this.data.paired && this.data.connected);
      this.notconnectable = this.data.paired && !this.data.connecting &&
          !this.data.connected && !this.data.connectable;
      // "paired" devices are those that are remembered but not connected.
      this.paired = this.data.paired && !this.data.connected &&
          this.data.connectable;

      var content = this.data.name;
      // Update the device's label according to its state. A "connecting" device
      // can be in the process of connecting and pairing, so we check connecting
      // first.
      if (this.data.connecting) {
        content = loadTimeData.getStringF('bluetoothDeviceConnecting',
            this.data.name);
      }
      label.textContent = content;
      this.contentElement.appendChild(label);
    },
  };

  /**
   * Class for displaying a list of Bluetooth devices.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var BluetoothDeviceList = cr.ui.define('list');

  BluetoothDeviceList.prototype = {
    __proto__: DeletableItemList.prototype,

    /**
     * Height of a list entry in px.
     * @type {number}
     * @private
     */
    itemHeight_: 32,

    /**
     * Width of a list entry in px.
     * @type {number}
     * @private.
     */
    itemWidth_: 400,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      // Force layout of all items even if not in the viewport to address
      // errors in scroll positioning when the list is hidden during initial
      // layout. The impact on performance should be minimal given that the
      // list is not expected to grow very large. Fixed height items are also
      // required to avoid caching incorrect sizes during layout of a hidden
      // list.
      this.autoExpands = true;
      this.fixedHeight = true;
      this.clear();
      this.selectionModel = new ListSingleSelectionModel();
    },

    /**
     * Adds a bluetooth device to the list of available devices. A check is
     * made to see if the device is already in the list, in which case the
     * existing device is updated.
     * @param {{name: string,
     *          address: string,
     *          paired: boolean,
     *          connected: boolean,
     *          connecting: boolean,
     *          connectable: boolean,
     *          pairing: string|undefined,
     *          passkey: number|undefined,
     *          pincode: string|undefined,
     *          entered: number|undefined}} device
     *     Description of the bluetooth device.
     * @return {boolean} True if the devies was successfully added or updated.
     */
    appendDevice: function(device) {
      var selectedDevice = this.getSelectedDevice_();
      var index = this.find(device.address);
      if (index == undefined) {
        this.dataModel.push(device);
        this.redraw();
      } else {
        this.dataModel.splice(index, 1, device);
        this.redrawItem(index);
      }
      this.updateListVisibility_();
      if (selectedDevice)
        this.setSelectedDevice_(selectedDevice);
      return true;
    },

    /**
     * Forces a revailidation of the list content. Deleting a single item from
     * the list results in a stale cache requiring an invalidation.
     * @param {string=} opt_selection Optional address of device to select
     *     after refreshing the list.
     */
    refresh: function(opt_selection) {
      // TODO(kevers): Investigate if the stale cache issue can be fixed in
      // cr.ui.list.
      var selectedDevice = opt_selection ? opt_selection :
          this.getSelectedDevice_();
      this.invalidate();
      this.redraw();
      if (selectedDevice)
        this.setSelectedDevice_(selectedDevice);
    },

    /**
     * Retrieves the address of the selected device, or null if no device is
     * selected.
     * @return {?string} Address of selected device or null.
     * @private
     */
    getSelectedDevice_: function() {
      var selection = this.selectedItem;
      if (selection)
        return selection.address;
      return null;
    },

    /**
     * Selects the device with the matching address.
     * @param {string} address The unique address of the device.
     * @private
     */
    setSelectedDevice_: function(address) {
      var index = this.find(address);
      if (index != undefined)
        this.selectionModel.selectRange(index, index);
    },

    /**
     * Perges all devices from the list.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
      this.redraw();
      this.updateListVisibility_();
    },

    /**
     * Returns the index of the list entry with the matching address.
     * @param {string} address Unique address of the Bluetooth device.
     * @return {number|undefined} Index of the matching entry or
     * undefined if no match found.
     */
    find: function(address) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.address == address)
          return i;
      }
    },

    /** @override */
    createItem: function(entry) {
      return new BluetoothListItem(entry);
    },

    /**
     * Overrides the default implementation, which is used to compute the
     * size of an element in the list.  The default implementation relies
     * on adding a placeholder item to the list and fetching its size and
     * position. This strategy does not work if an item is added to the list
     * while it is hidden, as the computed metrics will all be zero in that
     * case.
     * @return {{height: number, marginTop: number, marginBottom: number,
     *     width: number, marginLeft: number, marginRight: number}}
     *     The height and width of the item, taking margins into account,
     *     and the margins themselves.
     */
    measureItem: function() {
      return {
        height: this.itemHeight_,
        marginTop: 0,
        marginBotton: 0,
        width: this.itemWidth_,
        marginLeft: 0,
        marginRight: 0
      };
    },

    /**
     * Override the default implementation to return a predetermined size,
     * which in turns allows proper layout of items even if the list is hidden.
     * @return {height: number, width: number} Dimensions of a single item in
     *     the list of bluetooth device.
     * @private.
     */
    getDefaultItemSize_: function() {
      return {
        height: this.itemHeight_,
        width: this.itemWidth_
      };
    },

    /**
     * Override base implementation of handleClick_, which unconditionally
     * removes the item.  In this case, removal of the element is deferred
     * pending confirmation from the Bluetooth adapter.
     * @param {Event} e The click event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      var target = e.target;
      if (!target.classList.contains('row-delete-button'))
        return;

      var item = this.getListItemAncestor(target);
      var selected = this.selectionModel.selectedIndex;
      var index = this.getIndexOfListItem(item);
      if (item && item.deletable) {
        if (selected != index)
          this.setSelectedDevice_(item.data.address);
        // Device is busy until we hear back from the Bluetooth adapter.
        // Prevent double removal request.
        item.deletable = false;
        // TODO(kevers): Provide visual feedback that the device is busy.

        // Inform the bluetooth adapter that we are disconnecting or
        // forgetting the device.
        chrome.send('updateBluetoothDevice',
          [item.data.address, item.connected ? 'disconnect' : 'forget']);
      }
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var selectedDevice = this.getSelectedDevice_();
      this.dataModel.splice(index, 1);
      this.refresh(selectedDevice);
      this.updateListVisibility_();
    },

    /**
     * If the list has an associated empty list placholder then update the
     * visibility of the list and placeholder.
     * @private
     */
    updateListVisibility_: function() {
      var empty = this.dataModel.length == 0;
      var listPlaceHolderID = this.id + '-empty-placeholder';
      if ($(listPlaceHolderID)) {
        if (this.hidden != empty) {
          this.hidden = empty;
          $(listPlaceHolderID).hidden = !empty;
          this.refresh();
        }
      }
    },
  };

  cr.defineProperty(BluetoothListItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  cr.defineProperty(BluetoothListItem, 'paired', cr.PropertyKind.BOOL_ATTR);

  cr.defineProperty(BluetoothListItem, 'connecting', cr.PropertyKind.BOOL_ATTR);

  cr.defineProperty(BluetoothListItem, 'notconnectable',
      cr.PropertyKind.BOOL_ATTR);

  return {
    BluetoothListItem: BluetoothListItem,
    BluetoothDeviceList: BluetoothDeviceList,
    Constants: Constants
  };
});
