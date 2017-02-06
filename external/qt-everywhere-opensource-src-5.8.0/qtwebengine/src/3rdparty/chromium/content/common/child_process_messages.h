// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common IPC messages used for child processes.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits_macros.h"
#include "content/common/gpu_process_launch_causes.h"
#include "content/common/host_discardable_shared_memory_manager.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

IPC_ENUM_TRAITS_MAX_VALUE(content::CauseForGpuLaunch,
                          content::CAUSE_FOR_GPU_LAUNCH_MAX_ENUM - 1)

IPC_ENUM_TRAITS_MAX_VALUE(tracked_objects::ThreadData::Status,
                          tracked_objects::ThreadData::STATUS_LAST)

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::LocationSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(file_name)
  IPC_STRUCT_TRAITS_MEMBER(function_name)
  IPC_STRUCT_TRAITS_MEMBER(line_number)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::BirthOnThreadSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(thread_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::DeathDataSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(run_duration_sum)
  IPC_STRUCT_TRAITS_MEMBER(run_duration_max)
  IPC_STRUCT_TRAITS_MEMBER(run_duration_sample)
  IPC_STRUCT_TRAITS_MEMBER(queue_duration_sum)
  IPC_STRUCT_TRAITS_MEMBER(queue_duration_max)
  IPC_STRUCT_TRAITS_MEMBER(queue_duration_sample)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::TaskSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(birth)
  IPC_STRUCT_TRAITS_MEMBER(death_data)
  IPC_STRUCT_TRAITS_MEMBER(death_thread_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::ProcessDataPhaseSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(tasks)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::ProcessDataSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(phased_snapshots)
  IPC_STRUCT_TRAITS_MEMBER(process_id)
IPC_STRUCT_TRAITS_END()

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ChildProcessMsgStart

// Messages sent from the browser to the child process.

// Sent in response to ChildProcessHostMsg_ShutdownRequest to tell the child
// process that it's safe to shutdown.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_Shutdown)

#if defined(IPC_MESSAGE_LOG_ENABLED)
// Tell the child process to begin or end IPC message logging.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetIPCLoggingEnabled,
                     bool /* on or off */)
#endif

// Tell the child process to enable or disable the profiler status.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetProfilerStatus,
                     tracked_objects::ThreadData::Status /* profiler status */)

// Send to all the child processes to send back profiler data (ThreadData in
// tracked_objects).
IPC_MESSAGE_CONTROL2(ChildProcessMsg_GetChildProfilerData,
                     int /* sequence_number */,
                     int /* current_profiling_phase */)

// Send to all the child processes to mark the current profiling phase as
// finished and start a new phase.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_ProfilingPhaseCompleted,
                     int /* profiling_phase */)

// Sent to set the shared memory buffer to be used for storing histograms that
// are to be reported by the browser process to UMA. The following message
// (GetChildNonPersistentHistogramData) will return any histograms created
// before this message is received but not any histograms created afterward.
IPC_MESSAGE_CONTROL2(ChildProcessMsg_SetHistogramMemory,
                     base::SharedMemoryHandle /* shm_handle */,
                     int /* shm_size */)

// Send to all the child processes to send back histogram data.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_GetChildNonPersistentHistogramData,
                     int /* sequence_number */)

// Sent to child processes to tell them to enter or leave background mode.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetProcessBackgrounded,
                     bool /* background */)

// Sent to child processes to tell them to purge and suspend.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_PurgeAndSuspend)

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the child process to the browser.

// A renderer sends this when it wants to create a connection to the GPU
// process. The browser will create the GPU process if necessary, and will
// return a handle to the channel via a GpuChannelEstablished message.
IPC_SYNC_MESSAGE_CONTROL1_3(ChildProcessHostMsg_EstablishGpuChannel,
                            content::CauseForGpuLaunch,
                            int /* client id */,
                            IPC::ChannelHandle /* handle to channel */,
                            gpu::GPUInfo /* stats about GPU process*/)

// A renderer sends this when it wants to know whether a gpu process exists.
IPC_SYNC_MESSAGE_CONTROL0_1(ChildProcessHostMsg_HasGpuProcess,
                            bool /* result */)

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Send back profiler data (ThreadData in tracked_objects).
IPC_MESSAGE_CONTROL2(
    ChildProcessHostMsg_ChildProfilerData,
    int, /* sequence_number */
    tracked_objects::ProcessDataSnapshot /* process_data_snapshot */)

// Send back histograms as vector of pickled-histogram strings.
IPC_MESSAGE_CONTROL2(ChildProcessHostMsg_ChildHistogramData,
                     int, /* sequence_number */
                     std::vector<std::string> /* histogram_data */)

// Request a histogram from the browser. The browser will send the histogram
// data only if it has been passed the command line flag
// switches::kDomAutomationController.
IPC_SYNC_MESSAGE_CONTROL1_1(ChildProcessHostMsg_GetBrowserHistogram,
                            std::string, /* histogram_name */
                            std::string /* histogram_json */)

#if defined(OS_WIN)
// Request that the given font be loaded by the host so it's cached by the
// OS. Please see ChildProcessHost::PreCacheFont for details.
IPC_SYNC_MESSAGE_CONTROL1_0(ChildProcessHostMsg_PreCacheFont,
                            LOGFONT /* font data */)

// Release the cached font
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ReleaseCachedFonts)
#endif  // defined(OS_WIN)

// Asks the browser to create a block of shared memory for the child process to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL1_1(ChildProcessHostMsg_SyncAllocateSharedMemory,
                            uint32_t /* buffer size */,
                            base::SharedMemoryHandle)

// Asks the browser to create a block of shared memory for the child process to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL2_1(ChildProcessHostMsg_SyncAllocateSharedBitmap,
                            uint32_t /* buffer size */,
                            cc::SharedBitmapId,
                            base::SharedMemoryHandle)

// Informs the browser that the child allocated a shared bitmap.
IPC_MESSAGE_CONTROL3(ChildProcessHostMsg_AllocatedSharedBitmap,
                     uint32_t /* buffer size */,
                     base::SharedMemoryHandle,
                     cc::SharedBitmapId)

// Informs the browser that the child deleted a shared bitmap.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_DeletedSharedBitmap,
                     cc::SharedBitmapId)

// Asks the browser to create a gpu memory buffer.
IPC_SYNC_MESSAGE_CONTROL5_1(ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer,
                            gfx::GpuMemoryBufferId /* new_id */,
                            uint32_t /* width */,
                            uint32_t /* height */,
                            gfx::BufferFormat,
                            gfx::BufferUsage,
                            gfx::GpuMemoryBufferHandle)

// Informs the browser that the child deleted a gpu memory buffer.
IPC_MESSAGE_CONTROL2(ChildProcessHostMsg_DeletedGpuMemoryBuffer,
                     gfx::GpuMemoryBufferId,
                     gpu::SyncToken /* sync_token */)

// Asks the browser to create a block of discardable shared memory for the
// child process.
IPC_SYNC_MESSAGE_CONTROL2_1(
    ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory,
    uint32_t /* size */,
    content::DiscardableSharedMemoryId,
    base::SharedMemoryHandle)

// Informs the browser that the child deleted a block of discardable shared
// memory.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_DeletedDiscardableSharedMemory,
                     content::DiscardableSharedMemoryId)
