// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the Cast Media Acceleration (CMA) pipeline.
// Multiply-included message file, hence no include guard.

#include <stddef.h>

#include "chromecast/common/media/cma_ipc_common.h"
#include "chromecast/common/media/cma_param_traits.h"
#include "chromecast/common/media/cma_param_traits_macros.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_START CastMediaMsgStart

// Messages sent from the renderer to the browser process.

IPC_MESSAGE_CONTROL2(CmaHostMsg_CreateMedia,
                     int /* Media pipeline ID */,
                     chromecast::media::LoadType /* Load type */)
IPC_MESSAGE_CONTROL1(CmaHostMsg_DestroyMedia,
                     int /* Media pipeline ID */)
IPC_MESSAGE_CONTROL3(CmaHostMsg_SetCdm,
                     int /* Media pipeline ID */,
                     int /* render_frame_id */,
                     int /* cdm_id */)
IPC_MESSAGE_CONTROL2(CmaHostMsg_StartPlayingFrom,
                     int /* Media pipeline ID */,
                     base::TimeDelta /* Timestamp */)
IPC_MESSAGE_CONTROL1(CmaHostMsg_Flush,
                     int /* Media pipeline ID */)
IPC_MESSAGE_CONTROL1(CmaHostMsg_Stop,
                     int /* Media pipeline ID */)
IPC_MESSAGE_CONTROL2(CmaHostMsg_SetPlaybackRate,
                     int /* Media pipeline ID */,
                     double /* Playback rate */)

IPC_MESSAGE_CONTROL3(CmaHostMsg_CreateAvPipe,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     size_t /* Fifo size */)
IPC_MESSAGE_CONTROL3(CmaHostMsg_AudioInitialize,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     ::media::AudioDecoderConfig /* Audio config */)
IPC_MESSAGE_CONTROL3(CmaHostMsg_VideoInitialize,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     /* Video Config */
                     std::vector<::media::VideoDecoderConfig>)
IPC_MESSAGE_CONTROL3(CmaHostMsg_SetVolume,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     float /* Volume */)
IPC_MESSAGE_CONTROL2(CmaHostMsg_NotifyPipeWrite,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */)

// Messages from the browser to the renderer process.

IPC_MESSAGE_CONTROL1(CmaMsg_FlushDone, int /* Media pipeline ID */)
IPC_MESSAGE_CONTROL4(CmaMsg_TimeUpdate,
                     int /* Media pipeline ID */,
                     base::TimeDelta /* Media time */,
                     base::TimeDelta /* Max media time */,
                     base::TimeTicks /* STC */)
IPC_MESSAGE_CONTROL2(CmaMsg_BufferingNotification,
                     int /* Media pipeline ID */,
                     media::BufferingState /* Buffering state */)

IPC_MESSAGE_CONTROL5(CmaMsg_AvPipeCreated,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     bool /* Status */,
                     base::SharedMemoryHandle /* Shared memory */,
                     base::FileDescriptor /* socket handle */)
IPC_MESSAGE_CONTROL3(CmaMsg_TrackStateChanged,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     media::PipelineStatus /* Status */)
IPC_MESSAGE_CONTROL2(CmaMsg_NotifyPipeRead,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */)

IPC_MESSAGE_CONTROL2(CmaMsg_WaitForKey,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */)
IPC_MESSAGE_CONTROL2(CmaMsg_Eos,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */)
IPC_MESSAGE_CONTROL3(CmaMsg_PlaybackError,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     media::PipelineStatus /* status */)
IPC_MESSAGE_CONTROL3(CmaMsg_PlaybackStatistics,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     media::PipelineStatistics /* status */)
IPC_MESSAGE_CONTROL3(CmaMsg_NaturalSizeChanged,
                     int /* Media pipeline ID */,
                     chromecast::media::TrackId /* Track ID */,
                     gfx::Size /* Size */)
