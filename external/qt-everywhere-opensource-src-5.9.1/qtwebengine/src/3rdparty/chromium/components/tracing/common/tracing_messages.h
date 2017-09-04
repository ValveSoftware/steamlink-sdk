// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/sync_socket.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event_impl.h"
#include "components/tracing/tracing_export.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT TRACING_EXPORT
#define IPC_MESSAGE_START TracingMsgStart

IPC_STRUCT_TRAITS_BEGIN(base::trace_event::TraceLogStatus)
IPC_STRUCT_TRAITS_MEMBER(event_capacity)
IPC_STRUCT_TRAITS_MEMBER(event_count)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(base::trace_event::MemoryDumpRequestArgs)
IPC_STRUCT_TRAITS_MEMBER(dump_guid)
IPC_STRUCT_TRAITS_MEMBER(dump_type)
IPC_STRUCT_TRAITS_MEMBER(level_of_detail)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(base::trace_event::MemoryDumpArgs)
  IPC_STRUCT_TRAITS_MEMBER(level_of_detail)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(base::trace_event::MemoryDumpLevelOfDetail,
                          base::trace_event::MemoryDumpLevelOfDetail::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(
    base::trace_event::MemoryDumpType,
    static_cast<int>(base::trace_event::MemoryDumpType::LAST))

// Sent to all child processes to enable trace event recording.
IPC_MESSAGE_CONTROL3(TracingMsg_BeginTracing,
                     std::string /*  trace_config_str */,
                     base::TimeTicks /* browser_time */,
                     uint64_t /* Tracing process id (hash of child id) */)

// Sent to all child processes to disable trace event recording.
IPC_MESSAGE_CONTROL0(TracingMsg_EndTracing)

// Sent to all child processes to cancel trace event recording.
IPC_MESSAGE_CONTROL0(TracingMsg_CancelTracing)

// Sent to all child processes to get trace buffer fullness.
IPC_MESSAGE_CONTROL0(TracingMsg_GetTraceLogStatus)

// Sent to all child processes to request a local (current process) memory dump.
IPC_MESSAGE_CONTROL1(TracingMsg_ProcessMemoryDumpRequest,
                     base::trace_event::MemoryDumpRequestArgs)

// Reply to TracingHostMsg_GlobalMemoryDumpRequest, sent by the browser process.
// This is to get the result of a global dump initiated by a child process.
IPC_MESSAGE_CONTROL2(TracingMsg_GlobalMemoryDumpResponse,
                     uint64_t /* dump_guid */,
                     bool /* success */)

IPC_MESSAGE_CONTROL4(TracingMsg_SetUMACallback,
                     std::string /* histogram_name */,
                     base::HistogramBase::Sample /* histogram_lower_value */,
                     base::HistogramBase::Sample /* histogram_uppwer_value */,
                     bool /* repeat */)

IPC_MESSAGE_CONTROL1(TracingMsg_ClearUMACallback,
                     std::string /* histogram_name */)

// Notify the browser that this child process supports tracing.
IPC_MESSAGE_CONTROL0(TracingHostMsg_ChildSupportsTracing)

// Reply from child processes acking TracingMsg_EndTracing.
IPC_MESSAGE_CONTROL1(TracingHostMsg_EndTracingAck,
                     std::vector<std::string> /* known_categories */)

// Child processes send back trace data in JSON chunks.
IPC_MESSAGE_CONTROL1(TracingHostMsg_TraceDataCollected,
                     std::string /*json trace data*/)

// Reply to TracingMsg_GetTraceLogStatus.
IPC_MESSAGE_CONTROL1(
    TracingHostMsg_TraceLogStatusReply,
    base::trace_event::TraceLogStatus /*status of the trace log*/)

// Sent to the browser to initiate a global memory dump from a child process.
IPC_MESSAGE_CONTROL1(TracingHostMsg_GlobalMemoryDumpRequest,
                     base::trace_event::MemoryDumpRequestArgs)

// Reply to TracingMsg_ProcessMemoryDumpRequest.
IPC_MESSAGE_CONTROL2(TracingHostMsg_ProcessMemoryDumpResponse,
                     uint64_t /* dump_guid */,
                     bool /* success */)

IPC_MESSAGE_CONTROL1(TracingHostMsg_TriggerBackgroundTrace,
                     std::string /* name */)

IPC_MESSAGE_CONTROL0(TracingHostMsg_AbortBackgroundTrace)
