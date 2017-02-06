// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  // This structure maps the various cookie type names from C++ (hence the
  // underscores) to arrays of the different types of data each has, along with
  // the i18n name for the description of that data type.
  /** @const */ var cookieInfo = {
    'cookie': [['name', 'label_cookie_name'],
               ['content', 'label_cookie_content'],
               ['domain', 'label_cookie_domain'],
               ['path', 'label_cookie_path'],
               ['sendfor', 'label_cookie_send_for'],
               ['accessibleToScript', 'label_cookie_accessible_to_script'],
               ['created', 'label_cookie_created'],
               ['expires', 'label_cookie_expires']],
    'app_cache': [['manifest', 'label_app_cache_manifest'],
                  ['size', 'label_local_storage_size'],
                  ['created', 'label_cookie_created'],
                  ['accessed', 'label_cookie_last_accessed']],
    'database': [['name', 'label_cookie_name'],
                 ['desc', 'label_webdb_desc'],
                 ['size', 'label_local_storage_size'],
                 ['modified', 'label_local_storage_last_modified']],
    'local_storage': [['origin', 'label_local_storage_origin'],
                      ['size', 'label_local_storage_size'],
                      ['modified', 'label_local_storage_last_modified']],
    'indexed_db': [['origin', 'label_indexed_db_origin'],
                   ['size', 'label_indexed_db_size'],
                   ['modified', 'label_indexed_db_last_modified']],
    'file_system': [['origin', 'label_file_system_origin'],
                    ['persistent', 'label_file_system_persistent_usage'],
                    ['temporary', 'label_file_system_temporary_usage']],
    'channel_id': [['serverId', 'label_channel_id_server_id'],
                          ['certType', 'label_channel_id_type'],
                          ['created', 'label_channel_id_created']],
    'service_worker': [['origin', 'label_service_worker_origin'],
                       ['size', 'label_service_worker_size'],
                       ['scopes', 'label_service_worker_scopes']],
    'cache_storage': [['origin', 'label_cache_storage_origin'],
                       ['size', 'label_cache_storage_size'],
                       ['modified', 'label_cache_storage_last_modified']],
    'flash_lso': [['domain', 'label_cookie_domain']],
  };

  /**
   * Returns the item's height, like offsetHeight but such that it works better
   * when the page is zoomed. See the similar calculation in @{code cr.ui.List}.
   * This version also accounts for the animation done in this file.
   * @param {Element} item The item to get the height of.
   * @return {number} The height of the item, calculated with zooming in mind.
   */
  function getItemHeight(item) {
    var height = item.style.height;
    // Use the fixed animation target height if set, in case the element is
    // currently being animated and we'd get an intermediate height below.
    if (height && height.substr(-2) == 'px')
      return parseInt(height.substr(0, height.length - 2), 10);
    return item.getBoundingClientRect().height;
  }

  /**
   * Create tree nodes for the objects in the data array, and insert them all
   * into the given list using its @{code splice} method at the given index.
   * @param {Array<Object>} data The data objects for the nodes to add.
   * @param {number} start The index at which to start inserting the nodes.
   * @return {Array<options.CookieTreeNode>} An array of CookieTreeNodes added.
   */
  function spliceTreeNodes(data, start, list) {
    var nodes = data.map(function(x) { return new CookieTreeNode(x); });
    // Insert [start, 0] at the beginning of the array of nodes, making it
    // into the arguments we want to pass to @{code list.splice} below.
    nodes.splice(0, 0, start, 0);
    list.splice.apply(list, nodes);
    // Remove the [start, 0] prefix and return the array of nodes.
    nodes.splice(0, 2);
    return nodes;
  }

  /**
   * Adds information about an app that protects this data item to the
   * |element|.
   * @param {Element} element The DOM element the information should be
         appended to.
   * @param {{id: string, name: string}} appInfo Information about an app.
   */
  function addAppInfo(element, appInfo) {
    var img = element.ownerDocument.createElement('img');
    img.src = 'chrome://extension-icon/' + appInfo.id + '/16/1';
    element.title = loadTimeData.getString('label_protected_by_apps') +
                    ' ' + appInfo.name;
    img.className = 'protecting-app';
    element.appendChild(img);
  }

  var parentLookup = {};
  var lookupRequests = {};

  /**
   * Creates a new list item for sites data. Note that these are created and
   * destroyed lazily as they scroll into and out of view, so they must be
   * stateless. We cache the expanded item in @{code CookiesList} though, so it
   * can keep state. (Mostly just which item is selected.)
   * @param {Object} origin Data used to create a cookie list item.
   * @param {options.CookiesList} list The list that will contain this item.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function CookieListItem(origin, list) {
    var listItem = new DeletableItem();
    listItem.__proto__ = CookieListItem.prototype;

    listItem.origin = origin;
    listItem.list = list;
    listItem.decorate();

    // This hooks up updateOrigin() to the list item, makes the top-level
    // tree nodes (i.e., origins) register their IDs in parentLookup, and
    // causes them to request their children if they have none. Note that we
    // have special logic in the setter for the parent property to make sure
    // that we can still garbage collect list items when they scroll out of
    // view, even though it appears that we keep a direct reference.
    if (origin) {
      origin.parent = listItem;
      origin.updateOrigin();
    }

    return listItem;
  }

  CookieListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      this.siteChild = this.ownerDocument.createElement('div');
      this.siteChild.className = 'cookie-site';
      this.dataChild = this.ownerDocument.createElement('div');
      this.dataChild.className = 'cookie-data';
      this.sizeChild = this.ownerDocument.createElement('div');
      this.sizeChild.className = 'cookie-size';
      this.itemsChild = this.ownerDocument.createElement('div');
      this.itemsChild.className = 'cookie-items';
      this.infoChild = this.ownerDocument.createElement('div');
      this.infoChild.className = 'cookie-details';
      this.infoChild.hidden = true;

      var remove = this.ownerDocument.createElement('button');
      remove.textContent = loadTimeData.getString('remove_cookie');
      remove.onclick = this.removeCookie_.bind(this);
      this.infoChild.appendChild(remove);
      var content = this.contentElement;
      content.appendChild(this.siteChild);
      content.appendChild(this.dataChild);
      content.appendChild(this.sizeChild);
      content.appendChild(this.itemsChild);
      this.itemsChild.appendChild(this.infoChild);
      if (this.origin && this.origin.data) {
        this.siteChild.textContent = this.origin.data.title;
        this.siteChild.setAttribute('title', this.origin.data.title);
      }
      this.itemList_ = [];
    },

    /** @type {boolean} */
    get expanded() {
      return this.expanded_;
    },
    set expanded(expanded) {
      if (this.expanded_ == expanded)
        return;
      this.expanded_ = expanded;
      if (expanded) {
        this.classList.add('show-items');
        var oldExpanded = this.list.expandedItem;
        this.list.expandedItem = this;
        this.updateItems_();
        if (oldExpanded)
          oldExpanded.expanded = false;
      } else {
        if (this.list.expandedItem == this) {
          this.list.expandedItem = null;
        }
        this.style.height = '';
        this.itemsChild.style.height = '';
        this.classList.remove('show-items');
      }
    },

    /**
     * The callback for the "remove" button shown when an item is selected.
     * Requests that the currently selected cookie be removed.
     * @private
     */
    removeCookie_: function() {
      if (this.selectedIndex_ >= 0) {
        var item = this.itemList_[this.selectedIndex_];
        if (item && item.node)
          chrome.send('removeCookie', [item.node.pathId]);
      }
    },

    /**
     * Disable animation within this cookie list item, in preparation for making
     * changes that will need to be animated. Makes it possible to measure the
     * contents without displaying them, to set animation targets.
     * @private
     */
    disableAnimation_: function() {
      this.itemsHeight_ = getItemHeight(this.itemsChild);
      this.classList.add('measure-items');
    },

    /**
     * Enable animation after changing the contents of this cookie list item.
     * See @{code disableAnimation_}.
     * @private
     */
    enableAnimation_: function() {
      if (!this.classList.contains('measure-items'))
        this.disableAnimation_();
      this.itemsChild.style.height = '';
      // This will force relayout in order to calculate the new heights.
      var itemsHeight = getItemHeight(this.itemsChild);
      var fixedHeight = getItemHeight(this) + itemsHeight - this.itemsHeight_;
      this.itemsChild.style.height = this.itemsHeight_ + 'px';
      // Force relayout before enabling animation, so that if we have
      // changed things since the last layout, they will not be animated
      // during subsequent layouts.
      /** @suppress {suspiciousCode} */
      this.itemsChild.offsetHeight;
      this.classList.remove('measure-items');
      this.itemsChild.style.height = itemsHeight + 'px';
      this.style.height = fixedHeight + 'px';
    },

    /**
     * Updates the origin summary to reflect changes in its items.
     * Both CookieListItem and CookieTreeNode implement this API.
     * This implementation scans the descendants to update the text.
     */
    updateOrigin: function() {
      var info = {
        cookies: 0,
        database: false,
        localStorage: false,
        appCache: false,
        indexedDb: false,
        fileSystem: false,
        channelIDs: 0,
        serviceWorker: false,
        cacheStorage: false,
      };
      if (this.origin)
        this.origin.collectSummaryInfo(info);

      var list = [];
      if (info.cookies > 1)
        list.push(loadTimeData.getStringF('cookie_plural', info.cookies));
      else if (info.cookies > 0)
        list.push(loadTimeData.getString('cookie_singular'));
      if (info.database || info.indexedDb)
        list.push(loadTimeData.getString('cookie_database_storage'));
      if (info.localStorage)
        list.push(loadTimeData.getString('cookie_local_storage'));
      if (info.appCache)
        list.push(loadTimeData.getString('cookie_app_cache'));
      if (info.fileSystem)
        list.push(loadTimeData.getString('cookie_file_system'));
      if (info.channelIDs)
        list.push(loadTimeData.getString('cookie_channel_id'));
      if (info.serviceWorker)
        list.push(loadTimeData.getString('cookie_service_worker'));
      if (info.cacheStorage)
        list.push(loadTimeData.getString('cookie_cache_storage'));
      if (info.flashLSO)
        list.push(loadTimeData.getString('cookie_flash_lso'));

      var text = '';
      for (var i = 0; i < list.length; ++i) {
        if (text.length > 0)
          text += ', ' + list[i];
        else
          text = list[i];
      }
      this.dataChild.textContent = text;

      var apps = info.appsProtectingThis;
      for (var key in apps) {
        addAppInfo(this.dataChild, apps[key]);
      }

      if (info.quota && info.quota.totalUsage)
        this.sizeChild.textContent = info.quota.totalUsage;

      if (this.expanded)
        this.updateItems_();
    },

    /**
     * Updates the items section to reflect changes, animating to the new state.
     * Removes existing contents and calls @{code CookieTreeNode.createItems}.
     * @private
     */
    updateItems_: function() {
      this.disableAnimation_();
      this.itemsChild.textContent = '';
      this.infoChild.hidden = true;
      this.selectedIndex_ = -1;
      this.itemList_ = [];
      if (this.origin)
        this.origin.createItems(this);
      this.itemsChild.appendChild(this.infoChild);
      this.enableAnimation_();
    },

    /**
     * Append a new cookie node "bubble" to this list item.
     * @param {options.CookieTreeNode} node The cookie node to add a bubble for.
     * @param {Element} div The DOM element for the bubble itself.
     * @return {number} The index the bubble was added at.
     */
    appendItem: function(node, div) {
      this.itemList_.push({node: node, div: div});
      this.itemsChild.appendChild(div);
      return this.itemList_.length - 1;
    },

    /**
     * The currently selected cookie node ("cookie bubble") index.
     * @type {number}
     * @private
     */
    selectedIndex_: -1,

    /**
     * Get the currently selected cookie node ("cookie bubble") index.
     * @type {number}
     */
    get selectedIndex() {
      return this.selectedIndex_;
    },

    /**
     * Set the currently selected cookie node ("cookie bubble") index to
     * |itemIndex|, unselecting any previously selected node first.
     * @param {number} itemIndex The index to set as the selected index.
     */
    set selectedIndex(itemIndex) {
      // Get the list index up front before we change anything.
      var index = this.list.getIndexOfListItem(this);
      // Unselect any previously selected item.
      if (this.selectedIndex_ >= 0) {
        var item = this.itemList_[this.selectedIndex_];
        if (item && item.div)
          item.div.removeAttribute('selected');
      }
      // Special case: decrementing -1 wraps around to the end of the list.
      if (itemIndex == -2)
        itemIndex = this.itemList_.length - 1;
      // Check if we're going out of bounds and hide the item details.
      if (itemIndex < 0 || itemIndex >= this.itemList_.length) {
        this.selectedIndex_ = -1;
        this.disableAnimation_();
        this.infoChild.hidden = true;
        this.enableAnimation_();
        return;
      }
      // Set the new selected item and show the item details for it.
      this.selectedIndex_ = itemIndex;
      this.itemList_[itemIndex].div.setAttribute('selected', '');
      this.disableAnimation_();
      this.itemList_[itemIndex].node.setDetailText(this.infoChild,
                                                   this.list.infoNodes);
      this.infoChild.hidden = false;
      this.enableAnimation_();
      // If we're near the bottom of the list this may cause the list item to go
      // beyond the end of the visible area. Fix it after the animation is done.
      var list = this.list;
      window.setTimeout(function() { list.scrollIndexIntoView(index); }, 150);
    },
  };

  /**
   * {@code CookieTreeNode}s mirror the structure of the cookie tree lazily, and
   * contain all the actual data used to generate the {@code CookieListItem}s.
   * @param {Object} data The data object for this node.
   * @constructor
   */
  function CookieTreeNode(data) {
    this.data = data;
    this.children = [];
  }

  CookieTreeNode.prototype = {
    /**
     * Insert the given list of cookie tree nodes at the given index.
     * Both CookiesList and CookieTreeNode implement this API.
     * @param {Array<Object>} data The data objects for the nodes to add.
     * @param {number} start The index at which to start inserting the nodes.
     */
    insertAt: function(data, start) {
      var nodes = spliceTreeNodes(data, start, this.children);
      for (var i = 0; i < nodes.length; i++)
        nodes[i].parent = this;
      this.updateOrigin();
    },

    /**
     * Remove a cookie tree node from the given index.
     * Both CookiesList and CookieTreeNode implement this API.
     *
     * TODO(dbeam): this method now conflicts with HTMLElement#remove(), which
     * is why the param is optional. Rename.
     *
     * @param {number=} index The index of the tree node to remove.
     */
    remove: function(index) {
      if (index < this.children.length) {
        this.children.splice(index, 1);
        this.updateOrigin();
      }
    },

    /**
     * Clears all children.
     * Both CookiesList and CookieTreeNode implement this API.
     * It is used by CookiesList.loadChildren().
     */
    clear: function() {
      // We might leave some garbage in parentLookup for removed children.
      // But that should be OK because parentLookup is cleared when we
      // reload the tree.
      this.children = [];
      this.updateOrigin();
    },

    /**
     * The counter used by startBatchUpdates() and endBatchUpdates().
     * @type {number}
     */
    batchCount_: 0,

    /**
     * See cr.ui.List.startBatchUpdates().
     * Both CookiesList (via List) and CookieTreeNode implement this API.
     */
    startBatchUpdates: function() {
      this.batchCount_++;
    },

    /**
     * See cr.ui.List.endBatchUpdates().
     * Both CookiesList (via List) and CookieTreeNode implement this API.
     */
    endBatchUpdates: function() {
      if (!--this.batchCount_)
        this.updateOrigin();
    },

    /**
     * Requests updating the origin summary to reflect changes in this item.
     * Both CookieListItem and CookieTreeNode implement this API.
     */
    updateOrigin: function() {
      if (!this.batchCount_ && this.parent)
        this.parent.updateOrigin();
    },

    /**
     * Summarize the information in this node and update @{code info}.
     * This will recurse into child nodes to summarize all descendants.
     * @param {Object} info The info object from @{code updateOrigin}.
     */
    collectSummaryInfo: function(info) {
      if (this.children.length > 0) {
        for (var i = 0; i < this.children.length; ++i)
          this.children[i].collectSummaryInfo(info);
      } else if (this.data && !this.data.hasChildren) {
        if (this.data.type == 'cookie') {
          info.cookies++;
        } else if (this.data.type == 'database') {
          info.database = true;
        } else if (this.data.type == 'local_storage') {
          info.localStorage = true;
        } else if (this.data.type == 'app_cache') {
          info.appCache = true;
        } else if (this.data.type == 'indexed_db') {
          info.indexedDb = true;
        } else if (this.data.type == 'file_system') {
          info.fileSystem = true;
        } else if (this.data.type == 'quota') {
          info.quota = this.data;
        } else if (this.data.type == 'channel_id') {
          info.channelIDs++;
        } else if (this.data.type == 'service_worker') {
          info.serviceWorker = true;
        } else if (this.data.type == 'cache_storage') {
          info.cacheStorage = true;
        } else if (this.data.type == 'flash_lso') {
          info.flashLSO = true;
        }

        var apps = this.data.appsProtectingThis;
        if (apps) {
          if (!info.appsProtectingThis)
            info.appsProtectingThis = {};
          apps.forEach(function(appInfo) {
            info.appsProtectingThis[appInfo.id] = appInfo;
          });
        }
      }
    },

    /**
     * Create the cookie "bubbles" for this node, recursing into children
     * if there are any. Append the cookie bubbles to @{code item}.
     * @param {options.CookieListItem} item The cookie list item to create items
     *     in.
     */
    createItems: function(item) {
      if (this.children.length > 0) {
        for (var i = 0; i < this.children.length; ++i)
          this.children[i].createItems(item);
        return;
      }

      if (!this.data || this.data.hasChildren)
        return;

      var text = '';
      switch (this.data.type) {
        case 'cookie':
        case 'database':
          text = this.data.name;
          break;
        default:
          text = loadTimeData.getString('cookie_' + this.data.type);
      }
      if (!text)
        return;

      var div = item.ownerDocument.createElement('div');
      div.className = 'cookie-item';
      // Help out screen readers and such: this is a clickable thing.
      div.setAttribute('role', 'button');
      div.textContent = text;
      var apps = this.data.appsProtectingThis;
      if (apps)
        apps.forEach(addAppInfo.bind(null, div));

      var index = item.appendItem(this, div);
      div.onclick = function() {
        item.selectedIndex = (item.selectedIndex == index) ? -1 : index;
      };
    },

    /**
     * Set the detail text to be displayed to that of this cookie tree node.
     * Uses preallocated DOM elements for each cookie node type from @{code
     * infoNodes}, and inserts the appropriate elements to @{code element}.
     * @param {Element} element The DOM element to insert elements to.
     * @param {Object<{table: Element, info: Object<Element>}>} infoNodes The
     *     map from cookie node types to maps from cookie attribute names to DOM
     *     elements to display cookie attribute values, created by
     *     @see {CookiesList.decorate}.
     */
    setDetailText: function(element, infoNodes) {
      var table;
      if (this.data && !this.data.hasChildren && cookieInfo[this.data.type]) {
        var info = cookieInfo[this.data.type];
        var nodes = infoNodes[this.data.type].info;
        for (var i = 0; i < info.length; ++i) {
          var name = info[i][0];
          if (name != 'id' && this.data[name])
            nodes[name].textContent = this.data[name];
          else
            nodes[name].textContent = '';
        }
        table = infoNodes[this.data.type].table;
      }

      while (element.childNodes.length > 1)
        element.removeChild(element.firstChild);

      if (table)
        element.insertBefore(table, element.firstChild);
    },

    /**
     * The parent of this cookie tree node.
     * @type {?(options.CookieTreeNode|options.CookieListItem)}
     */
    get parent() {
      // See below for an explanation of this special case.
      if (typeof this.parent_ == 'number')
        return this.list_.getListItemByIndex(this.parent_);
      return this.parent_;
    },
    set parent(parent) {
      if (parent == this.parent)
        return;

      if (parent instanceof CookieListItem) {
        // If the parent is to be a CookieListItem, then we keep the reference
        // to it by its containing list and list index, rather than directly.
        // This allows the list items to be garbage collected when they scroll
        // out of view (except the expanded item, which we cache). This is
        // transparent except in the setter and getter, where we handle it.
        if (this.parent_ == undefined || parent.listIndex != -1) {
          // Setting the parent is somewhat tricky because the CookieListItem
          // constructor has side-effects on the |origin| that it wraps. Every
          // time a CookieListItem is created for an |origin|, it registers
          // itself as the parent of the |origin|.
          // The List implementation may create a temporary CookieListItem item
          // that wraps the |origin| of the very first entry of the CokiesList,
          // when the List is redrawn the first time. This temporary
          // CookieListItem is fresh (has listIndex = -1) and is never inserted
          // into the List. Therefore it gets never updated. This destroys the
          // chain of parent pointers.
          // This is the stack trace:
          //     CookieListItem
          //     CookiesList.createItem
          //     List.measureItem
          //     List.getDefaultItemSize_
          //     List.getDefaultItemHeight_
          //     List.getIndexForListOffset_
          //     List.getItemsInViewPort
          //     List.redraw
          //     List.endBatchUpdates
          //     CookiesList.loadChildren
          this.parent_ = parent.listIndex;
        }
        this.list_ = parent.list;
        parent.addEventListener('listIndexChange',
                                this.parentIndexChanged_.bind(this));
      } else {
        this.parent_ = parent;
      }

      if (this.data && this.data.id) {
        if (parent)
          parentLookup[this.data.id] = this;
        else
          delete parentLookup[this.data.id];
      }

      if (this.data && this.data.hasChildren &&
          !this.children.length && !lookupRequests[this.data.id]) {
        lookupRequests[this.data.id] = true;
        chrome.send('loadCookie', [this.pathId]);
      }
    },

    /**
     * Called when the parent is a CookieListItem whose index has changed.
     * See the code above that avoids keeping a direct reference to
     * CookieListItem parents, to allow them to be garbage collected.
     * @private
     */
    parentIndexChanged_: function(event) {
      if (typeof this.parent_ == 'number') {
        this.parent_ = event.newValue;
        // We set a timeout to update the origin, rather than doing it right
        // away, because this callback may occur while the list items are
        // being repopulated following a scroll event. Calling updateOrigin()
        // immediately could trigger relayout that would reset the scroll
        // position within the list, among other things.
        window.setTimeout(this.updateOrigin.bind(this), 0);
      }
    },

    /**
     * The cookie tree path id.
     * @type {string}
     */
    get pathId() {
      var parent = this.parent;
      if (parent && parent instanceof CookieTreeNode)
        return parent.pathId + ',' + this.data.id;
      return this.data.id;
    },
  };

  /**
   * Creates a new cookies list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var CookiesList = cr.ui.define('list');

  CookiesList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.classList.add('cookie-list');
      this.dataModel = new ArrayDataModel([]);
      this.addEventListener('keydown', this.handleKeyLeftRight_.bind(this));
      var sm = new ListSingleSelectionModel();
      sm.addEventListener('change', this.cookieSelectionChange_.bind(this));
      sm.addEventListener('leadIndexChange', this.cookieLeadChange_.bind(this));
      this.selectionModel = sm;
      this.infoNodes = {};
      this.fixedHeight = false;
      var doc = this.ownerDocument;
      // Create a table for each type of site data (e.g. cookies, databases,
      // etc.) and save it so that we can reuse it for all origins.
      for (var type in cookieInfo) {
        var table = doc.createElement('table');
        table.className = 'cookie-details-table';
        var tbody = doc.createElement('tbody');
        table.appendChild(tbody);
        var info = {};
        for (var i = 0; i < cookieInfo[type].length; i++) {
          var tr = doc.createElement('tr');
          var name = doc.createElement('td');
          var data = doc.createElement('td');
          var pair = cookieInfo[type][i];
          name.className = 'cookie-details-label';
          name.textContent = loadTimeData.getString(pair[1]);
          data.className = 'cookie-details-value';
          data.textContent = '';
          tr.appendChild(name);
          tr.appendChild(data);
          tbody.appendChild(tr);
          info[pair[0]] = data;
        }
        this.infoNodes[type] = {table: table, info: info};
      }
    },

    /**
     * Handles key down events and looks for left and right arrows, then
     * dispatches to the currently expanded item, if any.
     * @param {Event} e The keydown event.
     * @private
     */
    handleKeyLeftRight_: function(e) {
      var id = e.key;
      if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)
        return;
      if ((id == 'ArrowLeft' || id == 'ArrowRight') && this.expandedItem) {
        var cs = this.ownerDocument.defaultView.getComputedStyle(this);
        var rtl = cs.direction == 'rtl';
        if ((!rtl && id == 'ArrowLeft') || (rtl && id == 'ArrowRight'))
          this.expandedItem.selectedIndex--;
        else
          this.expandedItem.selectedIndex++;
        this.scrollIndexIntoView(this.expandedItem.listIndex);
        // Prevent the page itself from scrolling.
        e.preventDefault();
      }
    },

    /**
     * Called on selection model selection changes.
     * @param {Event} ce The selection change event.
     * @private
     */
    cookieSelectionChange_: function(ce) {
      ce.changes.forEach(function(change) {
          var listItem = this.getListItemByIndex(change.index);
          if (listItem) {
            if (!change.selected) {
              // We set a timeout here, rather than setting the item unexpanded
              // immediately, so that if another item gets set expanded right
              // away, it will be expanded before this item is unexpanded. It
              // will notice that, and unexpand this item in sync with its own
              // expansion. Later, this callback will end up having no effect.
              window.setTimeout(function() {
                if (!listItem.selected || !listItem.lead)
                  listItem.expanded = false;
              }, 0);
            } else if (listItem.lead) {
              listItem.expanded = true;
            }
          }
        }, this);
    },

    /**
     * Called on selection model lead changes.
     * @param {Event} pe The lead change event.
     * @private
     */
    cookieLeadChange_: function(pe) {
      if (pe.oldValue != -1) {
        var listItem = this.getListItemByIndex(pe.oldValue);
        if (listItem) {
          // See cookieSelectionChange_ above for why we use a timeout here.
          window.setTimeout(function() {
            if (!listItem.lead || !listItem.selected)
              listItem.expanded = false;
          }, 0);
        }
      }
      if (pe.newValue != -1) {
        var listItem = this.getListItemByIndex(pe.newValue);
        if (listItem && listItem.selected)
          listItem.expanded = true;
      }
    },

    /**
     * The currently expanded item. Used by CookieListItem above.
     * @type {?options.CookieListItem}
     */
    expandedItem: null,

    // from cr.ui.List
    /**
     * @override
     * @param {Object} data
     */
    createItem: function(data) {
      // We use the cached expanded item in order to allow it to maintain some
      // state (like its fixed height, and which bubble is selected).
      if (this.expandedItem && this.expandedItem.origin == data)
        return this.expandedItem;
      return new CookieListItem(data, this);
    },

    // from options.DeletableItemList
    /** @override */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item) {
        var pathId = item.pathId;
        if (pathId)
          chrome.send('removeCookie', [pathId]);
      }
    },

    /**
     * Insert the given list of cookie tree nodes at the given index.
     * Both CookiesList and CookieTreeNode implement this API.
     * @param {Array<Object>} data The data objects for the nodes to add.
     * @param {number} start The index at which to start inserting the nodes.
     */
    insertAt: function(data, start) {
      spliceTreeNodes(data, start, this.dataModel);
    },

    /**
     * Remove a cookie tree node from the given index.
     * Both CookiesList and CookieTreeNode implement this API.
     *
     * TODO(dbeam): this method now conflicts with HTMLElement#remove(), which
     * is why the param is optional. Rename.
     *
     * @param {number=} index The index of the tree node to remove.
     */
    remove: function(index) {
      if (index < this.dataModel.length)
        this.dataModel.splice(index, 1);
    },

    /**
     * Clears the list.
     * Both CookiesList and CookieTreeNode implement this API.
     * It is used by CookiesList.loadChildren().
     */
    clear: function() {
      parentLookup = {};
      this.dataModel.splice(0, this.dataModel.length);
      this.redraw();
    },

    /**
     * Add tree nodes by given parent.
     * @param {Object} parent The parent node.
     * @param {number} start The index at which to start inserting the nodes.
     * @param {Array} nodesData Nodes data array.
     * @private
     */
    addByParent_: function(parent, start, nodesData) {
      if (!parent)
        return;

      parent.startBatchUpdates();
      parent.insertAt(nodesData, start);
      parent.endBatchUpdates();

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Add tree nodes by parent id.
     * This is used by cookies_view.js.
     * @param {string} parentId Id of the parent node.
     * @param {number} start The index at which to start inserting the nodes.
     * @param {Array} nodesData Nodes data array.
     */
    addByParentId: function(parentId, start, nodesData) {
      var parent = parentId ? parentLookup[parentId] : this;
      this.addByParent_(parent, start, nodesData);
    },

    /**
     * Removes tree nodes by parent id.
     * This is used by cookies_view.js.
     * @param {string} parentId Id of the parent node.
     * @param {number} start The index at which to start removing the nodes.
     * @param {number} count Number of nodes to remove.
     */
    removeByParentId: function(parentId, start, count) {
      var parent = parentId ? parentLookup[parentId] : this;
      if (!parent)
        return;

      parent.startBatchUpdates();
      while (count-- > 0)
        parent.remove(start);
      parent.endBatchUpdates();

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Loads the immediate children of given parent node.
     * This is used by cookies_view.js.
     * @param {string} parentId Id of the parent node.
     * @param {Array} children The immediate children of parent node.
     */
    loadChildren: function(parentId, children) {
      if (parentId)
        delete lookupRequests[parentId];
      var parent = parentId ? parentLookup[parentId] : this;
      if (!parent)
        return;

      parent.startBatchUpdates();
      parent.clear();
      this.addByParent_(parent, 0, children);
      parent.endBatchUpdates();
    },
  };

  return {
    CookiesList: CookiesList,
    CookieListItem: CookieListItem,
    CookieTreeNode: CookieTreeNode,
  };
});
