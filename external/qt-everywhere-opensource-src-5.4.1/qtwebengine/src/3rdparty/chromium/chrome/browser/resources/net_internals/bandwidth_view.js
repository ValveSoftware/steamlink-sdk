// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This view displays summary statistics on bandwidth usage. */
var BandwidthView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function BandwidthView() {
    assertFirstConstructorCall(BandwidthView);

    // Call superclass's constructor.
    superClass.call(this, BandwidthView.MAIN_BOX_ID);

    g_browser.addSessionNetworkStatsObserver(this, true);
    g_browser.addHistoricNetworkStatsObserver(this, true);

    this.sessionNetworkStats_ = null;
    this.historicNetworkStats_ = null;
  }

  BandwidthView.TAB_ID = 'tab-handle-bandwidth';
  BandwidthView.TAB_NAME = 'Bandwidth';
  BandwidthView.TAB_HASH = '#bandwidth';

  // IDs for special HTML elements in bandwidth_view.html
  BandwidthView.MAIN_BOX_ID = 'bandwidth-view-tab-content';

  cr.addSingletonGetter(BandwidthView);

  BandwidthView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      // Even though this information is included in log dumps, there's no real
      // reason to display it when debugging a loaded log file.
      return false;
    },

    /**
     * Retains information on bandwidth usage this session.
     */
    onSessionNetworkStatsChanged: function(sessionNetworkStats) {
      this.sessionNetworkStats_ = sessionNetworkStats;
      return this.updateBandwidthUsageTable_();
    },

    /**
     * Displays information on bandwidth usage this session and over the
     * browser's lifetime.
     */
    onHistoricNetworkStatsChanged: function(historicNetworkStats) {
      this.historicNetworkStats_ = historicNetworkStats;
      return this.updateBandwidthUsageTable_();
    },

    /**
     * Update the bandwidth usage table.  Returns false on failure.
     */
    updateBandwidthUsageTable_: function() {
      var sessionNetworkStats = this.sessionNetworkStats_;
      var historicNetworkStats = this.historicNetworkStats_;
      if (!sessionNetworkStats || !historicNetworkStats)
        return false;

      var sessionOriginal = sessionNetworkStats.session_original_content_length;
      var sessionReceived = sessionNetworkStats.session_received_content_length;
      var historicOriginal =
          historicNetworkStats.historic_original_content_length;
      var historicReceived =
          historicNetworkStats.historic_received_content_length;

      var rows = [];
      rows.push({
          title: 'Original (KB)',
          sessionValue: bytesToRoundedKilobytes_(sessionOriginal),
          historicValue: bytesToRoundedKilobytes_(historicOriginal)
      });
      rows.push({
          title: 'Received (KB)',
          sessionValue: bytesToRoundedKilobytes_(sessionReceived),
          historicValue: bytesToRoundedKilobytes_(historicReceived)
      });
      rows.push({
          title: 'Savings (KB)',
          sessionValue:
              bytesToRoundedKilobytes_(sessionOriginal - sessionReceived),
          historicValue:
              bytesToRoundedKilobytes_(historicOriginal - historicReceived)
      });
      rows.push({
          title: 'Savings (%)',
          sessionValue: getPercentSavings_(sessionOriginal, sessionReceived),
          historicValue: getPercentSavings_(historicOriginal,
                                            historicReceived)
      });

      var input = new JsEvalContext({rows: rows});
      jstProcess(input, $(BandwidthView.MAIN_BOX_ID));
      return true;
    }
  };

  /**
   * Converts bytes to kilobytes rounded to one decimal place.
   */
  function bytesToRoundedKilobytes_(val) {
    return (val / 1024).toFixed(1);
  }

  /**
   * Returns bandwidth savings as a percent rounded to one decimal place.
   */
  function getPercentSavings_(original, received) {
    if (original > 0) {
      return ((original - received) * 100 / original).toFixed(1);
    }
    return '0.0';
  }

  return BandwidthView;
})();
