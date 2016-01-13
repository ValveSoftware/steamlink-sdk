// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DatabaseMsgStart

// Database messages sent from the browser to the renderer.

// Notifies the child process of the new database size
IPC_MESSAGE_CONTROL3(DatabaseMsg_UpdateSize,
                     std::string /* the origin */,
                     base::string16 /* the database name */,
                     int64 /* the new database size */)

// Notifies the child process of the new space available
IPC_MESSAGE_CONTROL2(DatabaseMsg_UpdateSpaceAvailable,
                     std::string /* the origin */,
                     int64 /* space available to origin */)

// Notifies the child process to reset it's cached value for the origin.
IPC_MESSAGE_CONTROL1(DatabaseMsg_ResetSpaceAvailable,
                     std::string /* the origin */)

// Asks the child process to close a database immediately
IPC_MESSAGE_CONTROL2(DatabaseMsg_CloseImmediately,
                     std::string /* the origin */,
                     base::string16 /* the database name */)

// Database messages sent from the renderer to the browser.

// Asks the browser process to open a DB file with the given name.
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_OpenFile,
                            base::string16 /* vfs file name */,
                            int /* desired flags */,
                            IPC::PlatformFileForTransit /* file_handle */)

// Asks the browser process to delete a DB file
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_DeleteFile,
                            base::string16 /* vfs file name */,
                            bool /* whether or not to sync the directory */,
                            int /* SQLite error code */)

// Asks the browser process to return the attributes of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileAttributes,
                            base::string16 /* vfs file name */,
                            int32 /* the attributes for the given DB file */)

// Asks the browser process to return the size of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileSize,
                            base::string16 /* vfs file name */,
                            int64 /* the size of the given DB file */)

// Asks the browser process for the amount of space available to an origin
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetSpaceAvailable,
                            std::string /* origin identifier */,
                            int64 /* remaining space available */)

// Notifies the browser process that a new database has been opened
IPC_MESSAGE_CONTROL4(DatabaseHostMsg_Opened,
                     std::string /* origin identifier */,
                     base::string16 /* database name */,
                     base::string16 /* database description */,
                     int64 /* estimated size */)

// Notifies the browser process that a database might have been modified
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Modified,
                     std::string /* origin identifier */,
                     base::string16 /* database name */)

// Notifies the browser process that a database is about to close
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Closed,
                     std::string /* origin identifier */,
                     base::string16 /* database name */)

// Sent when a sqlite error indicates the database is corrupt.
IPC_MESSAGE_CONTROL3(DatabaseHostMsg_HandleSqliteError,
                     std::string /* origin identifier */,
                     base::string16 /* database name */,
                     int  /* error */)
