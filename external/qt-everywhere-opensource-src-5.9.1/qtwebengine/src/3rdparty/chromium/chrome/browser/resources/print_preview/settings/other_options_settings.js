// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * UI component that renders checkboxes for various print options.
   * @param {!print_preview.ticket_items.Duplex} duplex Duplex ticket item.
   * @param {!print_preview.ticket_items.FitToPage} fitToPage Fit-to-page ticket
   *     item.
   * @param {!print_preview.ticket_items.CssBackground} cssBackground CSS
   *     background ticket item.
   * @param {!print_preview.ticket_items.SelectionOnly} selectionOnly Selection
   *     only ticket item.
   * @param {!print_preview.ticket_items.HeaderFooter} headerFooter Header
   *     footer ticket item.
   * @constructor
   * @extends {print_preview.SettingsSection}
   */
  function OtherOptionsSettings(
      duplex, fitToPage, cssBackground, selectionOnly, headerFooter) {
    print_preview.SettingsSection.call(this);

    /**
     * Duplex ticket item, used to read/write the duplex selection.
     * @type {!print_preview.ticket_items.Duplex}
     * @private
     */
    this.duplexTicketItem_ = duplex;

    /**
     * Fit-to-page ticket item, used to read/write the fit-to-page selection.
     * @type {!print_preview.ticket_items.FitToPage}
     * @private
     */
    this.fitToPageTicketItem_ = fitToPage;

    /**
     * Enable CSS backgrounds ticket item, used to read/write.
     * @type {!print_preview.ticket_items.CssBackground}
     * @private
     */
    this.cssBackgroundTicketItem_ = cssBackground;

    /**
     * Print selection only ticket item, used to read/write.
     * @type {!print_preview.ticket_items.SelectionOnly}
     * @private
     */
    this.selectionOnlyTicketItem_ = selectionOnly;

    /**
     * Header-footer ticket item, used to read/write.
     * @type {!print_preview.ticket_items.HeaderFooter}
     * @private
     */
    this.headerFooterTicketItem_ = headerFooter;

     /**
     * Header footer container element.
     * @type {HTMLElement}
     * @private
     */
    this.headerFooterContainer_ = null;

    /**
     * Header footer checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.headerFooterCheckbox_ = null;

    /**
     * Fit-to-page container element.
     * @type {HTMLElement}
     * @private
     */
    this.fitToPageContainer_ = null;

    /**
     * Fit-to-page checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.fitToPageCheckbox_ = null;

    /**
     * Duplex container element.
     * @type {HTMLElement}
     * @private
     */
    this.duplexContainer_ = null;

    /**
     * Duplex checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.duplexCheckbox_ = null;

    /**
     * Print CSS backgrounds container element.
     * @type {HTMLElement}
     * @private
     */
    this.cssBackgroundContainer_ = null;

    /**
     * Print CSS backgrounds checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.cssBackgroundCheckbox_ = null;

    /**
     * Print selection only container element.
     * @type {HTMLElement}
     * @private
     */
    this.selectionOnlyContainer_ = null;

    /**
     * Print selection only checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.selectionOnlyCheckbox_ = null;
  };

  OtherOptionsSettings.prototype = {
    __proto__: print_preview.SettingsSection.prototype,

    /** @override */
    isAvailable: function() {
      return this.headerFooterTicketItem_.isCapabilityAvailable() ||
             this.fitToPageTicketItem_.isCapabilityAvailable() ||
             this.duplexTicketItem_.isCapabilityAvailable() ||
             this.cssBackgroundTicketItem_.isCapabilityAvailable() ||
             this.selectionOnlyTicketItem_.isCapabilityAvailable();
    },

    /** @override */
    hasCollapsibleContent: function() {
      return this.headerFooterTicketItem_.isCapabilityAvailable() ||
             this.cssBackgroundTicketItem_.isCapabilityAvailable() ||
             this.selectionOnlyTicketItem_.isCapabilityAvailable();
    },

    /** @override */
    set isEnabled(isEnabled) {
      this.headerFooterCheckbox_.disabled = !isEnabled;
      this.fitToPageCheckbox_.disabled = !isEnabled;
      this.duplexCheckbox_.disabled = !isEnabled;
      this.cssBackgroundCheckbox_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.SettingsSection.prototype.enterDocument.call(this);
      this.tracker.add(
          this.headerFooterCheckbox_,
          'click',
          this.onHeaderFooterCheckboxClick_.bind(this));
      this.tracker.add(
          this.fitToPageCheckbox_,
          'click',
          this.onFitToPageCheckboxClick_.bind(this));
      this.tracker.add(
          this.duplexCheckbox_,
          'click',
          this.onDuplexCheckboxClick_.bind(this));
      this.tracker.add(
          this.cssBackgroundCheckbox_,
          'click',
          this.onCssBackgroundCheckboxClick_.bind(this));
      this.tracker.add(
          this.selectionOnlyCheckbox_,
          'click',
          this.onSelectionOnlyCheckboxClick_.bind(this));
      this.tracker.add(
          this.duplexTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onDuplexChange_.bind(this));
      this.tracker.add(
          this.fitToPageTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onFitToPageChange_.bind(this));
      this.tracker.add(
          this.cssBackgroundTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onCssBackgroundChange_.bind(this));
      this.tracker.add(
          this.selectionOnlyTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onSelectionOnlyChange_.bind(this));
      this.tracker.add(
          this.headerFooterTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onHeaderFooterChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.SettingsSection.prototype.exitDocument.call(this);
      this.headerFooterContainer_ = null;
      this.headerFooterCheckbox_ = null;
      this.fitToPageContainer_ = null;
      this.fitToPageCheckbox_ = null;
      this.duplexContainer_ = null;
      this.duplexCheckbox_ = null;
      this.cssBackgroundContainer_ = null;
      this.cssBackgroundCheckbox_ = null;
      this.selectionOnlyContainer_ = null;
      this.selectionOnlyCheckbox_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.headerFooterContainer_ = this.getElement().querySelector(
          '.header-footer-container');
      this.headerFooterCheckbox_ = this.headerFooterContainer_.querySelector(
          '.header-footer-checkbox');
      this.fitToPageContainer_ = this.getElement().querySelector(
          '.fit-to-page-container');
      this.fitToPageCheckbox_ = this.fitToPageContainer_.querySelector(
          '.fit-to-page-checkbox');
      this.duplexContainer_ = this.getElement().querySelector(
          '.duplex-container');
      this.duplexCheckbox_ = this.duplexContainer_.querySelector(
          '.duplex-checkbox');
      this.cssBackgroundContainer_ = this.getElement().querySelector(
          '.css-background-container');
      this.cssBackgroundCheckbox_ = this.cssBackgroundContainer_.querySelector(
          '.css-background-checkbox');
      this.selectionOnlyContainer_ = this.getElement().querySelector(
          '.selection-only-container');
      this.selectionOnlyCheckbox_ = this.selectionOnlyContainer_.querySelector(
          '.selection-only-checkbox');
    },

    /** @override */
    updateUiStateInternal: function() {
      if (this.isAvailable()) {
        setIsVisible(this.headerFooterContainer_,
                     this.headerFooterTicketItem_.isCapabilityAvailable() &&
                     !this.collapseContent);
        setIsVisible(this.fitToPageContainer_,
                     this.fitToPageTicketItem_.isCapabilityAvailable());
        setIsVisible(this.duplexContainer_,
                     this.duplexTicketItem_.isCapabilityAvailable());
        setIsVisible(this.cssBackgroundContainer_,
                     this.cssBackgroundTicketItem_.isCapabilityAvailable() &&
                     !this.collapseContent);
        setIsVisible(this.selectionOnlyContainer_,
                     this.selectionOnlyTicketItem_.isCapabilityAvailable() &&
                     !this.collapseContent);
      }
      print_preview.SettingsSection.prototype.updateUiStateInternal.call(this);
    },

    /** @override */
    isSectionVisibleInternal: function() {
      if (this.collapseContent) {
        return this.fitToPageTicketItem_.isCapabilityAvailable() ||
               this.duplexTicketItem_.isCapabilityAvailable();
      }

      return this.isAvailable();
    },

     /**
     * Called when the header-footer checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onHeaderFooterCheckboxClick_: function() {
      this.headerFooterTicketItem_.updateValue(
          this.headerFooterCheckbox_.checked);
    },

    /**
     * Called when the fit-to-page checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onFitToPageCheckboxClick_: function() {
      this.fitToPageTicketItem_.updateValue(this.fitToPageCheckbox_.checked);
    },

    /**
     * Called when the duplex checkbox is clicked. Updates the print ticket.
     * @private
     */
    onDuplexCheckboxClick_: function() {
      this.duplexTicketItem_.updateValue(this.duplexCheckbox_.checked);
    },

    /**
     * Called when the print CSS backgrounds checkbox is clicked. Updates the
     * print ticket store.
     * @private
     */
    onCssBackgroundCheckboxClick_: function() {
      this.cssBackgroundTicketItem_.updateValue(
          this.cssBackgroundCheckbox_.checked);
    },

    /**
     * Called when the print selection only is clicked. Updates the
     * print ticket store.
     * @private
     */
    onSelectionOnlyCheckboxClick_: function() {
      this.selectionOnlyTicketItem_.updateValue(
          this.selectionOnlyCheckbox_.checked);
    },

    /**
     * Called when the duplex ticket item has changed. Updates the duplex
     * checkbox.
     * @private
     */
    onDuplexChange_: function() {
      this.duplexCheckbox_.checked = this.duplexTicketItem_.getValue();
      this.updateUiStateInternal();
    },

    /**
     * Called when the fit-to-page ticket item has changed. Updates the
     * fit-to-page checkbox.
     * @private
     */
    onFitToPageChange_: function() {
      this.fitToPageCheckbox_.checked = this.fitToPageTicketItem_.getValue();
      this.updateUiStateInternal();
    },

    /**
     * Called when the CSS background ticket item has changed. Updates the
     * CSS background checkbox.
     * @private
     */
    onCssBackgroundChange_: function() {
      this.cssBackgroundCheckbox_.checked =
          this.cssBackgroundTicketItem_.getValue();
      this.updateUiStateInternal();
    },

    /**
     * Called when the print selection only ticket item has changed. Updates the
     * CSS background checkbox.
     * @private
     */
    onSelectionOnlyChange_: function() {
      this.selectionOnlyCheckbox_.checked =
          this.selectionOnlyTicketItem_.getValue();
      this.updateUiStateInternal();
    },

    /**
     * Called when the header-footer ticket item has changed. Updates the
     * header-footer checkbox.
     * @private
     */
    onHeaderFooterChange_: function() {
      this.headerFooterCheckbox_.checked =
          this.headerFooterTicketItem_.getValue();
      this.updateUiStateInternal();
    },
  };

  // Export
  return {
    OtherOptionsSettings: OtherOptionsSettings
  };
});
