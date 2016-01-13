// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/sync_socket.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START TracingMsgStart

// Sent to all child processes to enable trace event recording.
IPC_MESSAGE_CONTROL3(TracingMsg_BeginTracing,
                     std::string /*  category_filter_str */,
                     base::TimeTicks /* browser_time */,
                     int /* base::debug::TraceLog::Options */)

// Sent to all child processes to disable trace event recording.
IPC_MESSAGE_CONTROL0(TracingMsg_EndTracing)

// Sent to all child processes to start monitoring.
IPC_MESSAGE_CONTROL3(TracingMsg_EnableMonitoring,
                     std::string /*  category_filter_str */,
                     base::TimeTicks /* browser_time */,
                     int /* base::debug::TraceLog::Options */)

// Sent to all child processes to stop monitoring.
IPC_MESSAGE_CONTROL0(TracingMsg_DisableMonitoring)

// Sent to all child processes to capture the current monitorint snapshot.
IPC_MESSAGE_CONTROL0(TracingMsg_CaptureMonitoringSnapshot)

// Sent to all child processes to get trace buffer fullness.
IPC_MESSAGE_CONTROL0(TracingMsg_GetTraceBufferPercentFull)

// Sent to all child processes to set watch event.
IPC_MESSAGE_CONTROL2(TracingMsg_SetWatchEvent,
                     std::string /* category_name */,
                     std::string /* event_name */)

// Sent to all child processes to clear watch event.
IPC_MESSAGE_CONTROL0(TracingMsg_CancelWatchEvent)

// Sent everytime when a watch event is matched.
IPC_MESSAGE_CONTROL0(TracingHostMsg_WatchEventMatched);

// Notify the browser that this child process supports tracing.
IPC_MESSAGE_CONTROL0(TracingHostMsg_ChildSupportsTracing)

// Reply from child processes acking TracingMsg_EndTracing.
IPC_MESSAGE_CONTROL1(TracingHostMsg_EndTracingAck,
                     std::vector<std::string> /* known_categories */)

// Reply from child processes acking TracingMsg_CaptureMonitoringSnapshot.
IPC_MESSAGE_CONTROL0(TracingHostMsg_CaptureMonitoringSnapshotAck)

// Child processes send back trace data in JSON chunks.
IPC_MESSAGE_CONTROL1(TracingHostMsg_TraceDataCollected,
                     std::string /*json trace data*/)

// Child processes send back trace data of the current monitoring
// in JSON chunks.
IPC_MESSAGE_CONTROL1(TracingHostMsg_MonitoringTraceDataCollected,
                     std::string /*json trace data*/)

// Reply to TracingMsg_GetTraceBufferPercentFull.
IPC_MESSAGE_CONTROL1(TracingHostMsg_TraceBufferPercentFullReply,
                     float /*trace buffer percent full*/)

