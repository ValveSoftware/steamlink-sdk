// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_database_observer_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/storage_util.h"
#include "content/common/database_messages.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/sqlite/sqlite3.h"

using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

namespace {

const int kResultHistogramSize = 50;
const int kCallsiteHistogramSize = 10;
const int kWebSQLSuccess = -1;

int DetermineHistogramResult(int websql_error, int sqlite_error) {
  // If we have a sqlite error, log it after trimming the extended bits.
  // There are 26 possible values, but we leave room for some new ones.
  if (sqlite_error)
    return std::min(sqlite_error & 0xff, 30);

  // Otherwise, websql_error may be an SQLExceptionCode, SQLErrorCode
  // or a DOMExceptionCode, or -1 for success.
  if (websql_error == kWebSQLSuccess)
    return 0;  // no error

  // SQLExceptionCode starts at 1000
  if (websql_error >= 1000)
    websql_error -= 1000;

  return std::min(websql_error + 30, kResultHistogramSize - 1);
}

#define UMA_HISTOGRAM_WEBSQL_RESULT(name, callsite, websql_error, sqlite_error) \
  do { \
    DCHECK(callsite < kCallsiteHistogramSize); \
    int result = DetermineHistogramResult(websql_error, sqlite_error); \
    UMA_HISTOGRAM_ENUMERATION("websql.Async." name, \
                              result, kResultHistogramSize); \
    if (result) { \
      UMA_HISTOGRAM_ENUMERATION("websql.Async." name ".ErrorSite", \
                                callsite, kCallsiteHistogramSize); \
    } \
  } while (0)

// TODO(jsbell): Replace with use of url::Origin end-to-end.
// https://crbug.com/591482
std::string GetIdentifierFromOrigin(const WebSecurityOrigin& origin) {
  return storage::GetIdentifierFromOrigin(WebSecurityOriginToGURL(origin));
}

}  // namespace

WebDatabaseObserverImpl::WebDatabaseObserverImpl(IPC::SyncMessageFilter* sender)
    : sender_(sender),
      open_connections_(new storage::DatabaseConnectionsWrapper),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(sender);
  DCHECK(main_thread_task_runner_);
}

WebDatabaseObserverImpl::~WebDatabaseObserverImpl() {
}

void WebDatabaseObserverImpl::databaseOpened(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    const WebString& database_display_name,
    unsigned long estimated_size) {
  open_connections_->AddOpenConnection(GetIdentifierFromOrigin(origin),
                                       database_name);
  sender_->Send(new DatabaseHostMsg_Opened(
      origin, database_name, database_display_name, estimated_size));
}

void WebDatabaseObserverImpl::databaseModified(const WebSecurityOrigin& origin,
                                               const WebString& database_name) {
  sender_->Send(new DatabaseHostMsg_Modified(origin, database_name));
}

void WebDatabaseObserverImpl::databaseClosed(const WebSecurityOrigin& origin,
                                             const WebString& database_name) {
  DCHECK(!main_thread_task_runner_->RunsTasksOnCurrentThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IPC::SyncMessageFilter::Send), sender_,
                 new DatabaseHostMsg_Closed(origin, database_name)));
  open_connections_->RemoveOpenConnection(GetIdentifierFromOrigin(origin),
                                          database_name);
}

void WebDatabaseObserverImpl::reportOpenDatabaseResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int callsite,
    int websql_error,
    int sqlite_error,
    double call_time) {
  UMA_HISTOGRAM_WEBSQL_RESULT("OpenResult", callsite,
                              websql_error, sqlite_error);
  HandleSqliteError(origin, database_name, sqlite_error);

  if (websql_error == kWebSQLSuccess && sqlite_error == SQLITE_OK) {
    UMA_HISTOGRAM_TIMES("websql.Async.OpenTime.Success",
                        base::TimeDelta::FromSecondsD(call_time));
  } else {
    UMA_HISTOGRAM_TIMES("websql.Async.OpenTime.Error",
                        base::TimeDelta::FromSecondsD(call_time));
  }
}

void WebDatabaseObserverImpl::reportChangeVersionResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int callsite,
    int websql_error,
    int sqlite_error) {
  UMA_HISTOGRAM_WEBSQL_RESULT("ChangeVersionResult", callsite,
                              websql_error, sqlite_error);
  HandleSqliteError(origin, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportStartTransactionResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int callsite,
    int websql_error,
    int sqlite_error) {
  UMA_HISTOGRAM_WEBSQL_RESULT("BeginResult", callsite,
                              websql_error, sqlite_error);
  HandleSqliteError(origin, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportCommitTransactionResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int callsite,
    int websql_error,
    int sqlite_error) {
  UMA_HISTOGRAM_WEBSQL_RESULT("CommitResult", callsite,
                              websql_error, sqlite_error);
  HandleSqliteError(origin, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportExecuteStatementResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int callsite,
    int websql_error,
    int sqlite_error) {
  UMA_HISTOGRAM_WEBSQL_RESULT("StatementResult", callsite,
                              websql_error, sqlite_error);
  HandleSqliteError(origin, database_name, sqlite_error);
}

void WebDatabaseObserverImpl::reportVacuumDatabaseResult(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    int sqlite_error) {
  int result = DetermineHistogramResult(-1, sqlite_error);
  UMA_HISTOGRAM_ENUMERATION("websql.Async.VacuumResult",
                              result, kResultHistogramSize);
  HandleSqliteError(origin, database_name, sqlite_error);
}

bool WebDatabaseObserverImpl::WaitForAllDatabasesToClose(
    base::TimeDelta timeout) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  return open_connections_->WaitForAllDatabasesToClose(timeout);
}

void WebDatabaseObserverImpl::HandleSqliteError(const WebSecurityOrigin& origin,
                                                const WebString& database_name,
                                                int error) {
  // We filter out errors which the backend doesn't act on to avoid
  // a unnecessary ipc traffic, this method can get called at a fairly
  // high frequency (per-sqlstatement).
  if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB) {
    sender_->Send(
        new DatabaseHostMsg_HandleSqliteError(origin, database_name, error));
  }
}

}  // namespace content
