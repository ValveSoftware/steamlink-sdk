// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ipc/latency_info_param_traits.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_
#include "ui/events/ipc/latency_info_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_
#include "ui/events/ipc/latency_info_param_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_
#include "ui/events/ipc/latency_info_param_traits.h"
}  // namespace IPC
