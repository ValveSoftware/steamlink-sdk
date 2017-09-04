// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs required to closure-compile MD History.
 * @externs
 */

// Types:
/**
 * @typedef {{groupedOffset: number,
 *            incremental: boolean,
 *            querying: boolean,
 *            range: HistoryRange,
 *            searchTerm: string}}
 */
var QueryState;

/**
 * @typedef {{info: ?HistoryQuery,
 *            results: ?Array<!HistoryEntry>,
 *            sessionList: ?Array<!ForeignSession>}}
 */
var QueryResult;

/**
 * @constructor
 * @extends {MouseEvent}
 */
var DomRepeatClickEvent = function() {};

/** @type {Object} */
DomRepeatClickEvent.prototype.model;
