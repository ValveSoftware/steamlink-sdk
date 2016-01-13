// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_param.h"
#include "content/common/cc_messages.h"

#define IPC_MESSAGE_IMPL
#include "content/common/frame_param_macros.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#include "content/common/frame_param_macros.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#include "content/common/frame_param_macros.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#include "content/common/frame_param_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#include "content/common/frame_param_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#include "content/common/frame_param_macros.h"
}  // namespace IPC
