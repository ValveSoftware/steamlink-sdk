// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common IPC messages used for child processes.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/gpu_memory_buffer.h"

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

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::ParentChildPairSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(parent)
  IPC_STRUCT_TRAITS_MEMBER(child)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(tracked_objects::ProcessDataSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(tasks)
  IPC_STRUCT_TRAITS_MEMBER(descendants)
  IPC_STRUCT_TRAITS_MEMBER(process_id)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(gfx::GpuMemoryBufferType,
                          gfx::GPU_MEMORY_BUFFER_TYPE_LAST)

#if defined(OS_ANDROID)
IPC_STRUCT_TRAITS_BEGIN(gfx::SurfaceTextureId)
  IPC_STRUCT_TRAITS_MEMBER(primary_id)
  IPC_STRUCT_TRAITS_MEMBER(secondary_id)
IPC_STRUCT_TRAITS_END()
#endif

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferId)
  IPC_STRUCT_TRAITS_MEMBER(primary_id)
  IPC_STRUCT_TRAITS_MEMBER(secondary_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferHandle)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(global_id)
#if defined(OS_MACOSX)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_id)
#endif
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(surface_texture_id)
#endif
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
IPC_MESSAGE_CONTROL1(ChildProcessMsg_GetChildProfilerData,
                     int /* sequence_number */)

// Send to all the child processes to send back histogram data.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_GetChildHistogramData,
                     int /* sequence_number */)

// Sent to child processes to dump their handle table.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_DumpHandles)

// Sent to child processes to tell them to enter or leave background mode.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetProcessBackgrounded,
                     bool /* background */)

#if defined(USE_TCMALLOC)
// Sent to child process to request tcmalloc stats.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_GetTcmallocStats)
#endif

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Send back profiler data (ThreadData in tracked_objects).
IPC_MESSAGE_CONTROL2(ChildProcessHostMsg_ChildProfilerData,
                     int, /* sequence_number */
                     tracked_objects::ProcessDataSnapshot /* profiler_data */)

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

// Reply to ChildProcessMsg_DumpHandles when handle table dump is complete.
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_DumpHandlesDone)

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
                            uint32 /* buffer size */,
                            base::SharedMemoryHandle)

// Asks the browser to create a block of shared memory for the child process to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL2_1(ChildProcessHostMsg_SyncAllocateSharedBitmap,
                            uint32 /* buffer size */,
                            cc::SharedBitmapId,
                            base::SharedMemoryHandle)

// Informs the browser that the child allocated a shared bitmap.
IPC_MESSAGE_CONTROL3(ChildProcessHostMsg_AllocatedSharedBitmap,
                     uint32 /* buffer size */,
                     base::SharedMemoryHandle,
                     cc::SharedBitmapId)

// Informs the browser that the child deleted a shared bitmap.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_DeletedSharedBitmap,
                     cc::SharedBitmapId)

#if defined(USE_TCMALLOC)
// Reply to ChildProcessMsg_GetTcmallocStats.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_TcmallocStats,
                     std::string /* output */)
#endif

// Asks the browser to create a gpu memory buffer.
IPC_SYNC_MESSAGE_CONTROL4_1(ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer,
                            uint32 /* width */,
                            uint32 /* height */,
                            uint32 /* internalformat */,
                            uint32 /* usage */,
                            gfx::GpuMemoryBufferHandle)
