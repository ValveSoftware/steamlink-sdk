// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-printing-page',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

<if expr="chromeos">
    /**
     * Whether to show CUPS printers settings.
     * @private {boolean}
     */
    showCupsPrintingFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showCupsPrintingFeatures');
      },
    },
</if>

    /** @type {!Array<!CupsPrinterInfo>} */
    cupsPrinters: {
      type: Array,
      notify: true,
    },

    searchTerm: {
      type: String,
    },
  },

  listeners: {
    'show-cups-printer-details': 'onShowCupsPrinterDetailsPage_',
  },

<if expr="chromeos">
  /** @private */
  onTapCupsPrinters_: function() {
    settings.navigateTo(settings.Route.CUPS_PRINTERS);
  },

  /** @private */
  onShowCupsPrinterDetailsPage_: function(event) {
    settings.navigateTo(settings.Route.CUPS_PRINTER_DETAIL);
    this.$.arraySelector.select(event.detail);
  },
</if>

  /** @private */
  onTapCloudPrinters_: function() {
    settings.navigateTo(settings.Route.CLOUD_PRINTERS);
  },
});
