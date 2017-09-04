// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <stddef.h>

#include <cmath>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

// AAC(M4A) decoding specific constants.
static const int kAACPrimingFrameCount = 2112;
static const int kAACRemainderFrameCount = 519;

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : stream_index_(0),
      protocol_(protocol),
      audio_codec_(kUnknownAudioCodec),
      channels_(0),
      sample_rate_(0),
      av_sample_format_(0) {}

AudioFileReader::~AudioFileReader() {
  Close();
}

bool AudioFileReader::Open() {
  if (!OpenDemuxer())
    return false;
  if (!OpenDecoder())
    return false;

  // If the duration is unknown, fail out; this API can not work with streams of
  // unknown duration currently.
  return glue_->format_context()->duration != AV_NOPTS_VALUE;
}

bool AudioFileReader::OpenDemuxer() {
  glue_.reset(new FFmpegGlue(protocol_));
  AVFormatContext* format_context = glue_->format_context();

  // Open FFmpeg AVFormatContext.
  if (!glue_->OpenContext()) {
    DLOG(WARNING) << "AudioFileReader::Open() : error in avformat_open_input()";
    return false;
  }

  // Find the first audio stream, if any.
  codec_context_.reset();
  bool found_stream = false;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    if (format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_AUDIO) {
      stream_index_ = i;
      found_stream = true;
      break;
    }
  }

  if (!found_stream)
    return false;

  const int result = avformat_find_stream_info(format_context, NULL);
  if (result < 0) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in avformat_find_stream_info()";
    return false;
  }

  // Get the codec context.
  codec_context_ =
      AVStreamToAVCodecContext(format_context->streams[stream_index_]);
  if (!codec_context_)
    return false;

  DCHECK_EQ(codec_context_->codec_type, AVMEDIA_TYPE_AUDIO);
  return true;
}

bool AudioFileReader::OpenDecoder() {
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (codec) {
    // MP3 decodes to S16P which we don't support, tell it to use S16 instead.
    if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P)
      codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;

    const int result = avcodec_open2(codec_context_.get(), codec, nullptr);
    if (result < 0) {
      DLOG(WARNING) << "AudioFileReader::Open() : could not open codec -"
                    << " result: " << result;
      return false;
    }

    // Ensure avcodec_open2() respected our format request.
    if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
      DLOG(ERROR) << "AudioFileReader::Open() : unable to configure a"
                  << " supported sample format - "
                  << codec_context_->sample_fmt;
      return false;
    }
  } else {
    DLOG(WARNING) << "AudioFileReader::Open() : could not find codec.";
    return false;
  }

  // Verify the channel layout is supported by Chrome.  Acts as a sanity check
  // against invalid files.  See http://crbug.com/171962
  if (ChannelLayoutToChromeChannelLayout(
          codec_context_->channel_layout, codec_context_->channels) ==
      CHANNEL_LAYOUT_UNSUPPORTED) {
    return false;
  }

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->channels;
  audio_codec_ = CodecIDToAudioCodec(codec_context_->codec_id);
  sample_rate_ = codec_context_->sample_rate;
  av_sample_format_ = codec_context_->sample_fmt;
  return true;
}

void AudioFileReader::Close() {
  codec_context_.reset();
  glue_.reset();
}

int AudioFileReader::Read(AudioBus* audio_bus) {
  DCHECK(glue_.get() && codec_context_) <<
      "AudioFileReader::Read() : reader is not opened!";

  DCHECK_EQ(audio_bus->channels(), channels());
  if (audio_bus->channels() != channels())
    return 0;

  size_t bytes_per_sample = av_get_bytes_per_sample(codec_context_->sample_fmt);

  // Holds decoded audio.
  std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame> av_frame(av_frame_alloc());

  // Read until we hit EOF or we've read the requested number of frames.
  AVPacket packet;
  int current_frame = 0;
  bool continue_decoding = true;

  while (current_frame < audio_bus->frames() && continue_decoding &&
         ReadPacket(&packet)) {
    // Make a shallow copy of packet so we can slide packet.data as frames are
    // decoded from the packet; otherwise av_packet_unref() will corrupt memory.
    AVPacket packet_temp = packet;
    do {
      // Reset frame to default values.
      av_frame_unref(av_frame.get());

      int frame_decoded = 0;
      int result = avcodec_decode_audio4(codec_context_.get(), av_frame.get(),
                                         &frame_decoded, &packet_temp);

      if (result < 0) {
        DLOG(WARNING)
            << "AudioFileReader::Read() : error in avcodec_decode_audio4() -"
            << result;
        break;
      }

      // Update packet size and data pointer in case we need to call the decoder
      // with the remaining bytes from this packet.
      packet_temp.size -= result;
      packet_temp.data += result;

      if (!frame_decoded)
        continue;

      // Determine the number of sample-frames we just decoded.  Check overflow.
      int frames_read = av_frame->nb_samples;
      if (frames_read < 0) {
        continue_decoding = false;
        break;
      }

#ifdef CHROMIUM_NO_AVFRAME_CHANNELS
      int channels = av_get_channel_layout_nb_channels(
          av_frame->channel_layout);
#else
      int channels = av_frame->channels;
#endif
      if (av_frame->sample_rate != sample_rate_ ||
          channels != channels_ ||
          av_frame->format != av_sample_format_) {
        DLOG(ERROR) << "Unsupported midstream configuration change!"
                    << " Sample Rate: " << av_frame->sample_rate << " vs "
                    << sample_rate_
                    << ", Channels: " << channels << " vs "
                    << channels_
                    << ", Sample Format: " << av_frame->format << " vs "
                    << av_sample_format_;

        // This is an unrecoverable error, so bail out.
        continue_decoding = false;
        break;
      }

      // Truncate, if necessary, if the destination isn't big enough.
      if (current_frame + frames_read > audio_bus->frames()) {
        DLOG(ERROR) << "Truncating decoded data due to output size.";
        frames_read = audio_bus->frames() - current_frame;
      }

      // Deinterleave each channel and convert to 32bit floating-point with
      // nominal range -1.0 -> +1.0.  If the output is already in float planar
      // format, just copy it into the AudioBus.
      if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT) {
        float* decoded_audio_data = reinterpret_cast<float*>(av_frame->data[0]);
        int channels = audio_bus->channels();
        for (int ch = 0; ch < channels; ++ch) {
          float* bus_data = audio_bus->channel(ch) + current_frame;
          for (int i = 0, offset = ch; i < frames_read;
               ++i, offset += channels) {
            bus_data[i] = decoded_audio_data[offset];
          }
        }
      } else if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
        for (int ch = 0; ch < audio_bus->channels(); ++ch) {
          memcpy(audio_bus->channel(ch) + current_frame,
                 av_frame->extended_data[ch], sizeof(float) * frames_read);
        }
      } else {
        audio_bus->FromInterleavedPartial(
            av_frame->data[0], current_frame, frames_read, bytes_per_sample);
      }

      current_frame += frames_read;
    } while (packet_temp.size > 0);
    av_packet_unref(&packet);
  }

  // Zero any remaining frames.
  audio_bus->ZeroFramesPartial(
      current_frame, audio_bus->frames() - current_frame);

  // Returns the actual number of sample-frames decoded.
  // Ideally this represents the "true" exact length of the file.
  return current_frame;
}

base::TimeDelta AudioFileReader::GetDuration() const {
  const AVRational av_time_base = {1, AV_TIME_BASE};

  DCHECK_NE(glue_->format_context()->duration, AV_NOPTS_VALUE);
  base::CheckedNumeric<int64_t> estimated_duration_us =
      glue_->format_context()->duration;

  if (audio_codec_ == kCodecAAC) {
    // For certain AAC-encoded files, FFMPEG's estimated frame count might not
    // be sufficient to capture the entire audio content that we want. This is
    // especially noticeable for short files (< 10ms) resulting in silence
    // throughout the decoded buffer. Thus we add the priming frames and the
    // remainder frames to the estimation.
    // (See: crbug.com/513178)
    estimated_duration_us +=
        ceil(1000000.0 * static_cast<double>(kAACPrimingFrameCount +
                                             kAACRemainderFrameCount) /
             sample_rate());
  } else {
    // Add one microsecond to avoid rounding-down errors which can occur when
    // |duration| has been calculated from an exact number of sample-frames.
    // One microsecond is much less than the time of a single sample-frame
    // at any real-world sample-rate.
    estimated_duration_us += 1;
  }

  return ConvertFromTimeBase(av_time_base, estimated_duration_us.ValueOrDie());
}

int AudioFileReader::GetNumberOfFrames() const {
  return static_cast<int>(ceil(GetDuration().InSecondsF() * sample_rate()));
}

bool AudioFileReader::OpenDemuxerForTesting() {
  return OpenDemuxer();
}

bool AudioFileReader::ReadPacketForTesting(AVPacket* output_packet) {
  return ReadPacket(output_packet);
}

bool AudioFileReader::ReadPacket(AVPacket* output_packet) {
  while (av_read_frame(glue_->format_context(), output_packet) >= 0) {
    // Skip packets from other streams.
    if (output_packet->stream_index != stream_index_) {
      av_packet_unref(output_packet);
      continue;
    }
    return true;
  }
  return false;
}

bool AudioFileReader::SeekForTesting(base::TimeDelta seek_time) {
  // Use the AVStream's time_base, since |codec_context_| does not have
  // time_base populated until after OpenDecoder().
  return av_seek_frame(
             glue_->format_context(), stream_index_,
             ConvertToTimeBase(GetAVStreamForTesting()->time_base, seek_time),
             AVSEEK_FLAG_BACKWARD) >= 0;
}

const AVStream* AudioFileReader::GetAVStreamForTesting() const {
  return glue_->format_context()->streams[stream_index_];
}

}  // namespace media
