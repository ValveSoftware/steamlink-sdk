// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/database_message_filter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/common/database_messages.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/result_codes.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/database/vfs_backend.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/origin.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using storage::QuotaManager;
using storage::QuotaStatusCode;
using storage::DatabaseTracker;
using storage::DatabaseUtil;
using storage::VfsBackend;

namespace content {
namespace {

const int kNumDeleteRetries = 2;
const int kDelayDeleteRetryMs = 100;

bool IsOriginValid(const url::Origin& origin) {
  return !origin.unique();
}

}  // namespace

DatabaseMessageFilter::DatabaseMessageFilter(
    storage::DatabaseTracker* db_tracker)
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
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_OpenFile, OnDatabaseOpenFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_DeleteFile,
                                    OnDatabaseDeleteFile)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_GetFileAttributes,
                        OnDatabaseGetFileAttributes)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_GetFileSize, OnDatabaseGetFileSize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(DatabaseHostMsg_GetSpaceAvailable,
                                    OnDatabaseGetSpaceAvailable)
    IPC_MESSAGE_HANDLER(DatabaseHostMsg_SetFileSize, OnDatabaseSetFileSize)
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
    IPC::PlatformFileForTransit* handle) {
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
            tracked_file =
                db_tracker_->SaveIncognitoFile(vfs_file_name, std::move(file));
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
  *handle = IPC::InvalidPlatformFileForTransit();
  if (file.IsValid()) {
    *handle = IPC::TakePlatformFileForTransit(std::move(file));
  } else if (tracked_file) {
    DCHECK(tracked_file->IsValid());
    *handle =
        IPC::GetPlatformFileForTransit(tracked_file->GetPlatformFile(), false);
  }
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
    int32_t* attributes) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  *attributes = -1;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty())
    *attributes = VfsBackend::GetFileAttributes(db_file);
}

void DatabaseMessageFilter::OnDatabaseGetFileSize(
    const base::string16& vfs_file_name,
    int64_t* size) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  *size = 0;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty())
    *size = VfsBackend::GetFileSize(db_file);
}

void DatabaseMessageFilter::OnDatabaseGetSpaceAvailable(
    const url::Origin& origin,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(db_tracker_->quota_manager_proxy());

  if (!IsOriginValid(origin)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::DBMF_INVALID_ORIGIN_ON_GET_SPACE);
    return;
  }

  QuotaManager* quota_manager =
      db_tracker_->quota_manager_proxy()->quota_manager();
  if (!quota_manager) {
    NOTREACHED();  // The system is shutting down, messages are unexpected.
    DatabaseHostMsg_GetSpaceAvailable::WriteReplyParams(
        reply_msg, static_cast<int64_t>(0));
    Send(reply_msg);
    return;
  }

  // crbug.com/349708
  TRACE_EVENT0("io", "DatabaseMessageFilter::OnDatabaseGetSpaceAvailable");

  quota_manager->GetUsageAndQuota(
      GURL(origin.Serialize()), storage::kStorageTypeTemporary,
      base::Bind(&DatabaseMessageFilter::OnDatabaseGetUsageAndQuota, this,
                 reply_msg));
}

void DatabaseMessageFilter::OnDatabaseGetUsageAndQuota(
    IPC::Message* reply_msg,
    storage::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  int64_t available = 0;
  if ((status == storage::kQuotaStatusOk) && (usage < quota))
    available = quota - usage;
  DatabaseHostMsg_GetSpaceAvailable::WriteReplyParams(reply_msg, available);
  Send(reply_msg);
}

void DatabaseMessageFilter::OnDatabaseSetFileSize(
    const base::string16& vfs_file_name,
    int64_t size,
    bool* success) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  *success = false;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty())
    *success = VfsBackend::SetFileSize(db_file, size);
}

void DatabaseMessageFilter::OnDatabaseOpened(
    const url::Origin& origin,
    const base::string16& database_name,
    const base::string16& description,
    int64_t estimated_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!IsOriginValid(origin)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::DBMF_INVALID_ORIGIN_ON_OPEN);
    return;
  }

  GURL origin_url(origin.Serialize());
  UMA_HISTOGRAM_BOOLEAN("websql.OpenDatabase", IsOriginSecure(origin_url));

  int64_t database_size = 0;
  std::string origin_identifier(storage::GetIdentifierFromOrigin(origin_url));
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size);

  database_connections_.AddConnection(origin_identifier, database_name);
  Send(new DatabaseMsg_UpdateSize(origin, database_name, database_size));
}

void DatabaseMessageFilter::OnDatabaseModified(
    const url::Origin& origin,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!IsOriginValid(origin)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::DBMF_INVALID_ORIGIN_ON_MODIFIED);
    return;
  }

  std::string origin_identifier(
      storage::GetIdentifierFromOrigin(GURL(origin.Serialize())));
  if (!database_connections_.IsDatabaseOpened(origin_identifier,
                                              database_name)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::DBMF_DB_NOT_OPEN_ON_MODIFY);
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void DatabaseMessageFilter::OnDatabaseClosed(
    const url::Origin& origin,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!IsOriginValid(origin)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::DBMF_INVALID_ORIGIN_ON_CLOSED);
    return;
  }

  std::string origin_identifier(
      storage::GetIdentifierFromOrigin(GURL(origin.Serialize())));
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::DBMF_DB_NOT_OPEN_ON_CLOSE);
    return;
  }

  database_connections_.RemoveConnection(origin_identifier, database_name);
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
}

void DatabaseMessageFilter::OnHandleSqliteError(
    const url::Origin& origin,
    const base::string16& database_name,
    int error) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!IsOriginValid(origin)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::DBMF_INVALID_ORIGIN_ON_SQLITE_ERROR);
    return;
  }
  db_tracker_->HandleSqliteError(
      storage::GetIdentifierFromOrigin(GURL(origin.Serialize())), database_name,
      error);
}

void DatabaseMessageFilter::OnDatabaseSizeChanged(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int64_t database_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (database_connections_.IsOriginUsed(origin_identifier)) {
    Send(new DatabaseMsg_UpdateSize(
        url::Origin(storage::GetOriginFromIdentifier(origin_identifier)),
        database_name, database_size));
  }
}

void DatabaseMessageFilter::OnDatabaseScheduledForDeletion(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  Send(new DatabaseMsg_CloseImmediately(
      url::Origin(storage::GetOriginFromIdentifier(origin_identifier)),
      database_name));
}

}  // namespace content
