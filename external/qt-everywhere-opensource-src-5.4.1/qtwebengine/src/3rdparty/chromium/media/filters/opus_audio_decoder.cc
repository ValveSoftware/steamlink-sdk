// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/opus_audio_decoder.h"

#include <cmath>

#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "third_party/opus/src/include/opus.h"
#include "third_party/opus/src/include/opus_multistream.h"

namespace media {

static uint16 ReadLE16(const uint8* data, size_t data_size, int read_offset) {
  uint16 value = 0;
  DCHECK_LE(read_offset + sizeof(value), data_size);
  memcpy(&value, data + read_offset, sizeof(value));
  return base::ByteSwapToLE16(value);
}

// The Opus specification is part of IETF RFC 6716:
// http://tools.ietf.org/html/rfc6716

// Opus uses Vorbis channel mapping, and Vorbis channel mapping specifies
// mappings for up to 8 channels. This information is part of the Vorbis I
// Specification:
// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html
static const int kMaxVorbisChannels = 8;

// Maximum packet size used in Xiph's opusdec and FFmpeg's libopusdec.
static const int kMaxOpusOutputPacketSizeSamples = 960 * 6;

static void RemapOpusChannelLayout(const uint8* opus_mapping,
                                   int num_channels,
                                   uint8* channel_layout) {
  DCHECK_LE(num_channels, kMaxVorbisChannels);

  // Opus uses Vorbis channel layout.
  const int32 num_layouts = kMaxVorbisChannels;
  const int32 num_layout_values = kMaxVorbisChannels;

  // Vorbis channel ordering for streams with >= 2 channels:
  // 2 Channels
  //   L, R
  // 3 Channels
  //   L, Center, R
  // 4 Channels
  //   Front L, Front R, Back L, Back R
  // 5 Channels
  //   Front L, Center, Front R, Back L, Back R
  // 6 Channels (5.1)
  //   Front L, Center, Front R, Back L, Back R, LFE
  // 7 channels (6.1)
  //   Front L, Front Center, Front R, Side L, Side R, Back Center, LFE
  // 8 Channels (7.1)
  //   Front L, Center, Front R, Side L, Side R, Back L, Back R, LFE
  //
  // Channel ordering information is taken from section 4.3.9 of the Vorbis I
  // Specification:
  // http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9

  // These are the FFmpeg channel layouts expressed using the position of each
  // channel in the output stream from libopus.
  const uint8 kFFmpegChannelLayouts[num_layouts][num_layout_values] = {
    { 0 },

    // Stereo: No reorder.
    { 0, 1 },

    // 3 Channels, from Vorbis order to:
    //  L, R, Center
    { 0, 2, 1 },

    // 4 Channels: No reorder.
    { 0, 1, 2, 3 },

    // 5 Channels, from Vorbis order to:
    //  Front L, Front R, Center, Back L, Back R
    { 0, 2, 1, 3, 4 },

    // 6 Channels (5.1), from Vorbis order to:
    //  Front L, Front R, Center, LFE, Back L, Back R
    { 0, 2, 1, 5, 3, 4 },

    // 7 Channels (6.1), from Vorbis order to:
    //  Front L, Front R, Front Center, LFE, Side L, Side R, Back Center
    { 0, 2, 1, 6, 3, 4, 5 },

    // 8 Channels (7.1), from Vorbis order to:
    //  Front L, Front R, Center, LFE, Back L, Back R, Side L, Side R
    { 0, 2, 1, 7, 5, 6, 3, 4 },
  };

  // Reorder the channels to produce the same ordering as FFmpeg, which is
  // what the pipeline expects.
  const uint8* vorbis_layout_offset = kFFmpegChannelLayouts[num_channels - 1];
  for (int channel = 0; channel < num_channels; ++channel)
    channel_layout[channel] = opus_mapping[vorbis_layout_offset[channel]];
}

// Opus Extra Data contents:
// - "OpusHead" (64 bits)
// - version number (8 bits)
// - Channels C (8 bits)
// - Pre-skip (16 bits)
// - Sampling rate (32 bits)
// - Gain in dB (16 bits, S7.8)
// - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
//            2..254: reserved, 255: multistream with no mapping)
//
// - if (mapping != 0)
//    - N = totel number of streams (8 bits)
//    - M = number of paired streams (8 bits)
//    - C times channel origin
//         - if (C<2*M)
//            - stream = byte/2
//            - if (byte&0x1 == 0)
//                - left
//              else
//                - right
//         - else
//            - stream = byte-M

// Default audio output channel layout. Used to initialize |stream_map| in
// OpusExtraData, and passed to opus_multistream_decoder_create() when the
// extra data does not contain mapping information. The values are valid only
// for mono and stereo output: Opus streams with more than 2 channels require a
// stream map.
static const int kMaxChannelsWithDefaultLayout = 2;
static const uint8 kDefaultOpusChannelLayout[kMaxChannelsWithDefaultLayout] = {
    0, 1 };

// Size of the Opus extra data excluding optional mapping information.
static const int kOpusExtraDataSize = 19;

// Offset to the channel count byte in the Opus extra data.
static const int kOpusExtraDataChannelsOffset = 9;

// Offset to the pre-skip value in the Opus extra data.
static const int kOpusExtraDataSkipSamplesOffset = 10;

// Offset to the gain value in the Opus extra data.
static const int kOpusExtraDataGainOffset = 16;

// Offset to the channel mapping byte in the Opus extra data.
static const int kOpusExtraDataChannelMappingOffset = 18;

// Extra Data contains a stream map. The mapping values are in extra data beyond
// the always present |kOpusExtraDataSize| bytes of data. The mapping data
// contains stream count, coupling information, and per channel mapping values:
//   - Byte 0: Number of streams.
//   - Byte 1: Number coupled.
//   - Byte 2: Starting at byte 2 are |extra_data->channels| uint8 mapping
//             values.
static const int kOpusExtraDataNumStreamsOffset = kOpusExtraDataSize;
static const int kOpusExtraDataNumCoupledOffset =
    kOpusExtraDataNumStreamsOffset + 1;
static const int kOpusExtraDataStreamMapOffset =
    kOpusExtraDataNumStreamsOffset + 2;

struct OpusExtraData {
  OpusExtraData()
      : channels(0),
        skip_samples(0),
        channel_mapping(0),
        num_streams(0),
        num_coupled(0),
        gain_db(0),
        stream_map() {
    memcpy(stream_map,
           kDefaultOpusChannelLayout,
           kMaxChannelsWithDefaultLayout);
  }
  int channels;
  uint16 skip_samples;
  int channel_mapping;
  int num_streams;
  int num_coupled;
  int16 gain_db;
  uint8 stream_map[kMaxVorbisChannels];
};

// Returns true when able to successfully parse and store Opus extra data in
// |extra_data|. Based on opus header parsing code in libopusdec from FFmpeg,
// and opus_header from Xiph's opus-tools project.
static bool ParseOpusExtraData(const uint8* data, int data_size,
                               const AudioDecoderConfig& config,
                               OpusExtraData* extra_data) {
  if (data_size < kOpusExtraDataSize) {
    DLOG(ERROR) << "Extra data size is too small:" << data_size;
    return false;
  }

  extra_data->channels = *(data + kOpusExtraDataChannelsOffset);

  if (extra_data->channels <= 0 || extra_data->channels > kMaxVorbisChannels) {
    DLOG(ERROR) << "invalid channel count in extra data: "
                << extra_data->channels;
    return false;
  }

  extra_data->skip_samples =
      ReadLE16(data, data_size, kOpusExtraDataSkipSamplesOffset);
  extra_data->gain_db = static_cast<int16>(
      ReadLE16(data, data_size, kOpusExtraDataGainOffset));

  extra_data->channel_mapping = *(data + kOpusExtraDataChannelMappingOffset);

  if (!extra_data->channel_mapping) {
    if (extra_data->channels > kMaxChannelsWithDefaultLayout) {
      DLOG(ERROR) << "Invalid extra data, missing stream map.";
      return false;
    }

    extra_data->num_streams = 1;
    extra_data->num_coupled =
        (ChannelLayoutToChannelCount(config.channel_layout()) > 1) ? 1 : 0;
    return true;
  }

  if (data_size < kOpusExtraDataStreamMapOffset + extra_data->channels) {
    DLOG(ERROR) << "Invalid stream map; insufficient data for current channel "
                << "count: " << extra_data->channels;
    return false;
  }

  extra_data->num_streams = *(data + kOpusExtraDataNumStreamsOffset);
  extra_data->num_coupled = *(data + kOpusExtraDataNumCoupledOffset);

  if (extra_data->num_streams + extra_data->num_coupled != extra_data->channels)
    DVLOG(1) << "Inconsistent channel mapping.";

  for (int i = 0; i < extra_data->channels; ++i)
    extra_data->stream_map[i] = *(data + kOpusExtraDataStreamMapOffset + i);
  return true;
}

OpusAudioDecoder::OpusAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      opus_decoder_(NULL),
      start_input_timestamp_(kNoTimestamp()) {}

void OpusAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  const PipelineStatusCB& status_cb,
                                  const OutputCB& output_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  PipelineStatusCB initialize_cb = BindToCurrentLoop(status_cb);

  config_ = config;
  output_cb_ = BindToCurrentLoop(output_cb);

  if (!ConfigureDecoder()) {
    initialize_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  initialize_cb.Run(PIPELINE_OK);
}

void OpusAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb.is_null());

  DecodeBuffer(buffer, BindToCurrentLoop(decode_cb));
}

void OpusAudioDecoder::Reset(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  opus_multistream_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
  ResetTimestampState();
  task_runner_->PostTask(FROM_HERE, closure);
}

void OpusAudioDecoder::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!opus_decoder_)
    return;

  opus_multistream_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
  ResetTimestampState();
  CloseDecoder();
}

OpusAudioDecoder::~OpusAudioDecoder() {}

void OpusAudioDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& input,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb.is_null());

  DCHECK(input.get());

  // Libopus does not buffer output. Decoding is complete when an end of stream
  // input buffer is received.
  if (input->end_of_stream()) {
    decode_cb.Run(kOk);
    return;
  }

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (input->timestamp() == kNoTimestamp()) {
    DLOG(ERROR) << "Received a buffer without timestamps!";
    decode_cb.Run(kDecodeError);
    return;
  }

  // Apply the necessary codec delay.
  if (start_input_timestamp_ == kNoTimestamp())
    start_input_timestamp_ = input->timestamp();
  if (!discard_helper_->initialized() &&
      input->timestamp() == start_input_timestamp_) {
    discard_helper_->Reset(config_.codec_delay());
  }

  scoped_refptr<AudioBuffer> output_buffer;

  if (!Decode(input, &output_buffer)) {
    decode_cb.Run(kDecodeError);
    return;
  }

  if (output_buffer) {
    output_cb_.Run(output_buffer);
  }

  decode_cb.Run(kOk);
}

bool OpusAudioDecoder::ConfigureDecoder() {
  if (config_.codec() != kCodecOpus) {
    DVLOG(1) << "Codec must be kCodecOpus.";
    return false;
  }

  const int channel_count =
      ChannelLayoutToChannelCount(config_.channel_layout());
  if (!config_.IsValidConfig() || channel_count > kMaxVorbisChannels) {
    DLOG(ERROR) << "Invalid or unsupported audio stream -"
                << " codec: " << config_.codec()
                << " channel count: " << channel_count
                << " channel layout: " << config_.channel_layout()
                << " bits per channel: " << config_.bits_per_channel()
                << " samples per second: " << config_.samples_per_second();
    return false;
  }

  if (config_.is_encrypted()) {
    DLOG(ERROR) << "Encrypted audio stream not supported.";
    return false;
  }

  // Clean up existing decoder if necessary.
  CloseDecoder();

  // Parse the Opus Extra Data.
  OpusExtraData opus_extra_data;
  if (!ParseOpusExtraData(config_.extra_data(), config_.extra_data_size(),
                          config_,
                          &opus_extra_data))
    return false;

  if (config_.codec_delay() < 0) {
    DLOG(ERROR) << "Invalid file. Incorrect value for codec delay: "
                << config_.codec_delay();
    return false;
  }

  if (config_.codec_delay() != opus_extra_data.skip_samples) {
    DLOG(ERROR) << "Invalid file. Codec Delay in container does not match the "
                << "value in Opus Extra Data. " << config_.codec_delay()
                << " vs " << opus_extra_data.skip_samples;
    return false;
  }

  uint8 channel_mapping[kMaxVorbisChannels] = {0};
  memcpy(&channel_mapping,
         kDefaultOpusChannelLayout,
         kMaxChannelsWithDefaultLayout);

  if (channel_count > kMaxChannelsWithDefaultLayout) {
    RemapOpusChannelLayout(opus_extra_data.stream_map,
                           channel_count,
                           channel_mapping);
  }

  // Init Opus.
  int status = OPUS_INVALID_STATE;
  opus_decoder_ = opus_multistream_decoder_create(config_.samples_per_second(),
                                                  channel_count,
                                                  opus_extra_data.num_streams,
                                                  opus_extra_data.num_coupled,
                                                  channel_mapping,
                                                  &status);
  if (!opus_decoder_ || status != OPUS_OK) {
    DLOG(ERROR) << "opus_multistream_decoder_create failed status="
                << opus_strerror(status);
    return false;
  }

  status = opus_multistream_decoder_ctl(
      opus_decoder_, OPUS_SET_GAIN(opus_extra_data.gain_db));
  if (status != OPUS_OK) {
    DLOG(ERROR) << "Failed to set OPUS header gain; status="
                << opus_strerror(status);
    return false;
  }

  discard_helper_.reset(
      new AudioDiscardHelper(config_.samples_per_second(), 0));
  start_input_timestamp_ = kNoTimestamp();
  return true;
}

void OpusAudioDecoder::CloseDecoder() {
  if (opus_decoder_) {
    opus_multistream_decoder_destroy(opus_decoder_);
    opus_decoder_ = NULL;
  }
}

void OpusAudioDecoder::ResetTimestampState() {
  discard_helper_->Reset(
      discard_helper_->TimeDeltaToFrames(config_.seek_preroll()));
}

bool OpusAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& input,
                              scoped_refptr<AudioBuffer>* output_buffer) {
  // Allocate a buffer for the output samples.
  *output_buffer = AudioBuffer::CreateBuffer(
      config_.sample_format(),
      config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()),
      config_.samples_per_second(),
      kMaxOpusOutputPacketSizeSamples);
  const int buffer_size =
      output_buffer->get()->channel_count() *
      output_buffer->get()->frame_count() *
      SampleFormatToBytesPerChannel(config_.sample_format());

  float* float_output_buffer = reinterpret_cast<float*>(
      output_buffer->get()->channel_data()[0]);
  const int frames_decoded =
      opus_multistream_decode_float(opus_decoder_,
                                    input->data(),
                                    input->data_size(),
                                    float_output_buffer,
                                    buffer_size,
                                    0);

  if (frames_decoded < 0) {
    DLOG(ERROR) << "opus_multistream_decode failed for"
                << " timestamp: " << input->timestamp().InMicroseconds()
                << " us, duration: " << input->duration().InMicroseconds()
                << " us, packet size: " << input->data_size() << " bytes with"
                << " status: " << opus_strerror(frames_decoded);
    return false;
  }

  // Trim off any extraneous allocation.
  DCHECK_LE(frames_decoded, output_buffer->get()->frame_count());
  const int trim_frames = output_buffer->get()->frame_count() - frames_decoded;
  if (trim_frames > 0)
    output_buffer->get()->TrimEnd(trim_frames);

  // Handles discards and timestamping.  Discard the buffer if more data needed.
  if (!discard_helper_->ProcessBuffers(input, *output_buffer))
    *output_buffer = NULL;

  return true;
}

}  // namespace media
