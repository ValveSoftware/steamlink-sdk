// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/database_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "content/common/database_messages.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/result_codes.h"
#include "third_party/sqlite/sqlite3.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/browser/database/vfs_backend.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/common/database/database_identifier.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using quota::QuotaManager;
using quota::QuotaStatusCode;
using webkit_database::DatabaseTracker;
using webkit_database::DatabaseUtil;
using webkit_database::VfsBackend;

namespace content {
namespace {

const int kNumDeleteRetries = 2;
const int kDelayDeleteRetryMs = 100;

}  // namespace

DatabaseMessageFilter::DatabaseMessageFilter(
    webkit_database::DatabaseTracker* db_tracker)
    : BrowserMessageFilter(DatabaseMsgStart),
      db_tracker_(db_tracker),
      observer_added_(false) {
  DCHECK(db_tracker_.get());
}

void DatabaseMessageFilter::OnChannelClosing() {
  if (observer_added_) {
    observer_added_ = false;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DatabaseMessageFilter::RemoveObserver, this));
  }
}

void DatabaseMessageFilter::AddObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  db_tracker_->AddObserver(this);
}

void DatabaseMessageFilter::RemoveObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  db_tracker_->RemoveObserver(this);

  // If the renderer process died without closing all databases,
  // then we need to manually close those connections
  db_tracker_->CloseDatabases(database_connections_);
  database_connections_.RemoveAllConnections();
}

void DatabaseMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == DatabaseHostMsg_GetSpaceAvailable::ID)
    *thread = BrowserThread::IO;
  else if (IPC_MESSAGE_CLASS(message) == DatabaseMsgStart)
    *thread = BrowserThread::FILE;

  if (message.type() == DatabaseHostMsg_Opened::ID && !observer_added_) {
    observer_added_ = true;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DatabaseMessageFilter::AddObserver, this));
  }
}

bool DatabaseMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DatabaseMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_OpenFile,
                                    OnDatabaseOpenFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_DeleteFile,
                                    OnDatabaseDeleteFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_GetFileAttributes,
                                    OnDatabaseGetFileAttributes)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_GetFileSize,
                                    OnDatabaseGetFileSize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_GetSpaceAvailable,
                                    OnDatabaseGetSpaceAvailable)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_Opened, OnDatabaseOpened)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_Modified, OnDatabaseModified)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_Closed, OnDatabaseClosed)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_HandleSqliteError, OnHandleSqliteError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

DatabaseMessageFilter::~DatabaseMessageFilter() {
}

void DatabaseMessageFilter::OnDatabaseOpenFile(
    const base::string16& vfs_file_name,
    int desired_flags,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::File file;
  const base::File* tracked_file = NULL;
  std::string origin_identifier;
  base::string16 database_name;

  // When in incognito mode, we want to make sure that all DB files are
  // removed when the incognito browser context goes away, so we add the
  // SQLITE_OPEN_DELETEONCLOSE flag when opening all files, and keep
  // open handles to them in the database tracker to make sure they're
  // around for as long as needed.
  if (vfs_file_name.empty()) {
    file = VfsBackend::OpenTempFileInDirectory(db_tracker_->DatabaseDirectory(),
                                               desired_flags);
  } else if (DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                            &database_name, NULL) &&
             !db_tracker_->IsDatabaseScheduledForDeletion(origin_identifier,
                                                          database_name)) {
    base::FilePath db_file = DatabaseUtil::GetFullFilePathForVfsFile(
        db_tracker_.get(), vfs_file_name);
    if (!db_file.empty()) {
      if (db_tracker_->IsIncognitoProfile()) {
        tracked_file = db_tracker_->GetIncognitoFile(vfs_file_name);
        if (!tracked_file) {
          file =
              VfsBackend::OpenFile(db_file,
                                   desired_flags | SQLITE_OPEN_DELETEONCLOSE);
          if (!(desired_flags & SQLITE_OPEN_DELETEONCLOSE)) {
            tracked_file = db_tracker_->SaveIncognitoFile(vfs_file_name,
                                                          file.Pass());
          }
        }
      } else {
        file = VfsBackend::OpenFile(db_file, desired_flags);
      }
    }
  }

  // Then we duplicate the file handle to make it useable in the renderer
  // process. The original handle is closed, unless we saved it in the
  // database tracker.
  IPC::PlatformFileForTransit target_handle =
      IPC::InvalidPlatformFileForTransit();
  if (file.IsValid()) {
    target_handle = IPC::TakeFileHandleForProcess(file.Pass(), PeerHandle());
  } else if (tracked_file) {
    DCHECK(tracked_file->IsValid());
    target_handle =
        IPC::GetFileHandleForProcess(tracked_file->GetPlatformFile(),
                                     PeerHandle(), false);
  }

  DatabaseHostMsg_OpenFile::WriteReplyParams(reply_msg, target_handle);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseDeleteFile(
    const base::string16& vfs_file_name,
    const bool& sync_dir,
    IPC::Message* reply_msg) {
  DatabaseDeleteFile(vfs_file_name, sync_dir, reply_msg, kNumDeleteRetries);
}

void DatabaseMessageFilter::DatabaseDeleteFile(
    const base::string16& vfs_file_name,
    bool sync_dir,
    IPC::Message* reply_msg,
    int reschedule_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    // In order to delete a journal file in incognito mode, we only need to
    // close the open handle to it that's stored in the database tracker.
    if (db_tracker_->IsIncognitoProfile()) {
      const base::string16 wal_suffix(base::ASCIIToUTF16("-wal"));
      base::string16 sqlite_suffix;

      // WAL files can be deleted without having previously been opened.
      if (!db_tracker_->HasSavedIncognitoFileHandle(vfs_file_name) &&
          DatabaseUtil::CrackVfsFileName(vfs_file_name,
                                         NULL, NULL, &sqlite_suffix) &&
          sqlite_suffix == wal_suffix) {
        error_code = SQLITE_OK;
      } else {
        db_tracker_->CloseIncognitoFileHandle(vfs_file_name);
        error_code = SQLITE_OK;
      }
    } else {
      error_code = VfsBackend::DeleteFile(db_file, sync_dir);
    }

    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      BrowserThread::PostDelayedTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&DatabaseMessageFilter::DatabaseDeleteFile, this,
                     vfs_file_name, sync_dir, reply_msg, reschedule_count - 1),
          base::TimeDelta::FromMilliseconds(kDelayDeleteRetryMs));
      return;
    }
  }

  DatabaseHostMsg_DeleteFile::WriteReplyParams(reply_msg, error_code);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseGetFileAttributes(
    const base::string16& vfs_file_name,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  int32 attributes = -1;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty())
    attributes = VfsBackend::GetFileAttributes(db_file);

  DatabaseHostMsg_GetFileAttributes::WriteReplyParams(
      reply_msg, attributes);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseGetFileSize(
    const base::string16& vfs_file_name, IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  int64 size = 0;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty())
    size = VfsBackend::GetFileSize(db_file);

  DatabaseHostMsg_GetFileSize::WriteReplyParams(reply_msg, size);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseGetSpaceAvailable(
    const std::string& origin_identifier, IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(db_tracker_->quota_manager_proxy());

  QuotaManager* quota_manager =
      db_tracker_->quota_manager_proxy()->quota_manager();
  if (!quota_manager) {
    NOTREACHED();  // The system is shutting down, messages are unexpected.
    DatabaseHostMsg_GetSpaceAvailable::WriteReplyParams(
        reply_msg, static_cast<int64>(0));
    Send(reply_msg);
    return;
  }

  quota_manager->GetUsageAndQuota(
      webkit_database::GetOriginFromIdentifier(origin_identifier),
      quota::kStorageTypeTemporary,
      base::Bind(&DatabaseMessageFilter::OnDatabaseGetUsageAndQuota,
                 this, reply_msg));
}

void DatabaseMessageFilter::OnDatabaseGetUsageAndQuota(
    IPC::Message* reply_msg,
    quota::QuotaStatusCode status,
    int64 usage,
    int64 quota) {
  int64 available = 0;
  if ((status == quota::kQuotaStatusOk) && (usage < quota))
    available = quota - usage;
  DatabaseHostMsg_GetSpaceAvailable::WriteReplyParams(reply_msg, available);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseOpened(
    const std::string& origin_identifier,
    const base::string16& database_name,
    const base::string16& description,
    int64 estimated_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!DatabaseUtil::IsValidOriginIdentifier(origin_identifier)) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_DBMF"));
    BadMessageReceived();
    return;
  }

  int64 database_size = 0;
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size);
  database_connections_.AddConnection(origin_identifier, database_name);
  Send(new DatabaseMsg_UpdateSize(origin_identifier, database_name,
                                  database_size));
}

void DatabaseMessageFilter::OnDatabaseModified(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_DBMF"));
    BadMessageReceived();
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void DatabaseMessageFilter::OnDatabaseClosed(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_DBMF"));
    BadMessageReceived();
    return;
  }

  database_connections_.RemoveConnection(origin_identifier, database_name);
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
}

void DatabaseMessageFilter::OnHandleSqliteError(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int error) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!DatabaseUtil::IsValidOriginIdentifier(origin_identifier)) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_DBMF"));
    BadMessageReceived();
    return;
  }

  db_tracker_->HandleSqliteError(origin_identifier, database_name, error);
}

void DatabaseMessageFilter::OnDatabaseSizeChanged(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int64 database_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (database_connections_.IsOriginUsed(origin_identifier)) {
    Send(new DatabaseMsg_UpdateSize(origin_identifier, database_name,
                                    database_size));
  }
}

void DatabaseMessageFilter::OnDatabaseScheduledForDeletion(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  Send(new DatabaseMsg_CloseImmediately(origin_identifier, database_name));
}

}  // namespace content
