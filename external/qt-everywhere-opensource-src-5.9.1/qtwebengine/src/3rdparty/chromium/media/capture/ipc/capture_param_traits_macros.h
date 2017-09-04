// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_IPC_CAPTURE_PARAM_TRAITS_MACROS_H_
#define MEDIA_CAPTURE_IPC_CAPTURE_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_macros.h"
#include "media/capture/video_capture_types.h"

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelStorage, media::PIXEL_STORAGE_MAX)

#endif  // MEDIA_CAPTURE_IPC_CAPTURE_PARAM_TRAITS_MACROS_H_
