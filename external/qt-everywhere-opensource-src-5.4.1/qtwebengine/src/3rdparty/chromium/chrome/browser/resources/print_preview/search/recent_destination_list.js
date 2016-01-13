// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Sub-class of a destination list that shows recent destinations. This list
   * does not render a "Show all" button.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @constructor
   * @extends {print_preview.DestinationList}
   */
  function RecentDestinationList(eventTarget) {
    print_preview.DestinationList.call(
        this,
        eventTarget,
        localStrings.getString('recentDestinationsTitle'));
  };

  RecentDestinationList.prototype = {
    __proto__: print_preview.DestinationList.prototype,

    /** @override */
    updateShortListSize: function(size) {
      this.setShortListSizeInternal(
          Math.max(1, Math.min(size, this.getDestinationsCount())));
    },

    /** @override */
    renderListInternal: function(destinations) {
      setIsVisible(this.getChildElement('.no-destinations-message'),
                   destinations.length == 0);
      setIsVisible(this.getChildElement('.destination-list > footer'), false);
      var numItems = Math.min(destinations.length, this.shortListSize_);
      for (var i = 0; i < numItems; i++) {
        var destListItem = new print_preview.DestinationListItem(
            this.eventTarget_, destinations[i]);
        this.addChild(destListItem);
        destListItem.render(this.getChildElement('.destination-list > ul'));
      }
    }
  };

  return {
    RecentDestinationList: RecentDestinationList
  };
});
