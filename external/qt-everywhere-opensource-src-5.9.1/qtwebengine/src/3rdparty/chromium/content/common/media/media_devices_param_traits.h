// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included IPC traits file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaDeviceType,
                          content::NUM_MEDIA_DEVICE_TYPES - 1)

IPC_STRUCT_TRAITS_BEGIN(content::MediaDeviceInfo)
  IPC_STRUCT_TRAITS_MEMBER(device_id)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(group_id)
IPC_STRUCT_TRAITS_END()
