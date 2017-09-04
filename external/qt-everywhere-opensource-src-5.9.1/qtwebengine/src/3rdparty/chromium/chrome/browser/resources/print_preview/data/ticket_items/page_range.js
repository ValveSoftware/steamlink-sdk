// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Page range ticket item whose value is a {@code string} that represents
   * which pages in the document should be printed.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function PageRange(documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        null /*appState*/,
        null /*field*/,
        null /*destinationStore*/,
        documentInfo);
  };

  /**
   * Impossibly large page number.
   * @type {number}
   * @const
   * @private
   */
  PageRange.MAX_PAGE_NUMBER_ = 1000000000;

  PageRange.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      var result = pageRangeTextToPageRanges(
          value, this.getDocumentInfoInternal().pageCount);
      return result instanceof Array;
    },

    /**
     * @return {!print_preview.PageNumberSet} Set of page numbers defined by the
     *     page range string.
     */
    getPageNumberSet: function() {
      var pageNumberList = pageRangeTextToPageList(
          this.getValue(), this.getDocumentInfoInternal().pageCount);
      return new print_preview.PageNumberSet(pageNumberList);
    },

    /** @override */
    isCapabilityAvailable: function() {
      return true;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return '';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '';
    },

    /**
     * @return {!Array<Object<{from: number, to: number}>>} A list of page
     *     ranges.
     */
    getPageRanges: function() {
      var pageRanges = pageRangeTextToPageRanges(this.getValue());
      return pageRanges instanceof Array ? pageRanges : [];
    },

    /**
     * @return {!Array<object<{from: number, to: number}>>} A list of page
     *     ranges suitable for use in the native layer.
     * TODO(vitalybuka): this should be removed when native layer switched to
     *     page ranges.
     */
    getDocumentPageRanges: function() {
      var pageRanges = pageRangeTextToPageRanges(
          this.getValue(), this.getDocumentInfoInternal().pageCount);
      return pageRanges instanceof Array ? pageRanges : [];
    },

    /**
     * @return {!number} Number of pages reported by the document.
     */
    getDocumentNumPages: function() {
      return this.getDocumentInfoInternal().pageCount;
    },

    /**
     * @return {!PageRangeStatus}
     */
    checkValidity: function() {
      var pageRanges = pageRangeTextToPageRanges(
          this.getValue(), this.getDocumentInfoInternal().pageCount);
      return pageRanges instanceof Array ?
          PageRangeStatus.NO_ERROR : pageRanges;
    },
  };

  // Export
  return {
    PageRange: PageRange
  };
});
