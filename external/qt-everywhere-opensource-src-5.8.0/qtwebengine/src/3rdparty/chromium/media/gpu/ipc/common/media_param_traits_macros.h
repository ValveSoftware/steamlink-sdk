// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_

#include "gpu/config/gpu_info.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/decrypt_config.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "media/base/video_codecs.h"
#include "media/gpu/ipc/common/create_video_encoder_params.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::JpegDecodeAccelerator::Error,
                          media::JpegDecodeAccelerator::LARGEST_ERROR_ENUM)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoEncodeAccelerator::Error,
                          media::VideoEncodeAccelerator::kErrorMax)

IPC_STRUCT_TRAITS_BEGIN(media::VideoDecodeAccelerator::Config)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(is_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(cdm_id)
  IPC_STRUCT_TRAITS_MEMBER(is_deferred_initialization_allowed)
  IPC_STRUCT_TRAITS_MEMBER(surface_id)
  IPC_STRUCT_TRAITS_MEMBER(initial_expected_coded_size)
  IPC_STRUCT_TRAITS_MEMBER(supported_output_formats)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::CreateVideoEncoderParams)
  IPC_STRUCT_TRAITS_MEMBER(input_format)
  IPC_STRUCT_TRAITS_MEMBER(input_visible_size)
  IPC_STRUCT_TRAITS_MEMBER(output_profile)
  IPC_STRUCT_TRAITS_MEMBER(initial_bitrate)
  IPC_STRUCT_TRAITS_MEMBER(encoder_route_id)
IPC_STRUCT_TRAITS_END()

#endif  // MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
