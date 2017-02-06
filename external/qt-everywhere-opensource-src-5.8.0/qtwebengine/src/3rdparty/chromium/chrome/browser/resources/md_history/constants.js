// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Globals:
/** @const */ var RESULTS_PER_PAGE = 150;

/**
 * Amount of time between pageviews that we consider a 'break' in browsing,
 * measured in milliseconds.
 * @const
 */
var BROWSING_GAP_TIME = 15 * 60 * 1000;

/**
 * Maximum length of a history item title. Anything longer than this will be
 * cropped to fit within this limit. This value is large enough that it will not
 * be noticeable in a 960px wide history-item.
 * @const
 */
var TITLE_MAX_LENGTH = 300;

/**
 * @enum {number}
 */
var HistoryRange = {
  ALL_TIME: 0,
  WEEK: 1,
  MONTH: 2
};
