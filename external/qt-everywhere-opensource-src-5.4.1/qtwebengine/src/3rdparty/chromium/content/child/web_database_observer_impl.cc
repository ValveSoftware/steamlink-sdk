// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_database_observer_impl.h"

#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "content/common/database_messages.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/sqlite/sqlite3.h"

using blink::WebString;

namespace content {

namespace {

const int kResultHistogramSize = 50;
const int kCallsiteHistogramSize = 10;

int DetermineHistogramResult(int websql_error, int sqlite_error) {
  // If we have a sqlite error, log it after trimming the extended bits.
  // There are 26 possible values, but we leave room for some new ones.
  if (sqlite_error)
    return std::min(sqlite_error & 0xff, 30);

  // Otherwise, websql_error may be an SQLExceptionCode, SQLErrorCode
  // or a DOMExceptionCode, or -1 for success.
  if (websql_error == -1)
    return 0;  // no error

  // SQLExceptionCode starts at 1000
  if (websql_error >= 1000)
    websql_error -= 1000;

  return std::min(websql_error + 30, kResultHistogramSize - 1);
}

#define HISTOGRAM_WEBSQL_RESULT(name, is_sync_database, \
                                callsite, websql_error, sqlite_error) \
  do { \
    DCHECK(callsite < kCallsiteHistogramSize); \
    int result = DetermineHistogramResult(websql_error, sqlite_error); \
    if (is_sync_database) { \
      UMA_HISTOGRAM_ENUMERATION("websql.Sync." name, \
                                result, kResultHistogramSize); \
      if (result) { \
        UMA_HISTOGRAM_ENUMERATION("websql.Sync." name ".ErrorSite", \
                                  callsite, kCallsiteHistogramSize); \
      } \
    } else { \
      UMA_HISTOGRAM_ENUMERATION("websql.Async." name, \
                                result, kResultHistogramSize); \
      if (result) { \
        UMA_HISTOGRAM_ENUMERATION("websql.Async." name ".ErrorSite", \
                                  callsite, kCallsiteHistogramSize); \
      } \
    } \
  } while (0)

}  // namespace

WebDatabaseObserverImpl::WebDatabaseObserverImpl(
    IPC::SyncMessageFilter* sender)
    : sender_(sender),
      open_connections_(new webkit_database::DatabaseConnectionsWrapper) {
  DCHECK(sender);
}

WebDatabaseObserverImpl::~WebDatabaseObserverImpl() {
}

void WebDatabaseObserverImpl::databaseOpened(
    const WebString& origin_identifier,
    const WebString& database_name,
    const WebString& database_display_name,
    unsigned long estimated_size) {
  open_connections_->AddOpenConnection(origin_identifier.utf8(),
                                       database_name);
  sender_->Send(new DatabaseHostMsg_Opened(
      origin_identifier.utf8(), database_name,
      database_display_name, estimated_size));
}

void WebDatabaseObserverImpl::databaseModified(
    const WebString& origin_identifier,
    const WebString& database_name) {
  sender_->Send(new DatabaseHostMsg_Modified(
      origin_identifier.utf8(), database_name));
}

void WebDatabaseObserverImpl::databaseClosed(
    const WebString& origin_identifier,
    const WebString& database_name) {
  sender_->Send(new DatabaseHostMsg_Closed(
      origin_identifier.utf8(), database_name));
  open_connections_->RemoveOpenConnection(origin_identifier.utf8(),
                                          database_name);
}

void WebDatabaseObserverImpl::reportOpenDatabaseResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int callsite, int websql_error, int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("OpenResult", is_sync_database,
                          callsite, websql_error, sqlite_error);
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportChangeVersionResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int callsite, int websql_error, int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("ChangeVersionResult", is_sync_database,
                          callsite, websql_error, sqlite_error);
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportStartTransactionResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int callsite, int websql_error, int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("BeginResult", is_sync_database,
                          callsite, websql_error, sqlite_error);
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportCommitTransactionResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int callsite, int websql_error, int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("CommitResult", is_sync_database,
                          callsite, websql_error, sqlite_error);
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportExecuteStatementResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int callsite, int websql_error, int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("StatementResult", is_sync_database,
                          callsite, websql_error, sqlite_error);
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportVacuumDatabaseResult(
    const WebString& origin_identifier,
    const WebString& database_name,
    bool is_sync_database,
    int sqlite_error) {
  int result = DetermineHistogramResult(-1, sqlite_error);
  if (is_sync_database) {
    UMA_HISTOGRAM_ENUMERATION("websql.Sync.VacuumResult",
                              result, kResultHistogramSize);
  } else {
    UMA_HISTOGRAM_ENUMERATION("websql.Async.VacuumResult",
                              result, kResultHistogramSize);
  }
  HandleSqliteError(origin_identifier, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::WaitForAllDatabasesToClose() {
  open_connections_->WaitForAllDatabasesToClose();
}

void WebDatabaseObserverImpl::HandleSqliteError(
    const WebString& origin_identifier,
    const WebString& database_name,
    int error) {
  // We filter out errors which the backend doesn't act on to avoid
  // a unnecessary ipc traffic, this method can get called at a fairly
  // high frequency (per-sqlstatement).
  if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB) {
    sender_->Send(new DatabaseHostMsg_HandleSqliteError(
        origin_identifier.utf8(),
        database_name,
        error));
  }
}

}  // namespace content
