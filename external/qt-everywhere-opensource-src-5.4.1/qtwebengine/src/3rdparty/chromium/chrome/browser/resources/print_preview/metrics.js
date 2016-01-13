// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to measure usage statistics.
   * @constructor
   */
  function Metrics() {};

  /**
   * Enumeration of metrics bucket groups. Each group describes a set of events
   * that can happen in order. This implies that an event cannot occur twice and
   * an event that occurs semantically before another event, should not occur
   * after.
   * @enum {number}
   */
  Metrics.BucketGroup = {
    DESTINATION_SEARCH: 0,
    GCP_PROMO: 1
  };

  /**
   * Enumeration of buckets that a user can enter while using the destination
   * search widget.
   * @enum {number}
   */
  Metrics.DestinationSearchBucket = {
    // Used when the print destination search widget is shown.
    SHOWN: 0,
    // Used when the user selects a print destination.
    DESTINATION_SELECTED: 1,
    // Used when the print destination search widget is closed without selecting
    // a print destination.
    CANCELED: 2,
    // Used when the Google Cloud Print promotion (shown in the destination
    // search widget) is shown to the user.
    CLOUDPRINT_PROMO_SHOWN: 3,
    // Used when the user chooses to sign-in to their Google account.
    SIGNIN_TRIGGERED: 4,
    // Used when a user selects the privet printer in a pair of duplicate
    // privet and cloud printers.
    PRIVET_DUPLICATE_SELECTED: 5,
    // Used when a user selects the cloud printer in a pair of duplicate
    // privet and cloud printers.
    CLOUD_DUPLICATE_SELECTED: 6,
    // Used when a user sees a register promo for a cloud print printer.
    REGISTER_PROMO_SHOWN: 7,
    // Used when a user selects a register promo for a cloud print printer.
    REGISTER_PROMO_SELECTED: 8,
    // User changed active account.
    ACCOUNT_CHANGED: 9,
    // User tried to log into another account.
    ADD_ACCOUNT_SELECTED: 10
  };

  /**
   * Enumeration of buckets that a user can enter while using the Google Cloud
   * Print promotion.
   * @enum {number}
   */
  Metrics.GcpPromoBucket = {
    // Used when the Google Cloud Print pomotion (shown above the pdf preview
    // plugin) is shown to the user.
    SHOWN: 0,
    // Used when the user clicks the "Get started" link in the promotion shown
    // in CLOUDPRINT_BIG_PROMO_SHOWN.
    CLICKED: 1,
    // Used when the user dismisses the promotion shown in
    // CLOUDPRINT_BIG_PROMO_SHOWN.
    DISMISSED: 2
  };

  /**
   * Name of the C++ function to call to increment bucket counts.
   * @type {string}
   * @const
   * @private
   */
  Metrics.NATIVE_FUNCTION_NAME_ = 'reportUiEvent';

  Metrics.prototype = {
    /**
     * Increments the counter of a destination-search bucket.
     * @param {!Metrics.DestinationSearchBucket} bucket Bucket to increment.
     */
    incrementDestinationSearchBucket: function(bucket) {
      chrome.send(Metrics.NATIVE_FUNCTION_NAME_,
                  [Metrics.BucketGroup.DESTINATION_SEARCH, bucket]);
    },

    /**
     * Increments the counter of a gcp-promo bucket.
     * @param {!Metrics.GcpPromoBucket} bucket Bucket to increment.
     */
    incrementGcpPromoBucket: function(bucket) {
      chrome.send(Metrics.NATIVE_FUNCTION_NAME_,
                  [Metrics.BucketGroup.GCP_PROMO, bucket]);
    }
  };

  // Export
  return {
    Metrics: Metrics
  };
});
