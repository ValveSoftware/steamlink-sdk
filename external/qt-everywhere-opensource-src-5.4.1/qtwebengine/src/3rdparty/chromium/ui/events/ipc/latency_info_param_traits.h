// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_
#define UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_

#include "ipc/ipc_message_macros.h"
#include "ui/events/events_export.h"
#include "ui/events/latency_info.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT EVENTS_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(ui::LatencyComponentType,
                          ui::LATENCY_COMPONENT_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::LatencyInfo::LatencyComponent)
  IPC_STRUCT_TRAITS_MEMBER(sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(event_time)
  IPC_STRUCT_TRAITS_MEMBER(event_count)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::LatencyInfo)
  IPC_STRUCT_TRAITS_MEMBER(latency_components)
  IPC_STRUCT_TRAITS_MEMBER(trace_id)
  IPC_STRUCT_TRAITS_MEMBER(terminated)
IPC_STRUCT_TRAITS_END()

#endif // UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_H_
