// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A behavior for managing a tree of cookies.
 */

/** @polymerBehavior */
var CookieTreeBehaviorImpl = {
  properties: {
    /**
     * A summary list of all sites and how many entities each contain.
     * @type {!Array<!CookieDataSummaryItem>}
     */
    sites: Array,

    /**
     * The cookie tree with the details needed to display individual sites and
     * their contained data.
     * @type {!settings.CookieTreeNode}
     * @protected
     */
    rootCookieNode: Object,
  },

  /** @override */
  ready: function() {
    cr.addWebUIListener('onTreeItemRemoved',
                        this.onTreeItemRemoved_.bind(this));
    this.rootCookieNode = new settings.CookieTreeNode(null);
  },

  /**
   * Called when the cookie list is ready to be shown.
   * @param {!CookieList} list The cookie list to show.
   * @return {Promise}
   * @private
   */
  loadChildren_: function(list) {
    var loadChildrenRecurse = function(childList) {
      var parentId = childList.id;
      var children = childList.children;
      var prefix = '';
      if (parentId !== null) {
        this.rootCookieNode.populateChildNodes(parentId,
            this.rootCookieNode, children);
        prefix = parentId + ', ';
      }
      var promises = [];
      for (let child of children) {
        if (child.hasChildren) {
          promises.push(this.browserProxy.loadCookieChildren(
              prefix + child.id).then(loadChildrenRecurse.bind(this)));
        }
      }
      return Promise.all(promises);
    }.bind(this);

    // New root being added, clear the list and add the nodes.
    this.sites = [];
    this.rootCookieNode.addChildNodes(this.rootCookieNode, list.children);
    return loadChildrenRecurse(list).then(function() {
      this.sites = this.rootCookieNode.getSummaryList();
      return Promise.resolve();
    }.bind(this));
  },

  /**
   * Loads (or reloads) the whole cookie list.
   * @return {Promise}
   */
  loadCookies: function() {
    return this.browserProxy.reloadCookies().then(
        this.loadChildren_.bind(this));
  },

  /**
   * Called when a single item has been removed (not during delete all).
   * @param {!CookieRemovePacket} args The details about what to remove.
   * @private
   */
  onTreeItemRemoved_: function(args) {
    this.rootCookieNode.removeByParentId(args.id, args.start, args.count);
    this.sites = this.rootCookieNode.getSummaryList();
  },

  /**
   * Deletes site data for multiple sites.
   * @return {Promise}
   */
  removeAllCookies: function() {
    return this.browserProxy.removeAllCookies().then(
        this.loadChildren_.bind(this));
  },
};

/** @polymerBehavior */
var CookieTreeBehavior = [SiteSettingsBehavior, CookieTreeBehaviorImpl];
