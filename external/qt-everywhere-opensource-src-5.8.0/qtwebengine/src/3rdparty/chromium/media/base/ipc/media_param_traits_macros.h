// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_macros.h"
#include "media/base/audio_parameters.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_types.h"

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioParameters::Format,
                          media::AudioParameters::AUDIO_FORMAT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::ChannelLayout, media::CHANNEL_LAYOUT_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelFormat, media::PIXEL_FORMAT_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelStorage, media::PIXEL_STORAGE_MAX)

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
