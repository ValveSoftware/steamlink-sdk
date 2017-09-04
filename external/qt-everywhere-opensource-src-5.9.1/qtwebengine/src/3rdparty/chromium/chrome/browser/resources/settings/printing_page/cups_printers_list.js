// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-list' is a component for a list of
 * CUPS printers.
 */
Polymer({
  is: 'settings-cups-printers-list',

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /**
     * The model for the printer action menu.
     * @private {?CupsPrinterInfo}
     */
    activePrinter_: Object,
  },

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * @param {!{model: !{item: !CupsPrinterInfo}}} e
   * @private
   */
  onOpenActionMenuTap_: function(e) {
    this.activePrinter_ = e.model.item;
    var menu = /** @type {!CrActionMenuElement} */ (
        this.$$('dialog[is=cr-action-menu]'));
    menu.showAt(/** @type {!Element} */ (
        Polymer.dom(/** @type {!Event} */ (e)).localTarget));
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onDetailsTap_: function(event) {
    // Event is caught by 'settings-printing-page'.
    this.fire('show-cups-printer-details', this.activePrinter_);
    this.closeDropdownMenu_();
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onRemoveTap_: function(event) {
    var index = this.printers.indexOf(assert(this.activePrinter_));
    this.splice('printers', index, 1);
    this.browserProxy_.removeCupsPrinter(this.activePrinter_.printerId,
                                         this.activePrinter_.printerName);
    this.closeDropdownMenu_();
  },

  /** @private */
  closeDropdownMenu_: function() {
    this.activePrinter_ = null;
    var menu = /** @type {!CrActionMenuElement} */ (
        this.$$('dialog[is=cr-action-menu]'));
    menu.close();
  },

  /**
   * The filter callback function to show printers based on |searchTerm|.
   * @param {string} searchTerm
   * @private
   */
  filterPrinter_: function(searchTerm) {
    if (!searchTerm)
      return null;
    return function(printer) {
      return printer.printerName.toLowerCase().includes(
          searchTerm.toLowerCase());
    };
  },
});
