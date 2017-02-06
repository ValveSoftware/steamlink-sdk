// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the Media Session API.
// Multiply-included message file, hence no include guard.

#include "content/common/android/gin_java_bridge_errors.h"
#include "content/common/content_export.h"
#include "content/public/common/media_metadata.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaSessionMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::MediaMetadata)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(artist)
  IPC_STRUCT_TRAITS_MEMBER(album)
IPC_STRUCT_TRAITS_END()

// Messages for notifying the render process of media session status -------

IPC_MESSAGE_ROUTED2(MediaSessionMsg_DidActivate,
                    int /* request_id */,
                    bool /* success */)

IPC_MESSAGE_ROUTED1(MediaSessionMsg_DidDeactivate, int /* request_id */)

// Messages for controlling the media session in browser process ----------

IPC_MESSAGE_ROUTED2(MediaSessionHostMsg_Activate,
                    int /* session_id */,
                    int /* request_id */)

IPC_MESSAGE_ROUTED2(MediaSessionHostMsg_Deactivate,
                    int /* session_id */,
                    int /* request_id */)

IPC_MESSAGE_ROUTED2(MediaSessionHostMsg_SetMetadata,
                    int /* request_id*/,
                    content::MediaMetadata /* metadata */)
