// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Sub-class of a destination list that shows cloud-based destinations.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @constructor
   * @extends {print_preview.DestinationList}
   */
  function CloudDestinationList(eventTarget) {
    print_preview.DestinationList.call(
        this,
        eventTarget,
        localStrings.getString('cloudDestinationsTitle'),
        localStrings.getString('manage'));
  };

  CloudDestinationList.prototype = {
    __proto__: print_preview.DestinationList.prototype,

    /** @override */
    updateDestinations: function(destinations) {
      // Change the action link from "Manage..." to "Setup..." if user only has
      // Docs and FedEx printers.
      var docsId = print_preview.Destination.GooglePromotedId.DOCS;
      var fedexId = print_preview.Destination.GooglePromotedId.FEDEX;
      if ((destinations.length == 1 && destinations[0].id == docsId) ||
          (destinations.length == 2 &&
           ((destinations[0].id == docsId && destinations[1].id == fedexId) ||
            (destinations[0].id == fedexId && destinations[1].id == docsId)))) {
        this.setActionLinkTextInternal(
            localStrings.getString('setupCloudPrinters'));
      } else {
        this.setActionLinkTextInternal(localStrings.getString('manage'));
      }
      print_preview.DestinationList.prototype.updateDestinations.call(
          this, destinations);
    }
  };

  return {
    CloudDestinationList: CloudDestinationList
  };
});
