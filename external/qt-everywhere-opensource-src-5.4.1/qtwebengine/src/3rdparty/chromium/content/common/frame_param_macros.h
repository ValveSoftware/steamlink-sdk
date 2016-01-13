// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Allows for sharing of IPC param structures between BrowserPlugin code and
// RenderFrame code. All these should be folded directly back into the IPCs in
// frame_messages.h once BrowserPlugin has been fully converted over to use
// the RenderFrame infrastructure.
//
// TODO(ajwong): Remove once BrowserPlugin has been converted to use
// RenderFrames. http://crbug.com/330264

#ifndef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#define CONTENT_COMMON_FRAME_PARAM_MACROS_H_

#include "cc/output/compositor_frame_ack.h"
#include "cc/output/compositor_frame.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_STRUCT_BEGIN(FrameMsg_BuffersSwapped_Params)
  IPC_STRUCT_MEMBER(int, gpu_host_id)
  IPC_STRUCT_MEMBER(int, gpu_route_id)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(gfx::Size, size)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameMsg_CompositorFrameSwapped_Params)
  // Specifies which RenderWidget produced the CompositorFrame.
  IPC_STRUCT_MEMBER(int, producing_host_id)
  IPC_STRUCT_MEMBER(int, producing_route_id)

  IPC_STRUCT_MEMBER(cc::CompositorFrame, frame)
  IPC_STRUCT_MEMBER(uint32, output_surface_id)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, shared_memory_handle)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_BuffersSwappedACK_Params)
  IPC_STRUCT_MEMBER(int, gpu_host_id)
  IPC_STRUCT_MEMBER(int, gpu_route_id)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(uint32, sync_point)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_CompositorFrameSwappedACK_Params)
  // Specifies which RenderWidget produced the CompositorFrame.
  IPC_STRUCT_MEMBER(int, producing_host_id)
  IPC_STRUCT_MEMBER(int, producing_route_id)

  IPC_STRUCT_MEMBER(uint32, output_surface_id)
  IPC_STRUCT_MEMBER(cc::CompositorFrameAck, ack)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_ReclaimCompositorResources_Params)
  IPC_STRUCT_MEMBER(int, route_id)
  IPC_STRUCT_MEMBER(uint32, output_surface_id)
  IPC_STRUCT_MEMBER(int, renderer_host_id)
  IPC_STRUCT_MEMBER(cc::CompositorFrameAck, ack)
IPC_STRUCT_END()

#endif  // CONTENT_COMMON_FRAME_PARAM_MACROS_H_
