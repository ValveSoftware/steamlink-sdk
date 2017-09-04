// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printer-details-page' is the subpage for
 * viewing the details of a CUPS printer.
 */
Polymer({
  is: 'settings-cups-printer-details-page',

  properties: {
    /** @type {!CupsPrinterInfo} */
    printer: {
      type: Object,
      notify: true,
    },

    advancedExpanded: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * Event triggered when the input value changes.
   * @private
   */
  onValueChanged_: function() {
    this.browserProxy_.updateCupsPrinter(this.printer.printerId,
                                         this.printer.printerName);
  },

  /**
   * @param {Event} event
   * @private
   */
  toggleAdvancedExpanded_: function(event) {
    if (event.target.id == 'expandButton')
      return;  // Already handled.
    this.advancedExpanded = !this.advancedExpanded;
  },

  /**
   * @param {string} printerProtocol
   * @return {string} The Printer's protocol that displays in UI
   * @private
   */
  getPrinterProtocol_: function(printerProtocol) {
    if (!printerProtocol)
      return '';
    if (printerProtocol == 'ipp')
      return loadTimeData.getString('printerProtocolIpp');
    if (printerProtocol == 'ipps')
      return loadTimeData.getString('printerProtocolIpps');
    if (printerProtocol == 'http')
      return loadTimeData.getString('printerProtocolHttp');
    if (printerProtocol == 'https')
      return loadTimeData.getString('printerProtocolHttps');
    if (printerProtocol == 'socket')
      return loadTimeData.getString('printerProtocolAppSocket');
    if (printerProtocol == 'lpd')
      return loadTimeData.getString('printerProtocolLpd');
    if (printerProtocol == 'usb')
      return loadTimeData.getString('printerProtocolUsb');
    assertNotReached('Illegal printer protocol!');
  },
});
