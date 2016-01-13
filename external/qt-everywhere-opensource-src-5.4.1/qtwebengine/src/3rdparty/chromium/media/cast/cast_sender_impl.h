// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAST_CAST_SENDER_IMPL_H_
#define MEDIA_CAST_CAST_SENDER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/video_sender/video_sender.h"

namespace media {
class VideoFrame;

namespace cast {
class AudioSender;
class VideoSender;

// This class combines all required sending objects such as the audio and video
// senders, pacer, packet receiver and frame input.
class CastSenderImpl : public CastSender {
 public:
  CastSenderImpl(scoped_refptr<CastEnvironment> cast_environment,
                 transport::CastTransportSender* const transport_sender);

  virtual void InitializeAudio(
      const AudioSenderConfig& audio_config,
      const CastInitializationCallback& cast_initialization_cb) OVERRIDE;
  virtual void InitializeVideo(
      const VideoSenderConfig& video_config,
      const CastInitializationCallback& cast_initialization_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb)
      OVERRIDE;

  virtual ~CastSenderImpl();

  virtual scoped_refptr<AudioFrameInput> audio_frame_input() OVERRIDE;
  virtual scoped_refptr<VideoFrameInput> video_frame_input() OVERRIDE;

  virtual transport::PacketReceiverCallback packet_receiver() OVERRIDE;

 private:
  void ReceivedPacket(scoped_ptr<Packet> packet);

  CastInitializationCallback initialization_callback_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_ptr<VideoSender> video_sender_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;
  scoped_refptr<CastEnvironment> cast_environment_;
  // The transport sender is owned by the owner of the CastSender, and should be
  // valid throughout the lifetime of the CastSender.
  transport::CastTransportSender* const transport_sender_;
  uint32 ssrc_of_audio_sender_;
  uint32 ssrc_of_video_sender_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CastSenderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastSenderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_IMPL_H_
