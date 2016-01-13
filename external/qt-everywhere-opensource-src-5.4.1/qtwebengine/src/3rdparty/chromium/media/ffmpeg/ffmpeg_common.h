// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

// Include FFmpeg header files.
extern "C" {
// Disable deprecated features which result in spammy compile warnings.  This
// list of defines must mirror those in the 'defines' section of the ffmpeg.gyp
// file or the headers below will generate different structures.
#define FF_API_PIX_FMT_DESC 0
#define FF_API_OLD_DECODE_AUDIO 0
#define FF_API_DESTRUCT_PACKET 0
#define FF_API_GET_BUFFER 0

// Temporarily disable possible loss of data warning.
// TODO(scherkus): fix and upstream the compiler warnings.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audioconvert.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/log.h>
#include <libavutil/imgutils.h>
MSVC_POP_WARNING();
}  // extern "C"

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

// The following implement the deleters declared in ffmpeg_deleters.h (which
// contains the declarations needed for use with |scoped_ptr| without #include
// "pollution").

inline void ScopedPtrAVFree::operator()(void* x) const {
  av_free(x);
}

inline void ScopedPtrAVFreePacket::operator()(void* x) const {
  AVPacket* packet = static_cast<AVPacket*>(x);
  av_free_packet(packet);
  delete packet;
}

inline void ScopedPtrAVFreeContext::operator()(void* x) const {
  AVCodecContext* codec_context = static_cast<AVCodecContext*>(x);
  av_free(codec_context->extradata);
  avcodec_close(codec_context);
  av_free(codec_context);
}

inline void ScopedPtrAVFreeFrame::operator()(void* x) const {
  AVFrame* frame = static_cast<AVFrame*>(x);
  av_frame_free(&frame);
}

// Converts an int64 timestamp in |time_base| units to a base::TimeDelta.
// For example if |timestamp| equals 11025 and |time_base| equals {1, 44100}
// then the return value will be a base::TimeDelta for 0.25 seconds since that
// is how much time 11025/44100ths of a second represents.
MEDIA_EXPORT base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                                 int64 timestamp);

// Converts a base::TimeDelta into an int64 timestamp in |time_base| units.
// For example if |timestamp| is 0.5 seconds and |time_base| is {1, 44100}, then
// the return value will be 22050 since that is how many 1/44100ths of a second
// represent 0.5 seconds.
MEDIA_EXPORT int64 ConvertToTimeBase(const AVRational& time_base,
                                     const base::TimeDelta& timestamp);

void AVStreamToAudioDecoderConfig(
    const AVStream* stream,
    AudioDecoderConfig* config,
    bool record_stats);
void AudioDecoderConfigToAVCodecContext(
    const AudioDecoderConfig& config,
    AVCodecContext* codec_context);

void AVStreamToVideoDecoderConfig(
    const AVStream* stream,
    VideoDecoderConfig* config,
    bool record_stats);
void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context);

MEDIA_EXPORT void AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    bool is_encrypted,
    AudioDecoderConfig* config,
    bool record_stats);

// Converts FFmpeg's channel layout to chrome's ChannelLayout.  |channels| can
// be used when FFmpeg's channel layout is not informative in order to make a
// good guess about the plausible channel layout based on number of channels.
MEDIA_EXPORT ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                              int channels);

// Converts FFmpeg's audio sample format to Chrome's SampleFormat.
MEDIA_EXPORT SampleFormat
    AVSampleFormatToSampleFormat(AVSampleFormat sample_format);

// Converts FFmpeg's pixel formats to its corresponding supported video format.
MEDIA_EXPORT VideoFrame::Format PixelFormatToVideoFormat(
    PixelFormat pixel_format);

// Converts video formats to its corresponding FFmpeg's pixel formats.
PixelFormat VideoFormatToPixelFormat(VideoFrame::Format video_format);

// Convert FFmpeg UTC representation (YYYY-MM-DD HH:MM:SS) to base::Time.
// Returns true and sets |*out| if |date_utc| contains a valid
// date string. Otherwise returns fals and timeline_offset is unmodified.
MEDIA_EXPORT bool FFmpegUTCDateToTime(const char* date_utc, base::Time* out);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
