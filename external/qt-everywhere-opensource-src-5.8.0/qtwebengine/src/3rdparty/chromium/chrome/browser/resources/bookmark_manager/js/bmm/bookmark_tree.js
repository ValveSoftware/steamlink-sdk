// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('bmm', function() {
  'use strict';

  /**
   * The id of the bookmark root.
   * @type {string}
   * @const
   */
  var ROOT_ID = '0';

  /** @const */ var Tree = cr.ui.Tree;
  /** @const */ var TreeItem = cr.ui.TreeItem;
  /** @const */ var localStorage = window.localStorage;

  var treeLookup = {};

  // Manager for persisting the expanded state.
  var expandedManager = /** @type {EventListener} */({
    /**
     * A map of the collapsed IDs.
     * @type {Object}
     */
    map: 'bookmarkTreeState' in localStorage ?
        /** @type {Object} */(JSON.parse(localStorage['bookmarkTreeState'])) :
        {},

    /**
     * Set the collapsed state for an ID.
     * @param {string} id The bookmark ID of the tree item that was expanded or
     *     collapsed.
     * @param {boolean} expanded Whether the tree item was expanded.
     */
    set: function(id, expanded) {
      if (expanded)
        delete this.map[id];
      else
        this.map[id] = 1;

      this.save();
    },

    /**
     * @param {string} id The bookmark ID.
     * @return {boolean} Whether the tree item should be expanded.
     */
    get: function(id) {
      return !(id in this.map);
    },

    /**
     * Callback for the expand and collapse events from the tree.
     * @param {!Event} e The collapse or expand event.
     */
    handleEvent: function(e) {
      this.set(e.target.bookmarkId, e.type == 'expand');
    },

    /**
     * Cleans up old bookmark IDs.
     */
    cleanUp: function() {
      for (var id in this.map) {
        // If the id is no longer in the treeLookup the bookmark no longer
        // exists.
        if (!(id in treeLookup))
          delete this.map[id];
      }
      this.save();
    },

    timer: null,

    /**
     * Saves the expanded state to the localStorage.
     */
    save: function() {
      clearTimeout(this.timer);
      var map = this.map;
      // Save in a timeout so that we can coalesce multiple changes.
      this.timer = setTimeout(function() {
        localStorage['bookmarkTreeState'] = JSON.stringify(map);
      }, 100);
    }
  });

  // Clean up once per session but wait until things settle down a bit.
  setTimeout(expandedManager.cleanUp.bind(expandedManager), 1e4);

  /**
   * Creates a new tree item for a bookmark node.
   * @param {!Object} bookmarkNode The bookmark node.
   * @constructor
   * @extends {TreeItem}
   */
  function BookmarkTreeItem(bookmarkNode) {
    var ti = new TreeItem({
      label: bookmarkNode.title,
      bookmarkNode: bookmarkNode,
      // Bookmark toolbar and Other bookmarks are not draggable.
      draggable: bookmarkNode.parentId != ROOT_ID
    });
    ti.__proto__ = BookmarkTreeItem.prototype;
    treeLookup[bookmarkNode.id] = ti;
    return ti;
  }

  BookmarkTreeItem.prototype = {
    __proto__: TreeItem.prototype,

    /**
     * The ID of the bookmark this tree item represents.
     * @type {string}
     */
    get bookmarkId() {
      return this.bookmarkNode.id;
    }
  };

  /**
   * Asynchronousy adds a tree item at the correct index based on the bookmark
   * backend.
   *
   * Since the bookmark tree only contains folders the index we get from certain
   * callbacks is not very useful so we therefore have this async call which
   * gets the children of the parent and adds the tree item at the desired
   * index.
   *
   * This also exoands the parent so that newly added children are revealed.
   *
   * @param {!cr.ui.TreeItem} parent The parent tree item.
   * @param {!cr.ui.TreeItem} treeItem The tree item to add.
   * @param {Function=} opt_f A function which gets called after the item has
   *     been added at the right index.
   */
  function addTreeItem(parent, treeItem, opt_f) {
    chrome.bookmarks.getChildren(parent.bookmarkNode.id, function(children) {
      var isFolder = /**
                      * @type {function (BookmarkTreeNode, number,
                      *                  Array<(BookmarkTreeNode)>)}
                      */(bmm.isFolder);
      var index = children.filter(isFolder).map(function(item) {
        return item.id;
      }).indexOf(treeItem.bookmarkNode.id);
      parent.addAt(treeItem, index);
      parent.expanded = true;
      if (opt_f)
        opt_f();
    });
  }


  /**
   * Creates a new bookmark list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Tree}
   */
  var BookmarkTree = cr.ui.define('tree');

  BookmarkTree.prototype = {
    __proto__: Tree.prototype,

    decorate: function() {
      Tree.prototype.decorate.call(this);
      this.addEventListener('expand', expandedManager);
      this.addEventListener('collapse', expandedManager);

      bmm.tree = this;
    },

    handleBookmarkChanged: function(id, changeInfo) {
      var treeItem = treeLookup[id];
      if (treeItem)
        treeItem.label = treeItem.bookmarkNode.title = changeInfo.title;
    },

    /**
     * @param {string} id
     * @param {ReorderInfo} reorderInfo
     */
    handleChildrenReordered: function(id, reorderInfo) {
      var parentItem = treeLookup[id];
      // The tree only contains folders.
      var dirIds = reorderInfo.childIds.filter(function(id) {
        return id in treeLookup;
      }).forEach(function(id, i) {
        parentItem.addAt(treeLookup[id], i);
      });
    },

    handleCreated: function(id, bookmarkNode) {
      if (bmm.isFolder(bookmarkNode)) {
        var parentItem = treeLookup[bookmarkNode.parentId];
        var newItem = new BookmarkTreeItem(bookmarkNode);
        addTreeItem(parentItem, newItem);
      }
    },

    /**
     * @param {string} id
     * @param {MoveInfo} moveInfo
     */
    handleMoved: function(id, moveInfo) {
      var treeItem = treeLookup[id];
      if (treeItem) {
        var oldParentItem = treeLookup[moveInfo.oldParentId];
        oldParentItem.remove(treeItem);
        var newParentItem = treeLookup[moveInfo.parentId];
        // The tree only shows folders so the index is not the index we want. We
        // therefore get the children need to adjust the index.
        addTreeItem(newParentItem, treeItem);
      }
    },

    handleRemoved: function(id, removeInfo) {
      var parentItem = treeLookup[removeInfo.parentId];
      var itemToRemove = treeLookup[id];
      if (parentItem && itemToRemove)
        parentItem.remove(itemToRemove);
    },

    insertSubtree: function(folder) {
      if (!bmm.isFolder(folder))
        return;
      var children = folder.children;
      this.handleCreated(folder.id, folder);
      for (var i = 0; i < children.length; i++) {
        var child = children[i];
        this.insertSubtree(child);
      }
    },

    /**
     * Returns the bookmark node with the given ID. The tree only maintains
     * folder nodes.
     * @param {string} id The ID of the node to find.
     * @return {BookmarkTreeNode} The bookmark tree node or null if not found.
     */
    getBookmarkNodeById: function(id) {
      var treeItem = treeLookup[id];
      if (treeItem)
        return treeItem.bookmarkNode;
      return null;
    },

    /**
      * Returns the selected bookmark folder node as an array.
      * @type {!Array} Array of bookmark nodes.
      */
    get selectedFolders() {
       return this.selectedItem && this.selectedItem.bookmarkNode ?
           [this.selectedItem.bookmarkNode] : [];
     },

     /**
     * Fetches the bookmark items and builds the tree control.
     */
    reload: function() {
      /**
       * Recursive helper function that adds all the directories to the
       * parentTreeItem.
       * @param {!cr.ui.Tree|!cr.ui.TreeItem} parentTreeItem The parent tree
       *     element to append to.
       * @param {!Array<BookmarkTreeNode>} bookmarkNodes A list of bookmark
       *     nodes to be added.
       * @return {boolean} Whether any directories where added.
       */
      function buildTreeItems(parentTreeItem, bookmarkNodes) {
        var hasDirectories = false;
        for (var i = 0, bookmarkNode; bookmarkNode = bookmarkNodes[i]; i++) {
          if (bmm.isFolder(bookmarkNode)) {
            hasDirectories = true;
            var item = new BookmarkTreeItem(bookmarkNode);
            parentTreeItem.add(item);
            var children = assert(bookmarkNode.children);
            var anyChildren = buildTreeItems(item, children);
            item.expanded = anyChildren && expandedManager.get(bookmarkNode.id);
          }
        }
        return hasDirectories;
      }

      var self = this;
      chrome.bookmarkManagerPrivate.getSubtree('', true, function(root) {
        self.clear();
        buildTreeItems(self, root[0].children);
        cr.dispatchSimpleEvent(self, 'load');
      });
    },

    /**
     * Clears the tree.
     */
    clear: function() {
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in treeLookup) {
        delete treeLookup[id];
      }
      this.textContent = '';
    },

    /** @override */
    remove: function(child) {
      Tree.prototype.remove.call(this, child);
      if (child.bookmarkNode)
        delete treeLookup[child.bookmarkNode.id];
    }
  };

  return {
    BookmarkTree: BookmarkTree,
    BookmarkTreeItem: BookmarkTreeItem,
    treeLookup: treeLookup,
    tree: /** @type {Element} */(null),  // Set when decorated.
    ROOT_ID: ROOT_ID
  };
});
