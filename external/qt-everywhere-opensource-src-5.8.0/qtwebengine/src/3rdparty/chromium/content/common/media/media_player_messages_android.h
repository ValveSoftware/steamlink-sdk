// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for android media player.
// Multiply-included message file, hence no include guard.

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/decrypt_config.h"
#include "media/blink/renderer_media_player_interface.h"
#include "media/gpu/ipc/common/media_param_traits.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerMsgStart

IPC_ENUM_TRAITS(media::AudioCodec)
IPC_ENUM_TRAITS(media::DemuxerStream::Status)
IPC_ENUM_TRAITS(media::DemuxerStream::Type)
IPC_ENUM_TRAITS(media::VideoCodec)

IPC_STRUCT_TRAITS_BEGIN(media::DemuxerConfigs)
  IPC_STRUCT_TRAITS_MEMBER(audio_codec)
  IPC_STRUCT_TRAITS_MEMBER(audio_channels)
  IPC_STRUCT_TRAITS_MEMBER(audio_sampling_rate)
  IPC_STRUCT_TRAITS_MEMBER(is_audio_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(audio_extra_data)
  IPC_STRUCT_TRAITS_MEMBER(audio_codec_delay_ns)
  IPC_STRUCT_TRAITS_MEMBER(audio_seek_preroll_ns)

  IPC_STRUCT_TRAITS_MEMBER(video_codec)
  IPC_STRUCT_TRAITS_MEMBER(video_size)
  IPC_STRUCT_TRAITS_MEMBER(is_video_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(video_extra_data)

  IPC_STRUCT_TRAITS_MEMBER(duration)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::DemuxerData)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(access_units)
  IPC_STRUCT_TRAITS_MEMBER(demuxer_configs)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::AccessUnit)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(is_end_of_stream)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(key_id)
  IPC_STRUCT_TRAITS_MEMBER(iv)
  IPC_STRUCT_TRAITS_MEMBER(subsamples)
  IPC_STRUCT_TRAITS_MEMBER(is_key_frame)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(MediaPlayerHostMsg_Initialize_Type,
                          MEDIA_PLAYER_TYPE_LAST)

// Parameters to describe a media player
IPC_STRUCT_BEGIN(MediaPlayerHostMsg_Initialize_Params)
  IPC_STRUCT_MEMBER(MediaPlayerHostMsg_Initialize_Type, type)
  IPC_STRUCT_MEMBER(int, player_id)
  IPC_STRUCT_MEMBER(int, demuxer_client_id)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)
  IPC_STRUCT_MEMBER(GURL, frame_url)
  IPC_STRUCT_MEMBER(bool, allow_credentials)
  IPC_STRUCT_MEMBER(int, delegate_id)
  IPC_STRUCT_MEMBER(int, media_session_id)
IPC_STRUCT_END()

// Chrome for Android seek message sequence is:
// 1. Renderer->Browser MediaPlayerHostMsg_Seek
//    This is the beginning of actual seek flow in response to web app requests
//    for seeks and browser MediaPlayerMsg_SeekRequests. With this message,
//    the renderer asks browser to perform actual seek. At most one of these
//    actual seeks will be in process between this message and renderer's later
//    receipt of MediaPlayerMsg_SeekCompleted from the browser.
// 2. Browser->Renderer MediaPlayerMsg_SeekCompleted
//    Once the browser determines the seek is complete, it sends this message to
//    notify the renderer of seek completion.
//
// Other seek-related IPC messages:
// Browser->Renderer MediaPlayerMsg_SeekRequest
//    Browser requests to begin a seek. All browser-initiated seeks must begin
//    with this request. Renderer controls actual seek initiation via the normal
//    seek flow, above, keeping web apps aware of seeks. These requests are
//    also allowed while another actual seek is in progress.
//
// If the demuxer is located in the renderer, as in media source players, the
// browser must ensure the renderer demuxer is appropriately seeked between
// receipt of MediaPlayerHostMsg_Seek and transmission of
// MediaPlayerMsg_SeekCompleted. The following two renderer-demuxer control
// messages book-end the renderer-demuxer seek:
// 1.1 Browser->Renderer MediaPlayerMsg_DemuxerSeekRequest
// 1.2 Renderer->Browser MediaPlayerHostMsg_DemuxerSeekDone

// Only in short-term hack to seek to reach I-Frame to feed a newly constructed
// video decoder may the above IPC sequence be modified to exclude SeekRequest,
// Seek and SeekCompleted, with condition that DemuxerSeekRequest's
// |is_browser_seek| parameter be true. Regular seek messages must still be
// handled even when a hack browser seek is in progress. In this case, the
// browser seek request's |time_to_seek| may no longer be buffered and the
// demuxer may instead seek to a future buffered time. The resulting
// DemuxerSeekDone message's |actual_browser_seek_time| is the time actually
// seeked-to, and is only meaningful for these hack browser seeks.
// TODO(wolenetz): Instead of doing browser seek, replay cached data since last
// keyframe. See http://crbug.com/304234.

// Messages for notifying the render process of media playback status -------

// Media buffering has updated.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaBufferingUpdate,
                    int /* player_id */,
                    int /* percent */)

// A media playback error has occurred.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaError,
                    int /* player_id */,
                    int /* error */)

// Playback is completed.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_MediaPlaybackCompleted,
                    int /* player_id */)

// Media metadata has changed.
IPC_MESSAGE_ROUTED5(MediaPlayerMsg_MediaMetadataChanged,
                    int /* player_id */,
                    base::TimeDelta /* duration */,
                    int /* width */,
                    int /* height */,
                    bool /* success */)

// Requests renderer player to ask its client (blink HTMLMediaElement) to seek.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_SeekRequest,
                    int /* player_id */,
                    base::TimeDelta /* time_to_seek_to */)

// Media seek is completed.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_SeekCompleted,
                    int /* player_id */,
                    base::TimeDelta /* current_time */)

// Video size has changed.
IPC_MESSAGE_ROUTED3(MediaPlayerMsg_MediaVideoSizeChanged,
                    int /* player_id */,
                    int /* width */,
                    int /* height */)

// The current play time has updated.
IPC_MESSAGE_ROUTED3(MediaPlayerMsg_MediaTimeUpdate,
                    int /* player_id */,
                    base::TimeDelta /* current_timestamp */,
                    base::TimeTicks /* current_time_ticks */)

// A new key is required in order to continue decrypting encrypted content.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_WaitingForDecryptionKey, int /* player_id */)

// The player has been released.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_MediaPlayerReleased,
                    int /* player_id */)

// The player exited fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidExitFullscreen,
                    int /* player_id */)

// The player started playing.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidMediaPlayerPlay,
                    int /* player_id */)

// The player was paused.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidMediaPlayerPause,
                    int /* player_id */)

// Requests renderer demuxer seek.
IPC_MESSAGE_CONTROL3(MediaPlayerMsg_DemuxerSeekRequest,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* time_to_seek */,
                     bool /* is_browser_seek */)

// The media source player reads data from demuxer
IPC_MESSAGE_CONTROL2(MediaPlayerMsg_ReadFromDemuxer,
                     int /* demuxer_client_id */,
                     media::DemuxerStream::Type /* type */)

// Clank has connected to the remote device.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_ConnectedToRemoteDevice,
                    int /* player_id */,
                    std::string /* remote_playback_message */)

// Clank has disconnected from the remote device.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DisconnectedFromRemoteDevice,
                    int /* player_id */)

// The remote playback device selection has been cancelled.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_CancelledRemotePlaybackRequest,
                    int /* player_id */)

// The availability of remote devices has changed
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_RemoteRouteAvailabilityChanged,
                    int /* player_id */,
                    bool /* routes_available */)

// Messages for controlling the media playback in browser process ----------

// Destroy the media player object.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_DestroyMediaPlayer,
                    int /* player_id */)

// Initialize a media player object.
IPC_MESSAGE_ROUTED1(
    MediaPlayerHostMsg_Initialize,
    MediaPlayerHostMsg_Initialize_Params)

// Pause the player.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_Pause,
                    int /* player_id */,
                    bool /* is_media_related_action */)

// Release player resources after it was suspended.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_SuspendAndRelease, int /* player_id */)

// Perform a seek.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_Seek,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Start the player for playback.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_Start, int /* player_id */)

// Set the volume.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_SetVolume,
                    int /* player_id */,
                    double /* volume */)

// Set the poster image.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_SetPoster,
                    int /* player_id */,
                    GURL /* poster url */)

// Requests the player to enter fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_EnterFullscreen, int /* player_id */)

// Play the media on a remote device, if possible.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_RequestRemotePlayback,
                    int /* demuxer_client_id */)

// Control media playing on a remote device.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_RequestRemotePlaybackControl,
                    int /* demuxer_client_id */)

// Requests the player with |player_id| to use the CDM with |cdm_id|.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_SetCdm,
                    int /* player_id */,
                    int /* cdm_id */)

// Sent after the renderer demuxer has seeked.
IPC_MESSAGE_CONTROL2(MediaPlayerHostMsg_DemuxerSeekDone,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* actual_browser_seek_time */)

// Inform the media source player that the demuxer is ready.
IPC_MESSAGE_CONTROL2(MediaPlayerHostMsg_DemuxerReady,
                     int /* demuxer_client_id */,
                     media::DemuxerConfigs)

// Sent when the data was read from the ChunkDemuxer.
IPC_MESSAGE_CONTROL2(MediaPlayerHostMsg_ReadFromDemuxerAck,
                     int /* demuxer_client_id */,
                     media::DemuxerData)

// Inform the media source player of changed media duration from demuxer.
IPC_MESSAGE_CONTROL2(MediaPlayerHostMsg_DurationChanged,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* duration */)

#if defined(VIDEO_HOLE)
// Notify the player about the external surface, requesting it if necessary.
// |is_request| true if the player is requesting the external surface.
// |rect| the boundary rectangle of the video element.
IPC_MESSAGE_ROUTED3(MediaPlayerHostMsg_NotifyExternalSurface,
                    int /* player_id */,
                    bool /* is_request */,
                    gfx::RectF /* rect */)
#endif  // defined(VIDEO_HOLE)
