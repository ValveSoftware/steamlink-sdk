// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @param {string} currentPath
 */
var SelectionTreeNode = function(currentPath) {
  /** @type {string} */
  this.currentPath = currentPath;
  /** @type {boolean} */
  this.leaf = false;
  /** @type {Array<number>} */
  this.indexes = [];
  /** @type {Array<SelectionTreeNode>} */
  this.children = [];
};

/**
 * @param {number} index
 * @param {string} path
 */
SelectionTreeNode.prototype.addChild = function(index, path) {
  this.indexes.push(index);
  this.children[index] = new SelectionTreeNode(path);
};

/** @polymerBehavior */
var HistoryListBehavior = {
  properties: {
    /**
     * Polymer paths to the history items contained in this list.
     * @type {!Set<string>} selectedPaths
     */
    selectedPaths: {
      type: Object,
      value: /** @return {!Set<string>} */ function() { return new Set(); }
    },

    lastSelectedPath: String,
  },

  listeners: {
    'history-checkbox-select': 'itemSelected_',
  },

  /**
   * @param {number} historyDataLength
   * @return {boolean}
   * @private
   */
  hasResults: function(historyDataLength) { return historyDataLength > 0; },

  /**
   * @param {string} searchedTerm
   * @param {boolean} isLoading
   * @return {string}
   * @private
   */
  noResultsMessage: function(searchedTerm, isLoading) {
    if (isLoading)
      return '';

    var messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },

  /**
   * Deselect each item in |selectedPaths|.
   */
  unselectAllItems: function() {
    this.selectedPaths.forEach(function(path) {
      this.set(path + '.selected', false);
    }.bind(this));

    this.selectedPaths.clear();
  },

  /**
   * Performs a request to the backend to delete all selected items. If
   * successful, removes them from the view. Does not prompt the user before
   * deleting -- see <history-list-container> for a version of this method which
   * does prompt.
   */
  deleteSelected: function() {
    var toBeRemoved =
        Array.from(this.selectedPaths.values()).map(function(path) {
          return this.get(path);
        }.bind(this));

    md_history.BrowserService.getInstance()
        .deleteItems(toBeRemoved)
        .then(function() {
          this.removeItemsByPath(Array.from(this.selectedPaths));
          this.fire('unselect-all');
        }.bind(this));
  },

  /**
   * Removes the history items in |paths|. Assumes paths are of a.0.b.0...
   * structure.
   *
   * We want to use notifySplices to update the arrays for performance reasons
   * which requires manually batching and sending the notifySplices for each
   * level. To do this, we build a tree where each node is an array and then
   * depth traverse it to remove items. Each time a node has all children
   * deleted, we can also remove the node.
   *
   * @param {Array<string>} paths
   * @private
   */
  removeItemsByPath: function(paths) {
    if (paths.length == 0)
      return;

    this.removeItemsBeneathNode_(this.buildRemovalTree_(paths));
  },

  /**
   * Creates the tree to traverse in order to remove |paths| from this list.
   * Assumes paths are of a.0.b.0...
   * structure.
   *
   * @param {Array<string>} paths
   * @return {SelectionTreeNode}
   * @private
   */
  buildRemovalTree_: function(paths) {
    var rootNode = new SelectionTreeNode(paths[0].split('.')[0]);

    // Build a tree to each history item specified in |paths|.
    paths.forEach(function(path) {
      var components = path.split('.');
      var node = rootNode;
      components.shift();
      while (components.length > 1) {
        var index = Number(components.shift());
        var arrayName = components.shift();

        if (!node.children[index])
          node.addChild(index, [node.currentPath, index, arrayName].join('.'));

        node = node.children[index];
      }
      node.leaf = true;
      node.indexes.push(Number(components.shift()));
    });

    return rootNode;
  },

  /**
   * Removes the history items underneath |node| and deletes container arrays as
   * they become empty.
   * @param {SelectionTreeNode} node
   * @return {boolean} Whether this node's array should be deleted.
   * @private
   */
  removeItemsBeneathNode_: function(node) {
    var array = this.get(node.currentPath);
    var splices = [];

    node.indexes.sort(function(a, b) { return b - a; });
    node.indexes.forEach(function(index) {
      if (node.leaf || this.removeItemsBeneathNode_(node.children[index])) {
        var item = array.splice(index, 1)[0];
        splices.push({
          index: index,
          removed: [item],
          addedCount: 0,
          object: array,
          type: 'splice'
        });
      }
    }.bind(this));

    if (array.length == 0 && node.currentPath.indexOf('.') != -1)
      return true;

    // notifySplices gives better performance than individually splicing as it
    // batches all of the updates together.
    this.notifySplices(node.currentPath, splices);
    return false;
  },

  /**
   * @param {Event} e
   * @private
   */
  itemSelected_: function(e) {
    var item = e.detail.element;
    var paths = [];
    var itemPath = item.path;

    // Handle shift selection. Change the selection state of all items between
    // |path| and |lastSelected| to the selection state of |item|.
    if (e.detail.shiftKey && this.lastSelectedPath) {
      var itemPathComponents = itemPath.split('.');
      var itemIndex = Number(itemPathComponents.pop());
      var itemArrayPath = itemPathComponents.join('.');

      var lastItemPathComponents = this.lastSelectedPath.split('.');
      var lastItemIndex = Number(lastItemPathComponents.pop());
      if (itemArrayPath == lastItemPathComponents.join('.')) {
        for (var i = Math.min(itemIndex, lastItemIndex);
             i <= Math.max(itemIndex, lastItemIndex); i++) {
          paths.push(itemArrayPath + '.' + i);
        }
      }
    }

    if (paths.length == 0)
      paths.push(item.path);

    var selected = !this.selectedPaths.has(item.path);

    paths.forEach(function(path) {
      this.set(path + '.selected', selected);

      if (selected) {
        this.selectedPaths.add(path);
        return;
      }

      this.selectedPaths.delete(path);
    }.bind(this));

    this.lastSelectedPath = itemPath;
  },
};
