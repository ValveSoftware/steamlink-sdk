// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('network.status', function() {
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Returns the entries of |dataModel| as an array.
   * @param {ArrayDataModel} dataModel .
   * @return {Array} .
   */
  function dataModelToArray(dataModel) {
    var array = [];
    for (var i = 0; i < dataModel.length; i++) {
      array.push(dataModel.item(i));
    }
    return array;
  }

  /**
   * Calculates both set difference of |a| and |b| and returns them in an array:
   * [ a - b, b - a ].
   * @param {Array.<T>} a .
   * @param {Array.<T>} b .
   * @param {function(T): K} toKey .
   * @return {Array.<Array.<T>>} .
   */
  function differenceBy(a, b, toKey) {
    var inA = {};
    a.forEach(function(elA) {
      inA[toKey(elA)] = elA;
    });
    var bMinusA = [];
    b.forEach(function(elB) {
      var keyB = toKey(elB);
      if (inA[keyB])
        delete inA[keyB];
      else
        bMinusA.push(elB);
    });
    var aMinusB = [];
    for (var keyA in inA) {
      aMinusB.push(inA[keyA]);
    }
    return [aMinusB, bMinusA];
  }

  /**
   * Updates the data model of |uiList| to |newList|. Ensures that unchanged
   * entries are not touched. Doesn't preserve the order of |newList|.
   * @param {List} uiList .
   * @param {Array} newList .
   */
  function updateDataModel(uiList, newList) {
    uiList.startBatchUpdates();
    var dataModel = uiList.dataModel;
    var diff = differenceBy(dataModelToArray(dataModel), newList,
                            function(e) { return e[0]; });
    var toRemove = diff[0];
    var toAdd = diff[1];
    toRemove.forEach(function(element) {
      dataModel.splice(dataModel.indexOf(element), 1);
    });
    dataModel.splice.apply(dataModel, [dataModel.length, 0].concat(toAdd));
    uiList.endBatchUpdates();
  }

  /**
   * Creates a map of the entries of |array|. Each entry is associated to the
   * key getKey(entry).
   * @param {Array.<T>} array .
   * @param {function(T): K} getKey .
   * @return {Object.<K, T>} .
   */
  function createMapFromList(array, getKey) {
    var result = {};
    array.forEach(function(entry) {
      result[getKey(entry)] = entry;
    });
    return result;
  }

  /**
   * Wraps each entry in |array| into an array. The result contains rows with
   * one entry each.
   * @param {Array.<T>} array .
   * @return {Array.<Array.<T>>} .
   */
  function arrayToTable(array) {
    return array.map(function(e) {
      return [e];
    });
  }

  /**
   * The NetworkStatusList contains various button types according to the
   * following hierarchy. Note: this graph doesn't depict inheritance but
   * ownership of instances of these types.
   *
   * NetworkStatusList
   *   +-- TechnologyButton
   *       +-- NestedStatusButton
   *
   * Inheritance hierarchy:
   * ListItem
   *   +-- Button
   *       +-- UnfoldingButton
   *       |   +-- TechnologyButton
   *       +-- NestedStatusButton
   */

  /**
   * Base class for all buttons in the NetworkStatusList. |key| is used to
   * identify this button.
   * After construction, update() has to be called!
   * @param {NetworkStatusList} networkStatus .
   * @param {string} key .
   * @constructor
   */
  function Button(networkStatus, key) {
    var el = cr.doc.createElement('li');
    el.__proto__ = Button.prototype;
    el.decorate(networkStatus, key);
    return el;
  }

  Button.prototype = {
    __proto__: ListItem.prototype,

    /**
     * @override
     */
    decorate: function(networkStatus, key) {
      ListItem.prototype.decorate.call(this);
      this.networkStatus_ = networkStatus;
      this.key_ = key;
      this.className = 'network-status-button';

      var textContent = this.ownerDocument.createElement('div');
      textContent.className = 'network-status-button-labels';

      var title = this.ownerDocument.createElement('div');
      title.className = 'nested-status-button-title';
      textContent.appendChild(title);
      this.title_ = title;

      var subTitle = this.ownerDocument.createElement('div');
      subTitle.className = 'nested-status-button-subtitle';
      textContent.appendChild(subTitle);
      this.subTitle_ = subTitle;

      this.appendChild(textContent);
    },

    /**
     * @type {string}
     */
    get key() {
      return this.key_;
    },

    /**
     * Should be called if the data presented by this button
     * changed. E.g. updates the button's title, icon and nested buttons.
     * To be overriden by subclasses.
     */
    update: function() {
      this.title_.textContent = this.key;
    }
  };

  /**
   * A button that shows the status of one particular network.
   * @param {NetworkStatusList} networkStatus .
   * @param {string} networkID .
   * @extends {Button}
   * @constructor
   */
  function NestedStatusButton(networkStatus, networkID) {
    var el = new Button(networkStatus, networkID);
    el.__proto__ = NestedStatusButton.prototype;
    return el;
  }

  NestedStatusButton.prototype = {
    __proto__: Button.prototype,

    /**
     * @override
     */
    update: function() {
      var network = this.networkStatus_.getNetworkByID(this.networkID);
      this.title_.textContent = network.Name;
      this.subTitle_.textContent = network.ConnectionState;
    },

    get networkID() {
      return this.key;
    }
  };

  /**
   * A button (toplevel in the NetworkStatusList) that unfolds a list of nested
   * buttons when clicked. Only one button will be unfolded at a time.
   * @param {NetworkStatusList} networkStatus .
   * @param {string} key .
   * @extends {Button}
   * @constructor
   */
  function UnfoldingButton(networkStatus, key) {
    var el = new Button(networkStatus, key);
    el.__proto__ = UnfoldingButton.prototype;
    el.decorate();
    return el;
  }

  UnfoldingButton.prototype = {
    __proto__: Button.prototype,

    /**
     * @override
     */
    decorate: function() {
      this.dropdown_ = null;
      this.addEventListener('click', this.toggleDropdown_.bind(this));
    },

    /**
     * Returns the list of identifiers for which nested buttons will be created.
     * To be overridden by subclasses.
     * @return {Array.<string>} .
     */
    getEntries: function() {
      return [];
    },

    /**
     * Creates a nested button for |entry| of the current |getEntries|.
     * To be overridden by subclasses.
     * @param {string} entry .
     * @return {ListItem} .
     */
    createNestedButton: function(entry) {
      return new ListItem(entry);
    },

    /**
     * Creates the dropdown list containing the nested buttons.
     * To be overridden by subclasses.
     * @return {List} .
     */
    createDropdown: function() {
      var list = new List();
      var self = this;
      list.createItem = function(row) {
        return self.createNestedButton(row[0]);
      };
      list.autoExpands = true;
      list.dataModel = new ArrayDataModel(arrayToTable(this.getEntries()));
      list.selectionModel = new ListSingleSelectionModel();
      return list;
    },

    /**
     * @override
     */
    update: function() {
      Button.prototype.update.call(this);
      if (!this.dropdown_)
        return;
      updateDataModel(this.dropdown_, arrayToTable(this.getEntries()));
    },

    openDropdown_: function() {
      var dropdown = this.createDropdown();
      dropdown.className = 'network-dropdown';
      this.appendChild(dropdown);
      this.dropdown_ = dropdown;
      this.networkStatus_.openDropdown = this;
    },

    closeDropdown_: function() {
      this.removeChild(this.dropdown_);
      this.dropdown_ = null;
    },

    toggleDropdown_: function() {
      // TODO(pneubeck): request rescan
      if (this.networkStatus_.openDropdown === this) {
        this.closeDropdown_();
        this.networkStatus_.openDropdown = null;
      } else if (this.networkStatus_.openDropdown) {
        this.networkStatus_.openDropdown.closeDropdown_();
        this.openDropdown_();
      } else {
        this.openDropdown_();
      }
    }
  };

  /**
   * A button (toplevel in the NetworkStatusList) that represents one network
   * technology (like WiFi or Ethernet) and unfolds a list of nested buttons
   * when clicked.
   * @param {NetworkStatusList} networkStatus .
   * @param {string} technology .
   * @extends {UnfoldingButton}
   * @constructor
   */
  function TechnologyButton(networkStatus, technology) {
    var el = new UnfoldingButton(networkStatus, technology);
    el.__proto__ = TechnologyButton.prototype;
    el.decorate(technology);
    return el;
  }

  TechnologyButton.prototype = {
    __proto__: UnfoldingButton.prototype,

    /**
     * @param {string} technology .
     * @override
     */
    decorate: function(technology) {
      this.technology_ = technology;
    },

    /**
     * @override
     */
    getEntries: function() {
      return this.networkStatus_.getNetworkIDsOfType(this.technology_);
    },

    /**
     * @override
     */
    createNestedButton: function(id) {
      var self = this;
      var button = new NestedStatusButton(this.networkStatus_, id);
      button.onclick = function(e) {
        e.stopPropagation();
        self.networkStatus_.handleUserAction({
          command: 'openConfiguration',
          networkId: id
        });
      };
      button.update();
      return button;
    },

    /**
     * @override
     */
    createDropdown: function() {
      var list = UnfoldingButton.prototype.createDropdown.call(this);
      var getIndex = this.networkStatus_.getIndexOfNetworkID.bind(
          this.networkStatus_);
      list.dataModel.setCompareFunction(0, function(a, b) {
        return ArrayDataModel.prototype.defaultValuesCompareFunction(
            getIndex(a),
            getIndex(b));
      });
      list.dataModel.sort(0, 'asc');
      return list;
    },

    /**
     * @type {string}
     */
    get technology() {
      return this.technology_;
    },

    /**
     * @override
     */
    update: function() {
      UnfoldingButton.prototype.update.call(this);
      if (!this.dropdown_)
        return;
      this.dropdown_.items.forEach(function(button) {
        button.update();
      });
    }
  };

  /**
   * The order of the toplevel buttons.
   */
  var BUTTON_ORDER = [
    'Ethernet',
    'WiFi',
    'Cellular',
    'VPN',
    'addConnection'
  ];

  /**
   * A map from button key to index according to |BUTTON_ORDER|.
   */
  var BUTTON_POSITION = {};
  BUTTON_ORDER.forEach(function(entry, index) {
    BUTTON_POSITION[entry] = index;
  });

  /**
   * Groups networks by type.
   * @param {Object.<string, Object>} networkByID A map from network ID to
   * network properties.
   * @return {Object.<string, Array.<string>>} A map from network type to the
   * list of IDs of networks of that type.
   */
  function createNetworkIDsByType(networkByID) {
    var byType = {};
    for (var id in networkByID) {
      var network = networkByID[id];
      var group = byType[network.Type];
      if (group === undefined) {
        group = [];
        byType[network.Type] = group;
      }
      group.push(network.GUID);
    }
    return byType;
  }

  /**
   * A list-like control showing the available networks and controls to
   * dis-/connect to networks and to open dialogs to create, modify and remove
   * network configurations.
   * @constructor
   */
  var NetworkStatusList = cr.ui.define('list');

  NetworkStatusList.prototype = {
    __proto__: List.prototype,

    /**
     * @override
     */
    decorate: function() {
      List.prototype.decorate.call(this);

      /**
       * The currently open unfolding button.
       * @type {UnfoldingButton}
       */
      this.openDropdown = null;

      /**
       * The set of technologies shown to the user.
       * @type {Object.<string, boolean>}
       */
      this.technologies_ = {};

      /**
       * A map from network type to the array of IDs of network of that type.
       * @type {Object.<string, Array.<string>>}
       */
      this.networkIDsByType_ = {};

      /**
       * A map from network ID to the network's properties.
       * @type {Object.<string, Object>}
       */
      this.networkByID_ = {};

      /**
       * A map from network ID to the network's position in the last received
       * network list.
       * @type {Object.<string, number>}
       */
      this.networkIndexByID_ = {};

      /**
       * A function that handles the various user actions.
       * See |setUserActionHandler|.
       * @type {function({command: string, networkID: string})}
       */
      this.userActionHandler_ = function() {};

      this.autoExpands = true;
      this.dataModel = new ArrayDataModel([]);
      this.dataModel.setCompareFunction(0, function(a, b) {
        return ArrayDataModel.prototype.defaultValuesCompareFunction(
            BUTTON_POSITION[a], BUTTON_POSITION[b]);
      });
      this.dataModel.sort(0, 'asc');
      this.selectionModel = new ListSingleSelectionModel();

      this.updateDataStructuresAndButtons_();
      this.registerStatusListener_();
    },

    /**
     * @override
     */
    createItem: function(row) {
      var key = row[0];
      if (key in this.technologies_) {
        var button = new TechnologyButton(
            this,
            key);
        button.update();
        return button;
      } else {
        return new Button(this, key);
      }
    },

    /**
     * See |setUserActionHandler| for the possible commands.
     * @param {{command: string, networkID: string}} action .
     */
    handleUserAction: function(action) {
      this.userActionHandler_(action);
    },

    /**
     * A function that handles the various user actions.
     * |command| will be one of
     *   - openConfiguration
     * @param {function({command: string, networkID: string})} handler .
     */
    setUserActionHandler: function(handler) {
      this.userActionHandler_ = handler;
    },

    /**
     * @param {string} technology .
     * @return {Array.<string>} Array of network IDs.
     */
    getNetworkIDsOfType: function(technology) {
      var networkIDs = this.networkIDsByType_[technology];
      if (!networkIDs)
        return [];
      return networkIDs;
    },

    /**
     * @param {string} networkID .
     * @return {number} The index of network with |networkID| in the last
     * received network list.
     */
    getIndexOfNetworkID: function(networkID) {
      return this.networkIndexByID_[networkID];
    },

    /**
     * @param {string} networkID .
     * @return {Object} The last received properties of network with
     * |networkID|.
     */
    getNetworkByID: function(networkID) {
      return this.networkByID_[networkID];
    },

    /**
     * @param {string} networkType .
     * @return {?TechnologyButton} .
     */
    getTechnologyButtonForType_: function(networkType) {
      var buttons = this.items;
      for (var i = 0; i < buttons.length; i++) {
        var button = buttons[i];
        if (button instanceof TechnologyButton &&
            button.technology === networkType) {
          return button;
        }
      }
      console.log('TechnologyButton for type ' + networkType +
          ' requested but not found.');
      return null;
    },

    updateTechnologiesFromNetworks_: function() {
      var newTechnologies = {};
      Object.keys(this.networkIDsByType_).forEach(function(technology) {
        newTechnologies[technology] = true;
      });
      this.technologies_ = newTechnologies;
    },

    updateDataStructuresAndButtons_: function() {
      this.networkIDsByType_ = createNetworkIDsByType(this.networkByID_);
      this.updateTechnologiesFromNetworks_();
      var keys = Object.keys(this.technologies_);
      // Add keys of always visible toplevel buttons.
      keys.push('addConnection');
      updateDataModel(this, arrayToTable(keys));
      this.items.forEach(function(button) {
        button.update();
      });
    },

    /**
     * @param {Array.<string>} networkIDs .
     */
    updateIndexes_: function(networkIDs) {
      var newNetworkIndexByID = {};
      networkIDs.forEach(function(id, index) {
        newNetworkIndexByID[id] = index;
      });
      this.networkIndexByID_ = newNetworkIndexByID;
    },

    /**
     * @param {Array.<string>} networkIDs .
     */
    onNetworkListChanged_: function(networkIDs) {
      var diff = differenceBy(Object.keys(this.networkByID_),
                              networkIDs,
                              function(e) { return e; });
      var toRemove = diff[0];
      var toAdd = diff[1];

      var addCallback = this.addNetworkCallback_.bind(this);
      toAdd.forEach(function(id) {
        console.log('NetworkStatus: Network ' + id + ' added.');
        chrome.networkingPrivate.getProperties(id, addCallback);
      });

      toRemove.forEach(function(id) {
        console.log('NetworkStatus: Network ' + id + ' removed.');
        delete this.networkByID_[id];
      }, this);

      this.updateIndexes_(networkIDs);
      this.updateDataStructuresAndButtons_();
    },

    /**
     * @param {Array.<string>} networkIDs .
     */
    onNetworksChanged_: function(networkIDs) {
      var updateCallback = this.updateNetworkCallback_.bind(this);
      networkIDs.forEach(function(id) {
        console.log('NetworkStatus: Network ' + id + ' changed.');
        chrome.networkingPrivate.getProperties(id, updateCallback);
      });
    },

    /**
     * @param {Object} network .
     */
    updateNetworkCallback_: function(network) {
      this.networkByID_[network.GUID] = network;
      this.getTechnologyButtonForType_(network.Type).update();
    },

    /**
     * @param {Object} network .
     */
    addNetworkCallback_: function(network) {
      this.networkByID_[network.GUID] = network;
      this.updateDataStructuresAndButtons_();
    },

    /**
     * @param {Array.<Object>} networks .
     */
    setVisibleNetworks: function(networks) {
      this.networkByID_ = createMapFromList(
          networks,
          function(network) {
            return network.GUID;
          });
      this.updateIndexes_(networks.map(function(network) {
        return network.GUID;
      }));
      this.updateDataStructuresAndButtons_();
    },

    /**
     * Registers |this| at the networkingPrivate extension API and requests an
     * initial list of all networks.
     */
    registerStatusListener_: function() {
      chrome.networkingPrivate.onNetworkListChanged.addListener(
          this.onNetworkListChanged_.bind(this));
      chrome.networkingPrivate.onNetworksChanged.addListener(
          this.onNetworksChanged_.bind(this));
      chrome.networkingPrivate.getNetworks(
          { 'networkType': 'All', 'visible': true },
          this.setVisibleNetworks.bind(this));
    }
  };

  return {
    NetworkStatusList: NetworkStatusList
  };
});
