// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 * @externs
 */

/**
 * The type of the history result object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc:
 *     BrowsingHistoryHandler::HistoryEntry::ToValue()
 * @typedef {{allTimestamps: Array<number>,
 *            blockedVisit: boolean,
 *            dateRelativeDay: string,
 *            dateShort: string,
 *            dateTimeOfDay: string,
 *            deviceName: string,
 *            deviceType: string,
 *            domain: string,
 *            fallbackFaviconText: string,
 *            hostFilteringBehavior: number,
 *            snippet: string,
 *            starred: boolean,
 *            time: number,
 *            title: string,
 *            url: string}}
 */
var HistoryEntry;

/**
 * The type of the history results info object. The definition is based on
 * chrome/browser/ui/webui/browsing_history_handler.cc:
 *     BrowsingHistoryHandler::QueryComplete()
 * @typedef {{finished: boolean,
 *            hasSyncedResults: boolean,
 *            queryEndTime: string,
 *            queryStartTime: string,
 *            term: string}}
 */
var HistoryQuery;

/**
 * The type of the foreign session info object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 * @typedef {{collapsed: boolean,
 *            deviceType: string,
 *            name: string,
 *            modifiedTime: string,
 *            tag: string,
 *            timestamp: number,
 *            windows: Array}}
 */
var ForeignSession;

/**
 * The type of the foreign session tab object. This definition is based on
 * chrome/browser/ui/webui/foreign_session_handler.cc:
 * @typedef {{direction: string,
 *            sessionId: number,
 *            timestamp: number,
 *            title: string,
 *            type: string,
 *            url: string}}
 */
var ForeignSessionTab;
