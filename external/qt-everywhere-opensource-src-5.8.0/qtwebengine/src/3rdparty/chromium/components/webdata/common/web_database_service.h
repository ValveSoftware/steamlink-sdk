// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/webdata_export.h"

class WebDatabaseBackend;
class WebDataRequestManager;

namespace content {
class BrowserContext;
}

namespace tracked_objects {
class Location;
}

class WDTypedResult;
class WebDataServiceConsumer;


////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseService defines the interface to a generic data repository
// responsible for controlling access to the web database (metadata associated
// with web pages).
//
////////////////////////////////////////////////////////////////////////////////

class WEBDATA_EXPORT WebDatabaseService
    : public base::RefCountedDeleteOnMessageLoop<WebDatabaseService> {
 public:
  typedef base::Callback<std::unique_ptr<WDTypedResult>(WebDatabase*)> ReadTask;
  typedef base::Callback<WebDatabase::State(WebDatabase*)> WriteTask;

  // Types for managing DB loading callbacks.
  typedef base::Closure DBLoadedCallback;
  typedef base::Callback<void(sql::InitStatus)> DBLoadErrorCallback;

  // Takes the path to the WebDatabase file.
  // WebDatabaseService lives on |ui_thread| and posts tasks to |db_thread|.
  WebDatabaseService(const base::FilePath& path,
                     scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
                     scoped_refptr<base::SingleThreadTaskRunner> db_thread);

  // Adds |table| as a WebDatabaseTable that will participate in
  // managing the database, transferring ownership. All calls to this
  // method must be made before |LoadDatabase| is called.
  virtual void AddTable(std::unique_ptr<WebDatabaseTable> table);

  // Initializes the web database service.
  virtual void LoadDatabase();

  // Unloads the database and shuts down the service.
  virtual void ShutdownDatabase();

  // Gets a pointer to the WebDatabase (owned by WebDatabaseService).
  // TODO(caitkp): remove this method once SyncServices no longer depend on it.
  virtual WebDatabase* GetDatabaseOnDB() const;

  // Returns a pointer to the WebDatabaseBackend.
  scoped_refptr<WebDatabaseBackend> GetBackend() const;

  // Schedule an update/write task on the DB thread.
  virtual void ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const WriteTask& task);

  // Schedule a read task on the DB thread.
  virtual WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ReadTask& task,
      WebDataServiceConsumer* consumer);

  // Cancel an existing request for a task on the DB thread.
  // TODO(caitkp): Think about moving the definition of the Handle type to
  // somewhere else.
  virtual void CancelRequest(WebDataServiceBase::Handle h);

  // Register a callback to be notified that the database has loaded. Multiple
  // callbacks may be registered, and each will be called at most once
  // (following a successful database load), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  void RegisterDBLoadedCallback(const DBLoadedCallback& callback);

  // Register a callback to be notified that the database has failed to load.
  // Multiple callbacks may be registered, and each will be called at most once
  // (following a database load failure), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  void RegisterDBErrorCallback(const DBLoadErrorCallback& callback);

  bool db_loaded() const { return db_loaded_; };

 private:
  class BackendDelegate;
  friend class BackendDelegate;
  friend class base::RefCountedDeleteOnMessageLoop<WebDatabaseService>;
  friend class base::DeleteHelper<WebDatabaseService>;

  typedef std::vector<DBLoadedCallback> LoadedCallbacks;
  typedef std::vector<DBLoadErrorCallback> ErrorCallbacks;

  virtual ~WebDatabaseService();

  void OnDatabaseLoadDone(sql::InitStatus status);

  base::FilePath path_;

  // The primary owner is |WebDatabaseService| but is refcounted because
  // PostTask on DB thread may outlive us.
  scoped_refptr<WebDatabaseBackend> web_db_backend_;

  // Callbacks to be called once the DB has loaded.
  LoadedCallbacks loaded_callbacks_;

  // Callbacks to be called if the DB has failed to load.
  ErrorCallbacks error_callbacks_;

  // True if the WebDatabase has loaded.
  bool db_loaded_;

  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  // All vended weak pointers are invalidated in ShutdownDatabase().
  base::WeakPtrFactory<WebDatabaseService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseService);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
