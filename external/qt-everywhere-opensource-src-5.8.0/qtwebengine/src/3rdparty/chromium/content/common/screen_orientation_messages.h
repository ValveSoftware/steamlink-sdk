// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for screen orientation.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationError.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ScreenOrientationMsgStart

IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationLockType,
                              blink::WebScreenOrientationLockDefault,
                              blink::WebScreenOrientationLockNatural)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
      blink::WebLockOrientationError,
      blink::WebLockOrientationErrorNotAvailable,
      blink::WebLockOrientationErrorCanceled)

// The browser process' response to a ScreenOrientationHostMsg_LockRequest when
// the lock actually succeeded. The |request_id| passed when receiving the
// request is passed back so the renderer process can associate the response to
// the right request.
IPC_MESSAGE_ROUTED1(ScreenOrientationMsg_LockSuccess,
                    int /* request_id */)

// The browser process' response to a ScreenOrientationHostMsg_LockRequest when
// the lock actually failed. The message includes the |error| type. The
// |request_id| passed when receiving the request is passed back so the renderer
// process can associate the response to the right request.
IPC_MESSAGE_ROUTED2(ScreenOrientationMsg_LockError,
                    int, /* request_id */
                    blink::WebLockOrientationError /* error */)

// The renderer process requests the browser process to lock the screen
// orientation to the specified |orientations|. The request contains a
// |request_id| that will have to be passed back to the renderer process when
// notifying about a success or error (see ScreenOrientationMsg_LockError and
// ScreenOrientationMsg_LockSuccess).
IPC_MESSAGE_ROUTED2(ScreenOrientationHostMsg_LockRequest,
                    blink::WebScreenOrientationLockType, /* orientation */
                    int /* request_id */)

// The renderer process requests the browser process to unlock the screen
// orientation.
IPC_MESSAGE_ROUTED0(ScreenOrientationHostMsg_Unlock)

// The renderer process is now using the Screen Orientation API and informs the
// browser process that it should start accurately listening to the screen
// orientation if it wasn't already.
// This is only expected to be acted upon when the underlying platform requires
// heavy work in order to accurately know the screen orientation.
IPC_MESSAGE_CONTROL0(ScreenOrientationHostMsg_StartListening)

// The renderer process is no longer using the Screen Orientation API and
// informs the browser process that it can stop accurately listening to the
// screen orientation if no other process cares about it.
// This is only expected to be acted upon when the underlying platform requires
// heavy work in order to accurately know the screen orientation.
IPC_MESSAGE_CONTROL0(ScreenOrientationHostMsg_StopListening)
