// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword.metrics', function() {
  'use strict';

  /**
   * Helper function to record enum values in UMA.
   * @param {!string} name
   * @param {!number} value
   * @param {!number} maxValue
   */
  function recordEnum(name, value, maxValue) {
    var metricDesc = {
      'metricName': name,
      'type': 'histogram-linear',
      'min': 1,
      'max': maxValue,
      'buckets': maxValue + 1
    };
    chrome.metricsPrivate.recordValue(metricDesc, value);
  }

  return {
    recordEnum: recordEnum
  };
});
