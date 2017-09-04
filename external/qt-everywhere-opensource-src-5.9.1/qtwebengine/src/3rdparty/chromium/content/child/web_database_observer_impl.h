// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
#define CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_sync_message_filter.h"
#include "storage/common/database/database_connections.h"
#include "third_party/WebKit/public/platform/WebDatabaseObserver.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class WebDatabaseObserverImpl : public blink::WebDatabaseObserver {
 public:
  explicit WebDatabaseObserverImpl(IPC::SyncMessageFilter* sender);
  virtual ~WebDatabaseObserverImpl();

  void databaseOpened(const blink::WebSecurityOrigin& origin,
                      const blink::WebString& database_name,
                      const blink::WebString& database_display_name,
                      unsigned long estimated_size) override;
  void databaseModified(const blink::WebSecurityOrigin& origin,
                        const blink::WebString& database_name) override;
  void databaseClosed(const blink::WebSecurityOrigin& origin,
                      const blink::WebString& database_name) override;
  void reportOpenDatabaseResult(const blink::WebSecurityOrigin& origin,
                                const blink::WebString& database_name,
                                int callsite,
                                int websql_error,
                                int sqlite_error,
                                double call_time) override;
  void reportChangeVersionResult(const blink::WebSecurityOrigin& origin,
                                 const blink::WebString& database_name,
                                 int callsite,
                                 int websql_error,
                                 int sqlite_error) override;
  void reportStartTransactionResult(const blink::WebSecurityOrigin& origin,
                                    const blink::WebString& database_name,
                                    int callsite,
                                    int websql_error,
                                    int sqlite_error) override;
  void reportCommitTransactionResult(const blink::WebSecurityOrigin& origin,
                                     const blink::WebString& database_name,
                                     int callsite,
                                     int websql_error,
                                     int sqlite_error) override;
  void reportExecuteStatementResult(const blink::WebSecurityOrigin& origin,
                                    const blink::WebString& database_name,
                                    int callsite,
                                    int websql_error,
                                    int sqlite_error) override;
  void reportVacuumDatabaseResult(const blink::WebSecurityOrigin& origin,
                                  const blink::WebString& database_name,
                                  int sqlite_error) override;

  bool WaitForAllDatabasesToClose(base::TimeDelta timeout);

 private:
  void HandleSqliteError(const blink::WebSecurityOrigin& origin,
                         const blink::WebString& database_name,
                         int error);

  scoped_refptr<IPC::SyncMessageFilter> sender_;
  scoped_refptr<storage::DatabaseConnectionsWrapper> open_connections_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseObserverImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
