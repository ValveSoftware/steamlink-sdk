// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RECEIVER_CAST_RECEIVER_IMPL_H_
#define MEDIA_CAST_RECEIVER_CAST_RECEIVER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/receiver/frame_receiver.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {

class AudioDecoder;
class VideoDecoder;

// This is a pure owner class that groups all required receiver-related objects
// together, such as the paced packet sender, audio/video RTP frame receivers,
// and software decoders (created on-demand).
class CastReceiverImpl : public CastReceiver {
 public:
  CastReceiverImpl(scoped_refptr<CastEnvironment> cast_environment,
                   const FrameReceiverConfig& audio_config,
                   const FrameReceiverConfig& video_config,
                   transport::PacketSender* const packet_sender);

  virtual ~CastReceiverImpl();

  // CastReceiver implementation.
  virtual transport::PacketReceiverCallback packet_receiver() OVERRIDE;
  virtual void RequestDecodedAudioFrame(
      const AudioFrameDecodedCallback& callback) OVERRIDE;
  virtual void RequestEncodedAudioFrame(
      const ReceiveEncodedFrameCallback& callback) OVERRIDE;
  virtual void RequestDecodedVideoFrame(
      const VideoFrameDecodedCallback& callback) OVERRIDE;
  virtual void RequestEncodedVideoFrame(
      const ReceiveEncodedFrameCallback& callback) OVERRIDE;

 private:
  // Forwards |packet| to a specific RTP frame receiver, or drops it if SSRC
  // does not map to one of the receivers.
  void DispatchReceivedPacket(scoped_ptr<Packet> packet);

  // Feeds an EncodedFrame into |audio_decoder_|.  RequestDecodedAudioFrame()
  // uses this as a callback for RequestEncodedAudioFrame().
  void DecodeEncodedAudioFrame(
      const AudioFrameDecodedCallback& callback,
      scoped_ptr<transport::EncodedFrame> encoded_frame);

  // Feeds an EncodedFrame into |video_decoder_|.  RequestDecodedVideoFrame()
  // uses this as a callback for RequestEncodedVideoFrame().
  void DecodeEncodedVideoFrame(
      const VideoFrameDecodedCallback& callback,
      scoped_ptr<transport::EncodedFrame> encoded_frame);

  // Receives an AudioBus from |audio_decoder_|, logs the event, and passes the
  // data on by running the given |callback|.  This method is static to ensure
  // it can be called after a CastReceiverImpl instance is destroyed.
  // DecodeEncodedAudioFrame() uses this as a callback for
  // AudioDecoder::DecodeFrame().
  static void EmitDecodedAudioFrame(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const AudioFrameDecodedCallback& callback,
      uint32 frame_id,
      uint32 rtp_timestamp,
      const base::TimeTicks& playout_time,
      scoped_ptr<AudioBus> audio_bus,
      bool is_continuous);

  // Receives a VideoFrame from |video_decoder_|, logs the event, and passes the
  // data on by running the given |callback|.  This method is static to ensure
  // it can be called after a CastReceiverImpl instance is destroyed.
  // DecodeEncodedVideoFrame() uses this as a callback for
  // VideoDecoder::DecodeFrame().
  static void EmitDecodedVideoFrame(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const VideoFrameDecodedCallback& callback,
      uint32 frame_id,
      uint32 rtp_timestamp,
      const base::TimeTicks& playout_time,
      const scoped_refptr<VideoFrame>& video_frame,
      bool is_continuous);

  const scoped_refptr<CastEnvironment> cast_environment_;
  transport::PacedSender pacer_;
  FrameReceiver audio_receiver_;
  FrameReceiver video_receiver_;

  // Used by DispatchReceivedPacket() to direct packets to the appropriate frame
  // receiver.
  const uint32 ssrc_of_audio_sender_;
  const uint32 ssrc_of_video_sender_;

  // Parameters for the decoders that are created on-demand.  The values here
  // might be nonsense if the client of CastReceiverImpl never intends to use
  // the internal software-based decoders.
  const int num_audio_channels_;
  const int audio_sampling_rate_;
  const transport::AudioCodec audio_codec_;
  const transport::VideoCodec video_codec_;

  // Created on-demand to decode frames from |audio_receiver_| into AudioBuses
  // for playback.
  scoped_ptr<AudioDecoder> audio_decoder_;

  // Created on-demand to decode frames from |video_receiver_| into VideoFrame
  // images for playback.
  scoped_ptr<VideoDecoder> video_decoder_;

  DISALLOW_COPY_AND_ASSIGN(CastReceiverImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RECEIVER_CAST_RECEIVER_IMPL_
