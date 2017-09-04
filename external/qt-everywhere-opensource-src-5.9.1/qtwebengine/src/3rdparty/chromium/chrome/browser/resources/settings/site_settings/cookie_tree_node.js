// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{title: string,
 *            id: string,
 *            data: CookieDetails}}
 */
var CookieDataItem;

/**
 * @typedef {{site: string,
 *            id: string,
 *            localData: string}}
 */
var CookieDataSummaryItem;

/**
 * @typedef {{id: string,
 *            start: number,
 *            children: !Array<CookieDetails>}}
 */
var CookieList;

/**
 * @typedef {{id: string,
 *            start: number,
 *            count: number}}
 */
var CookieRemovePacket;

var categoryLabels = {
  'app_cache': loadTimeData.getString('cookieAppCache'),
  'cache_storage': loadTimeData.getString('cookieCacheStorage'),
  'channel_id': loadTimeData.getString('cookieChannelId'),
  'cookie': loadTimeData.getString('cookieSingular'),
  'database': loadTimeData.getString('cookieDatabaseStorage'),
  'file_system': loadTimeData.getString('cookieFileSystem'),
  'flash_lso': loadTimeData.getString('cookieFlashLso'),
  'indexed_db': loadTimeData.getString('cookieDatabaseStorage'),
  'local_storage': loadTimeData.getString('cookieLocalStorage'),
  'service_worker': loadTimeData.getString('cookieServiceWorker'),
  'media_license': loadTimeData.getString('cookieMediaLicense'),
};

/**
 * Retrieves the human friendly text to show for the type of cookie.
 * @param {string} dataType The datatype to look up.
 * @param {string} totalUsage How much data is being consumed.
 * @return {string} The human-friendly description for this cookie.
 */
function getCookieDataCategoryText(dataType, totalUsage) {
  if (dataType == 'quota')
    return totalUsage;
  return categoryLabels[dataType];
}

cr.define('settings', function() {
  'use strict';

  /**
   * @constructor
   */
  function CookieTreeNode(data) {
    /**
     * The data for this cookie node.
     * @type {CookieDetails}
     */
    this.data = data;

    /**
     * The child cookie nodes.
     * @private {!Array<!settings.CookieTreeNode>}
     */
    this.children_ = [];
  };

  CookieTreeNode.prototype = {
    /**
     * Converts a list of cookies and add them as CookieTreeNode children to
     * the given parent node.
     * @param {!settings.CookieTreeNode} parentNode The parent node to add
     *     children to.
     * @param {!Array<!CookieDetails>} newNodes The list containing the data to
     *     add.
     */
    addChildNodes: function(parentNode, newNodes) {
      var nodes = newNodes.map(function(x) {
        return new settings.CookieTreeNode(x);
      });
      parentNode.children_ = nodes;
    },

    /**
     * Looks up a parent node and adds a list of CookieTreeNodes to them.
     * @param {string} parentId The ID of the parent to add the nodes to.
     * @param {!settings.CookieTreeNode} startingNode The node to start with
     *     when looking for the parent node to add the children to.
     * @param {!Array<!CookieDetails>} newNodes The list containing the data to
         add.
     * @return {boolean} True if the parent node was found.
     */
    populateChildNodes: function(parentId, startingNode, newNodes) {
      for (var i = 0; i < startingNode.children_.length; ++i) {
        if (startingNode.children_[i].data.id == parentId) {
          this.addChildNodes(startingNode.children_[i], newNodes);
          return true;
        }

        if (this.populateChildNodes(
            parentId, startingNode.children_[i], newNodes)) {
          return true;
        }
      }
      return false;
    },

    /**
     * Removes child nodes from a node with a given id.
     * @param {string} id The id of the parent node to delete from.
     * @param {number} firstChild The index of the first child to start deleting
     *     from.
     * @param {number} count The number of children to delete.
     */
    removeByParentId: function(id, firstChild, count) {
      var node = id == null ? this : this.fetchNodeById(id, true);
      node.children_.splice(firstChild, count);
    },

    /**
     * Returns an array of cookies from the current node within the cookie tree.
     * @return {!Array<!CookieDataItem>} The Cookie List.
     */
    getCookieList: function() {
      var list = [];
      for (var child of this.children_) {
        for (var cookie of child.children_) {
          list.push({title: cookie.data.title,
                     id: cookie.data.id,
                     data: cookie.data});
        }
      }

      return list;
    },

    /**
     * Get a summary list of all sites and their stored data.
     * @return {!Array<!CookieDataSummaryItem>} The summary list.
     */
    getSummaryList: function() {
      var list = [];
      for (var i = 0; i < this.children_.length; ++i) {
        var siteEntry = this.children_[i];
        var title = siteEntry.data.title;
        var id = siteEntry.data.id;
        var description = '';

        if (siteEntry.children_.length == 0)
          continue;

        for (var j = 0; j < siteEntry.children_.length; ++j) {
          var descriptionNode = siteEntry.children_[j];
          if (j > 0)
            description += ', ';

          // Some types, like quota, have no description nodes.
          var dataType = '';
          if (descriptionNode.data.type != undefined) {
            dataType = descriptionNode.data.type;
          } else {
            // A description node might not have children when it's deleted.
            if (descriptionNode.children_.length > 0)
              dataType = descriptionNode.children_[0].data.type;
          }

          var count =
              (dataType == 'cookie') ? descriptionNode.children_.length : 0;
          if (count > 1) {
            description += loadTimeData.getStringF('cookiePlural', count);
          } else {
            description += getCookieDataCategoryText(
                dataType, descriptionNode.data.totalUsage);
          }
        }
        list.push({ site: title, id: id, localData: description });
      }
      list.sort(function(a, b) {
        return a.site.localeCompare(b.site);
      });
      return list;
    },

    /**
     * Fetch a CookieTreeNode by ID.
     * @param {string} id The ID to look up.
     * @param {boolean} recursive Whether to search the children also.
     * @return {settings.CookieTreeNode} The node found, if any.
     */
    fetchNodeById: function(id, recursive) {
      for (var i = 0; i < this.children_.length; ++i) {
        if (this.children_[i] == null)
          return null;
        if (this.children_[i].data.id == id)
          return this.children_[i];
        if (recursive) {
          var node = this.children_[i].fetchNodeById(id, true);
          if (node != null)
            return node;
        }
      }
      return null;
    },

    /**
     * Fetch a CookieTreeNode by site.
     * @param {string} site The web site to look up.
     * @return {?settings.CookieTreeNode} The node found, if any.
     */
    fetchNodeBySite: function(site) {
      for (var i = 0; i < this.children_.length; ++i) {
        if (this.children_[i].data.title == site)
          return this.children_[i];
      }
      return null;
    },
  };

  return {
    CookieTreeNode: CookieTreeNode,
  };
});
