// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Data model which contains information related to the document to print.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DocumentInfo() {
    cr.EventTarget.call(this);

    /**
     * Whether the document is styled by CSS media styles.
     * @type {boolean}
     * @private
     */
    this.hasCssMediaStyles_ = false;

    /**
     * Whether the document has selected content.
     * @type {boolean}
     * @private
     */
    this.hasSelection_ = false;

    /**
     * Whether the document to print is modifiable (i.e. can be reflowed).
     * @type {boolean}
     * @private
     */
    this.isModifiable_ = true;

    /**
     * Whether scaling of the document is prohibited.
     * @type {boolean}
     * @private
     */
    this.isScalingDisabled_ = false;

    /**
     * Margins of the document in points.
     * @type {print_preview.Margins}
     * @private
     */
    this.margins_ = null;

    /**
     * Number of pages in the document to print.
     * @type {number}
     * @private
     */
    this.pageCount_ = 0;

    // Create the document info with some initial settings. Actual
    // page-related information won't be set until preview generation occurs,
    // so we'll use some defaults until then. This way, the print ticket store
    // will be valid even if no preview can be generated.
    var initialPageSize = new print_preview.Size(612, 792); // 8.5"x11"

    /**
     * Size of the pages of the document in points.
     * @type {!print_preview.Size}
     * @private
     */
    this.pageSize_ = initialPageSize;

    /**
     * Printable area of the document in points.
     * @type {!print_preview.PrintableArea}
     * @private
     */
    this.printableArea_ = new print_preview.PrintableArea(
        new print_preview.Coordinate2d(0, 0), initialPageSize);

    /**
     * Title of document.
     * @type {string}
     * @private
     */
    this.title_ = '';

    /**
     * Whether this data model has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;
  };

  /**
   * Event types dispatched by this data model.
   * @enum {string}
   */
  DocumentInfo.EventType = {
    CHANGE: 'print_preview.DocumentInfo.CHANGE'
  };

  DocumentInfo.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** @return {boolean} Whether the document is styled by CSS media styles. */
    get hasCssMediaStyles() {
      return this.hasCssMediaStyles_;
    },

    /** @return {boolean} Whether the document has selected content. */
    get hasSelection() {
      return this.hasSelection_;
    },

    /**
     * @return {boolean} Whether the document to print is modifiable (i.e. can
     *     be reflowed).
     */
    get isModifiable() {
      return this.isModifiable_;
    },

    /** @return {boolean} Whether scaling of the document is prohibited. */
    get isScalingDisabled() {
      return this.isScalingDisabled_;
    },

    /** @return {print_preview.Margins} Margins of the document in points. */
    get margins() {
      return this.margins_;
    },

    /** @return {number} Number of pages in the document to print. */
    get pageCount() {
      return this.pageCount_;
    },

    /**
     * @return {!print_preview.Size} Size of the pages of the document in
     *     points.
     */
    get pageSize() {
      return this.pageSize_;
    },

    /**
     * @return {!print_preview.PrintableArea} Printable area of the document in
     *     points.
     */
    get printableArea() {
      return this.printableArea_;
    },

    /** @return {string} Title of document. */
    get title() {
      return this.title_;
    },

    /**
     * Initializes the state of the data model and dispatches a CHANGE event.
     * @param {boolean} isModifiable Whether the document is modifiable.
     * @param {string} title Title of the document.
     * @param {boolean} hasSelection Whether the document has user-selected
     *     content.
     */
    init: function(isModifiable, title, hasSelection) {
      this.isModifiable_ = isModifiable;
      this.title_ = title;
      this.hasSelection_ = hasSelection;
      this.isInitialized_ = true;
      cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
    },

    /**
     * Updates whether scaling is disabled for the document and dispatches a
     * CHANGE event.
     * @param {boolean} isScalingDisabled Whether scaling of the document is
     *     prohibited.
     */
    updateIsScalingDisabled: function(isScalingDisabled) {
      if (this.isInitialized_ && this.isScalingDisabled_ != isScalingDisabled) {
        this.isScalingDisabled_ = isScalingDisabled;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    },

    /**
     * Updates the total number of pages in the document and dispatches a CHANGE
     * event.
     * @param {number} pageCount Number of pages in the document.
     */
    updatePageCount: function(pageCount) {
      if (this.isInitialized_ && this.pageCount_ != pageCount) {
        this.pageCount_ = pageCount;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    },

    /**
     * Updates information about each page and dispatches a CHANGE event.
     * @param {!print_preview.PrintableArea} printableArea Printable area of the
     *     document in points.
     * @param {!print_preview.Size} pageSize Size of the pages of the document
     *     in points.
     * @param {boolean} hasCssMediaStyles Whether the document is styled by CSS
     *     media styles.
     * @param {print_preview.Margins} margins Margins of the document in points.
     */
    updatePageInfo: function(
        printableArea, pageSize, hasCssMediaStyles, margins) {
      if (this.isInitialized_ &&
          (!this.printableArea_.equals(printableArea) ||
           !this.pageSize_.equals(pageSize) ||
           this.hasCssMediaStyles_ != hasCssMediaStyles ||
           this.margins_ == null || !this.margins_.equals(margins))) {
        this.printableArea_ = printableArea;
        this.pageSize_ = pageSize;
        this.hasCssMediaStyles_ = hasCssMediaStyles;
        this.margins_ = margins;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    }
  };

  // Export
  return {
    DocumentInfo: DocumentInfo
  };
});
