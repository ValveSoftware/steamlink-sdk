// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the AEC dump.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START AecDumpMsgStart

// Messages sent from the browser to the renderer.

// The browser hands over a file handle to the consumer in the renderer
// identified by |id| to use for AEC dump.
IPC_MESSAGE_CONTROL2(AecDumpMsg_EnableAecDump,
                     int /* id */,
                     IPC::PlatformFileForTransit /* file_handle */)

// The browser hands over a file handle to the consumer in the renderer
// identified by |id| to use for the event log.
IPC_MESSAGE_CONTROL2(WebRTCEventLogMsg_EnableEventLog,
                     int /* id */,
                     IPC::PlatformFileForTransit /* file_handle */)

// Tell the renderer to disable AEC dump in all consumers.
IPC_MESSAGE_CONTROL0(AecDumpMsg_DisableAecDump)

// Tell the renderer to disable event log in all consumers.
IPC_MESSAGE_CONTROL0(WebRTCEventLogMsg_DisableEventLog)

// Messages sent from the renderer to the browser.

// Registers a consumer with the browser. The consumer will then get a file
// handle when the dump is enabled.
IPC_MESSAGE_CONTROL1(AecDumpMsg_RegisterAecDumpConsumer,
                     int /* id */)

// Registers a consumer with the browser. The consumer will then get a file
// handle when the dump is enabled.
IPC_MESSAGE_CONTROL1(WebRTCEventLogMsg_RegisterEventLogConsumer, int /* id */)

// Unregisters a consumer with the browser.
IPC_MESSAGE_CONTROL1(AecDumpMsg_UnregisterAecDumpConsumer,
                     int /* id */)

// Unregisters a consumer with the browser.
IPC_MESSAGE_CONTROL1(WebRTCEventLogMsg_UnregisterEventLogConsumer, int /* id */)
