// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending upon circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#define UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_ipc_export.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/swap_result.h"
#include "ipc/ipc_message_macros.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap_handle_ozone.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gfx::BufferFormat, gfx::BufferFormat::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::BufferUsage, gfx::BufferUsage::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::GpuMemoryBufferType,
                          gfx::GPU_MEMORY_BUFFER_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SwapResult, gfx::SwapResult::SWAP_RESULT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SelectionBound::Type, gfx::SelectionBound::LAST);

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferHandle)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(stride)
#if defined(USE_OZONE)
  IPC_STRUCT_TRAITS_MEMBER(native_pixmap_handle)
#elif defined(OS_MACOSX)
  IPC_STRUCT_TRAITS_MEMBER(mach_port)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferId)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

#if defined(USE_OZONE)
IPC_STRUCT_TRAITS_BEGIN(gfx::NativePixmapHandle)
  IPC_STRUCT_TRAITS_MEMBER(fds)
  IPC_STRUCT_TRAITS_MEMBER(strides_and_offsets)
IPC_STRUCT_TRAITS_END()
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
