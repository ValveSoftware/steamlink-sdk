// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for screen orientation.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ScreenOrientationMsgStart

IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationType,
                              blink::WebScreenOrientationUndefined,
                              blink::WebScreenOrientationLandscapeSecondary)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationLockType,
                              blink::WebScreenOrientationLockDefault,
                              blink::WebScreenOrientationLockPortrait)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
      blink::WebLockOrientationCallback::ErrorType,
      blink::WebLockOrientationCallback::ErrorTypeNotAvailable,
      blink::WebLockOrientationCallback::ErrorTypeCanceled)

// The browser process informs the renderer process that the screen orientation
// has changed. |orientation| contains the new screen orientation in degrees.
// TODO(mlamouri): we could probably get rid of it.
IPC_MESSAGE_CONTROL1(ScreenOrientationMsg_OrientationChange,
                     blink::WebScreenOrientationType /* orientation */ )

// The browser process' response to a ScreenOrientationHostMsg_LockRequest when
// the lock actually succeeded. The message includes the new |angle| and |type|
// of orientation. The |request_id| passed when receiving the request is passed
// back so the renderer process can associate the response to the right request.
IPC_MESSAGE_ROUTED3(ScreenOrientationMsg_LockSuccess,
                    int, /* request_id */
                    unsigned, /* angle */
                    blink::WebScreenOrientationType /* type */)

// The browser process' response to a ScreenOrientationHostMsg_LockRequest when
// the lock actually failed. The message includes the |error| type. The
// |request_id| passed when receiving the request is passed back so the renderer
// process can associate the response to the right request.
IPC_MESSAGE_ROUTED2(ScreenOrientationMsg_LockError,
                    int, /* request_id */
                    blink::WebLockOrientationCallback::ErrorType /* error */);

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
