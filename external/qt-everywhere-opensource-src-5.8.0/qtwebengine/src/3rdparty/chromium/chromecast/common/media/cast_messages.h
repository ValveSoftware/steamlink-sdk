// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages related to media on Chromecast.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_START CastMediaMsgStart

// Messages sent from the browser to the renderer process.

IPC_MESSAGE_CONTROL1(CmaMsg_UpdateSupportedHdmiSinkCodecs,
                     int /* Codec support, bitmask of media_caps.h values */)
