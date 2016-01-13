// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for desktop notification.
// Multiply-included message file, hence no include guard.

#include "content/public/common/show_desktop_notification_params.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START DesktopNotificationMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::ShowDesktopNotificationHostMsgParams)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(icon_url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(body)
  IPC_STRUCT_TRAITS_MEMBER(direction)
  IPC_STRUCT_TRAITS_MEMBER(replace_id)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Used to inform the renderer that the browser has displayed its
// requested notification.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PostDisplay,
                    int /* notification_id */)

// Used to inform the renderer that the browser has encountered an error
// trying to display a notification.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PostError,
                    int /* notification_id */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED2(DesktopNotificationMsg_PostClose,
                    int /* notification_id */,
                    bool /* by_user */)

// Informs the renderer that one of its notifications was clicked on.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PostClick,
                    int /* notification_id */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED1(DesktopNotificationMsg_PermissionRequestDone,
                    int /* request_id */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED2(DesktopNotificationHostMsg_Show,
                    int /* notification_id */,
                    content::ShowDesktopNotificationHostMsgParams /* params */)

IPC_MESSAGE_ROUTED1(DesktopNotificationHostMsg_Cancel,
                    int /* notification_id */)

IPC_MESSAGE_ROUTED2(DesktopNotificationHostMsg_RequestPermission,
                    GURL /* origin */,
                    int /* callback_context */)

IPC_SYNC_MESSAGE_ROUTED1_1(DesktopNotificationHostMsg_CheckPermission,
                           GURL /* origin */,
                           int /* permission_result */)
