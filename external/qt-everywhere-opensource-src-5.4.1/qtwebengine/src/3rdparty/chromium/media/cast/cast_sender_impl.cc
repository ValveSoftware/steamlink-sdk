// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/cast/cast_sender_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"

namespace media {
namespace cast {

// The LocalVideoFrameInput class posts all incoming video frames to the main
// cast thread for processing.
class LocalVideoFrameInput : public VideoFrameInput {
 public:
  LocalVideoFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<VideoSender> video_sender)
      : cast_environment_(cast_environment), video_sender_(video_sender) {}

  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoSender::InsertRawVideoFrame,
                                           video_sender_,
                                           video_frame,
                                           capture_time));
  }

 protected:
  virtual ~LocalVideoFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalVideoFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<VideoSender> video_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoFrameInput);
};

// The LocalAudioFrameInput class posts all incoming audio frames to the main
// cast thread for processing. Therefore frames can be inserted from any thread.
class LocalAudioFrameInput : public AudioFrameInput {
 public:
  LocalAudioFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<AudioSender> audio_sender)
      : cast_environment_(cast_environment), audio_sender_(audio_sender) {}

  virtual void InsertAudio(scoped_ptr<AudioBus> audio_bus,
                           const base::TimeTicks& recorded_time) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::InsertAudio,
                                           audio_sender_,
                                           base::Passed(&audio_bus),
                                           recorded_time));
  }

 protected:
  virtual ~LocalAudioFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalAudioFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalAudioFrameInput);
};

scoped_ptr<CastSender> CastSender::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    transport::CastTransportSender* const transport_sender) {
  CHECK(cast_environment);
  return scoped_ptr<CastSender>(
      new CastSenderImpl(cast_environment, transport_sender));
}

CastSenderImpl::CastSenderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    transport::CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      weak_factory_(this) {
  CHECK(cast_environment);
}

void CastSenderImpl::InitializeAudio(
    const AudioSenderConfig& audio_config,
    const CastInitializationCallback& cast_initialization_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(audio_config.use_external_encoder ||
        cast_environment_->HasAudioThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeAudio()";

  audio_sender_.reset(
      new AudioSender(cast_environment_, audio_config, transport_sender_));

  const CastInitializationStatus status = audio_sender_->InitializationResult();
  if (status == STATUS_AUDIO_INITIALIZED) {
    ssrc_of_audio_sender_ = audio_config.incoming_feedback_ssrc;
    audio_frame_input_ =
        new LocalAudioFrameInput(cast_environment_, audio_sender_->AsWeakPtr());
  }
  cast_initialization_cb.Run(status);
}

void CastSenderImpl::InitializeVideo(
    const VideoSenderConfig& video_config,
    const CastInitializationCallback& cast_initialization_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(video_config.use_external_encoder ||
        cast_environment_->HasVideoThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeVideo()";

  video_sender_.reset(new VideoSender(cast_environment_,
                                      video_config,
                                      create_vea_cb,
                                      create_video_encode_mem_cb,
                                      transport_sender_));

  const CastInitializationStatus status = video_sender_->InitializationResult();
  if (status == STATUS_VIDEO_INITIALIZED) {
    ssrc_of_video_sender_ = video_config.incoming_feedback_ssrc;
    video_frame_input_ =
        new LocalVideoFrameInput(cast_environment_, video_sender_->AsWeakPtr());
  }
  cast_initialization_cb.Run(status);
}

CastSenderImpl::~CastSenderImpl() {
  VLOG(1) << "CastSenderImpl@" << this << "::~CastSenderImpl()";
}

// ReceivedPacket handle the incoming packets to the cast sender
// it's only expected to receive RTCP feedback packets from the remote cast
// receiver. The class verifies that that it is a RTCP packet and based on the
// SSRC of the incoming packet route the packet to the correct sender; audio or
// video.
//
// Definition of SSRC as defined in RFC 3550.
// Synchronization source (SSRC): The source of a stream of RTP
//    packets, identified by a 32-bit numeric SSRC identifier carried in
//    the RTP header so as not to be dependent upon the network address.
//    All packets from a synchronization source form part of the same
//    timing and sequence number space, so a receiver groups packets by
//    synchronization source for playback.  Examples of synchronization
//    sources include the sender of a stream of packets derived from a
//    signal source such as a microphone or a camera, or an RTP mixer
//    (see below).  A synchronization source may change its data format,
//    e.g., audio encoding, over time.  The SSRC identifier is a
//    randomly chosen value meant to be globally unique within a
//    particular RTP session (see Section 8).  A participant need not
//    use the same SSRC identifier for all the RTP sessions in a
//    multimedia session; the binding of the SSRC identifiers is
//    provided through RTCP (see Section 6.5.1).  If a participant
//    generates multiple streams in one RTP session, for example from
//    separate video cameras, each MUST be identified as a different
//    SSRC.
void CastSenderImpl::ReceivedPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_);
  size_t length = packet->size();
  const uint8_t* data = &packet->front();
  if (!Rtcp::IsRtcpPacket(data, length)) {
    VLOG(1) << "CastSenderImpl@" << this << "::ReceivedPacket() -- "
            << "Received an invalid (non-RTCP?) packet in the cast sender.";
    return;
  }
  uint32 ssrc_of_sender = Rtcp::GetSsrcOfSender(data, length);
  if (ssrc_of_sender == ssrc_of_audio_sender_) {
    if (!audio_sender_) {
      NOTREACHED();
      return;
    }
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::IncomingRtcpPacket,
                                           audio_sender_->AsWeakPtr(),
                                           base::Passed(&packet)));
  } else if (ssrc_of_sender == ssrc_of_video_sender_) {
    if (!video_sender_) {
      NOTREACHED();
      return;
    }
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoSender::IncomingRtcpPacket,
                                           video_sender_->AsWeakPtr(),
                                           base::Passed(&packet)));
  } else {
    VLOG(1) << "CastSenderImpl@" << this << "::ReceivedPacket() -- "
            << "Received a RTCP packet with a non matching sender SSRC "
            << ssrc_of_sender;
  }
}

scoped_refptr<AudioFrameInput> CastSenderImpl::audio_frame_input() {
  return audio_frame_input_;
}

scoped_refptr<VideoFrameInput> CastSenderImpl::video_frame_input() {
  return video_frame_input_;
}

transport::PacketReceiverCallback CastSenderImpl::packet_receiver() {
  return base::Bind(&CastSenderImpl::ReceivedPacket,
                    weak_factory_.GetWeakPtr());
}

}  // namespace cast
}  // namespace media
