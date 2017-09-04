// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/database_util.h"

#include "content/common/database_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/sqlite/sqlite3.h"

using blink::Platform;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

Platform::FileHandle DatabaseUtil::DatabaseOpenFile(
    const WebString& vfs_file_name,
    int desired_flags,
    IPC::SyncMessageFilter* sync_message_filter) {
  IPC::PlatformFileForTransit file_handle =
      IPC::InvalidPlatformFileForTransit();

  sync_message_filter->Send(new DatabaseHostMsg_OpenFile(
      vfs_file_name, desired_flags, &file_handle));

  return IPC::PlatformFileForTransitToPlatformFile(file_handle);
}

int DatabaseUtil::DatabaseDeleteFile(
    const WebString& vfs_file_name,
    bool sync_dir,
    IPC::SyncMessageFilter* sync_message_filter) {
  int rv = SQLITE_IOERR_DELETE;
  sync_message_filter->Send(
      new DatabaseHostMsg_DeleteFile(vfs_file_name, sync_dir, &rv));
  return rv;
}

long DatabaseUtil::DatabaseGetFileAttributes(
    const WebString& vfs_file_name,
    IPC::SyncMessageFilter* sync_message_filter) {
  int32_t rv = -1;
  sync_message_filter->Send(
      new DatabaseHostMsg_GetFileAttributes(vfs_file_name, &rv));
  return rv;
}

long long DatabaseUtil::DatabaseGetFileSize(
    const WebString& vfs_file_name,
    IPC::SyncMessageFilter* sync_message_filter) {
  int64_t rv = 0LL;
  sync_message_filter->Send(
      new DatabaseHostMsg_GetFileSize(vfs_file_name, &rv));
  return rv;
}

long long DatabaseUtil::DatabaseGetSpaceAvailable(
    const WebSecurityOrigin& origin,
    IPC::SyncMessageFilter* sync_message_filter) {
  int64_t rv = 0LL;
  sync_message_filter->Send(new DatabaseHostMsg_GetSpaceAvailable(origin, &rv));
  return rv;
}

bool DatabaseUtil::DatabaseSetFileSize(
    const WebString& vfs_file_name,
    int64_t size,
    IPC::SyncMessageFilter* sync_message_filter) {
  bool rv = false;
  sync_message_filter->Send(
      new DatabaseHostMsg_SetFileSize(vfs_file_name, size, &rv));
  return rv;
}

}  // namespace content
