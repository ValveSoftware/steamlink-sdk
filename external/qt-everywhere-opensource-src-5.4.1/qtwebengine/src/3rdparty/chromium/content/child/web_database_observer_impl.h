// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
#define CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/WebDatabaseObserver.h"
#include "webkit/common/database/database_connections.h"

namespace content {

class WebDatabaseObserverImpl : public blink::WebDatabaseObserver {
 public:
  explicit WebDatabaseObserverImpl(IPC::SyncMessageFilter* sender);
  virtual ~WebDatabaseObserverImpl();

  virtual void databaseOpened(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      const blink::WebString& database_display_name,
      unsigned long estimated_size);
  virtual void databaseModified(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name);
  virtual void databaseClosed(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name);
  virtual void reportOpenDatabaseResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int callsite, int websql_error, int sqlite_error);
  virtual void reportChangeVersionResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int callsite, int websql_error, int sqlite_error);
  virtual void reportStartTransactionResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int callsite, int websql_error, int sqlite_error);
  virtual void reportCommitTransactionResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int callsite, int websql_error, int sqlite_error);
  virtual void reportExecuteStatementResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int callsite, int websql_error, int sqlite_error);
  virtual void reportVacuumDatabaseResult(
      const blink::WebString& origin_identifier,
      const blink::WebString& database_name,
      bool is_sync_database,
      int sqlite_error);

  void WaitForAllDatabasesToClose();

 private:
  void HandleSqliteError(const blink::WebString& origin_identifier,
                         const blink::WebString& database_name,
                         int error);

  scoped_refptr<IPC::SyncMessageFilter> sender_;
  scoped_refptr<webkit_database::DatabaseConnectionsWrapper> open_connections_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseObserverImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
