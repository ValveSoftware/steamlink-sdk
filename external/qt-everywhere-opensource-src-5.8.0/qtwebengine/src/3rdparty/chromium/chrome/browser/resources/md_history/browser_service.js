// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a singleton object, md_history.BrowserService, which
 * provides access to chrome.send APIs.
 */

cr.define('md_history', function() {
  /** @constructor */
  function BrowserService() {
    /** @private {Array<!HistoryEntry>} */
    this.pendingDeleteItems_ = null;
    /** @private {PromiseResolver} */
    this.pendingDeletePromise_ = null;
  }

  BrowserService.prototype = {
    /**
     * @param {!Array<!HistoryEntry>} items
     * @return {Promise<!Array<!HistoryEntry>>}
     */
    deleteItems: function(items) {
      if (this.pendingDeleteItems_ != null) {
        // There's already a deletion in progress, reject immediately.
        return new Promise(function(resolve, reject) { reject(items); });
      }

      var removalList = items.map(function(item) {
        return {
          url: item.url,
          timestamps: item.allTimestamps
        };
      });

      this.pendingDeleteItems_ = items;
      this.pendingDeletePromise_ = new PromiseResolver();

      chrome.send('removeVisits', removalList);

      return this.pendingDeletePromise_.promise;
    },

    /**
     * @param {boolean} successful
     * @private
     */
    resolveDelete_: function(successful) {
      if (this.pendingDeleteItems_ == null ||
          this.pendingDeletePromise_ == null) {
        return;
      }

      if (successful)
        this.pendingDeletePromise_.resolve(this.pendingDeleteItems_);
      else
        this.pendingDeletePromise_.reject(this.pendingDeleteItems_);

      this.pendingDeleteItems_ = null;
      this.pendingDeletePromise_ = null;
    },
  };

  cr.addSingletonGetter(BrowserService);

  return {BrowserService: BrowserService};
});

/**
 * Called by the history backend when deletion was succesful.
 */
function deleteComplete() {
  md_history.BrowserService.getInstance().resolveDelete_(true);
}

/**
 * Called by the history backend when the deletion failed.
 */
function deleteFailed() {
  md_history.BrowserService.getInstance().resolveDelete_(false);
}

