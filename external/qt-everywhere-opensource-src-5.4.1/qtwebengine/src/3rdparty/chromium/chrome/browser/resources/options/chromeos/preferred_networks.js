// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var DeletableItem = options.DeletableItem;
  var DeletableItemList = options.DeletableItemList;

  /////////////////////////////////////////////////////////////////////////////
  // NetworkPreferences class:

  /**
   * Encapsulated handling of ChromeOS network preferences page.
   * @constructor
   */
  function PreferredNetworks(model) {
    OptionsPage.call(this,
                     'preferredNetworksPage',
                     null,
                     'preferredNetworksPage');
  }

  cr.addSingletonGetter(PreferredNetworks);

  PreferredNetworks.prototype = {
    __proto__: OptionsPage.prototype,

   /**
     * Initializes the preferred networks page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      PreferredNetworkList.decorate($('remembered-network-list'));
      $('preferred-networks-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    update: function(rememberedNetworks) {
      var list = $('remembered-network-list');
      list.clear();
      for (var i = 0; i < rememberedNetworks.length; i++) {
        list.append(rememberedNetworks[i]);
      }
      list.redraw();
    }

  };

  /**
   * Creates a list entry for a remembered network.
   * @param{{networkName: string,
             networkType: string,
             servicePath: string}} data
   *    Description of the network.
   * @constructor
   */
  function PreferredNetworkListItem(data) {
    var el = cr.doc.createElement('div');
    el.__proto__ = PreferredNetworkListItem.prototype;
    el.data = {};
    for (var key in data)
      el.data[key] = data[key];
    el.decorate();
    return el;
  }

  PreferredNetworkListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Description of the network.
     * @type {{networkName: string,
     *         networkType: string,
     *         servicePath: string}}
     */
    data: null,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);
      var label = this.ownerDocument.createElement('div');
      label.textContent = this.data.networkName;
      if (this.data.policyManaged)
        this.deletable = false;
      this.contentElement.appendChild(label);
    }
  };

  /**
   * Class for displaying a list of preferred networks.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var PreferredNetworkList = cr.ui.define('list');

  PreferredNetworkList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.addEventListener('blur', this.onBlur_);
      this.clear();
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },

    /** @override */
    createItem: function(entry) {
      return new PreferredNetworkListItem(entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item) {
        // Inform the network library that we are forgetting this network.
        chrome.send('networkCommand',
                    [item.networkType,
                    item.servicePath,
                    'forget']);
      }
      this.dataModel.splice(index, 1);
      // Invalidate the list since it has a stale cache after a splice
      // involving a deletion.
      this.invalidate();
      this.redraw();
    },

    /**
     * Purges all networks from the list.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
      this.redraw();
    },

    /**
     * Adds a remembered network to the list.
     * @param {{networkName: string,
                networkType: string,
                servicePath: string} data
     *     Description of the network.
     */
    append: function(data) {
      this.dataModel.push(data);
    }
  };

  // Export
  return {
    PreferredNetworks: PreferredNetworks
  };

});
