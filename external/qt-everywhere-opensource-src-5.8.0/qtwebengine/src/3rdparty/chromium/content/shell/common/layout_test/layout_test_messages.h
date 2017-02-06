// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START LayoutTestMsgStart

IPC_SYNC_MESSAGE_ROUTED1_1(LayoutTestHostMsg_ReadFileToString,
                           base::FilePath /* local path */,
                           std::string /* contents */)
IPC_SYNC_MESSAGE_ROUTED1_1(LayoutTestHostMsg_RegisterIsolatedFileSystem,
                           std::vector<base::FilePath> /* absolute_filenames */,
                           std::string /* filesystem_id */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_ClearAllDatabases)
IPC_MESSAGE_ROUTED1(LayoutTestHostMsg_SetDatabaseQuota,
                    int /* quota */)
IPC_MESSAGE_ROUTED2(LayoutTestHostMsg_SimulateWebNotificationClick,
                    std::string /* title */,
                    int /* action_index */)
IPC_MESSAGE_ROUTED2(LayoutTestHostMsg_SimulateWebNotificationClose,
                    std::string /* title */,
                    bool /* by_user */)
IPC_MESSAGE_ROUTED1(LayoutTestHostMsg_AcceptAllCookies,
                    bool /* accept */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_DeleteAllCookies)
IPC_MESSAGE_ROUTED4(LayoutTestHostMsg_SetPermission,
                    std::string /* name */,
                    blink::mojom::PermissionStatus /* status */,
                    GURL /* origin */,
                    GURL /* embedding_origin */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_ResetPermissions)

// Notifies the browser that one of renderers has changed layout test runtime
// flags (i.e. has set dump_as_text).
IPC_MESSAGE_CONTROL1(
    LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged,
    base::DictionaryValue /* changed_layout_test_runtime_flags */)

// Used send flag changes to renderers - either when
// 1) broadcasting change happening in one renderer to all other renderers, or
// 2) sending accumulated changes to a single new renderer.
IPC_MESSAGE_CONTROL1(
    LayoutTestMsg_ReplicateLayoutTestRuntimeFlagsChanges,
    base::DictionaryValue /* changed_layout_test_runtime_flags */)

// Sent by secondary test window to notify the test has finished.
IPC_MESSAGE_CONTROL0(LayoutTestHostMsg_TestFinishedInSecondaryRenderer)
