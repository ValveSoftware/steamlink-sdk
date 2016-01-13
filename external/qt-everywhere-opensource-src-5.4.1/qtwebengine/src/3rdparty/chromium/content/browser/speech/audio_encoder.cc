// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_encoder.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/speech/audio_buffer.h"
#include "third_party/flac/include/FLAC/stream_encoder.h"
#include "third_party/speex/include/speex/speex.h"

namespace content {
namespace {

//-------------------------------- FLACEncoder ---------------------------------

const char* const kContentTypeFLAC = "audio/x-flac; rate=";
const int kFLACCompressionLevel = 0;  // 0 for speed

class FLACEncoder : public AudioEncoder {
 public:
  FLACEncoder(int sampling_rate, int bits_per_sample);
  virtual ~FLACEncoder();
  virtual void Encode(const AudioChunk& raw_audio) OVERRIDE;
  virtual void Flush() OVERRIDE;

 private:
  static FLAC__StreamEncoderWriteStatus WriteCallback(
      const FLAC__StreamEncoder* encoder,
      const FLAC__byte buffer[],
      size_t bytes,
      unsigned samples,
      unsigned current_frame,
      void* client_data);

  FLAC__StreamEncoder* encoder_;
  bool is_encoder_initialized_;

  DISALLOW_COPY_AND_ASSIGN(FLACEncoder);
};

FLAC__StreamEncoderWriteStatus FLACEncoder::WriteCallback(
    const FLAC__StreamEncoder* encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void* client_data) {
  FLACEncoder* me = static_cast<FLACEncoder*>(client_data);
  DCHECK(me->encoder_ == encoder);
  me->encoded_audio_buffer_.Enqueue(buffer, bytes);
  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLACEncoder::FLACEncoder(int sampling_rate, int bits_per_sample)
    : AudioEncoder(std::string(kContentTypeFLAC) +
                   base::IntToString(sampling_rate),
                   bits_per_sample),
      encoder_(FLAC__stream_encoder_new()),
      is_encoder_initialized_(false) {
  FLAC__stream_encoder_set_channels(encoder_, 1);
  FLAC__stream_encoder_set_bits_per_sample(encoder_, bits_per_sample);
  FLAC__stream_encoder_set_sample_rate(encoder_, sampling_rate);
  FLAC__stream_encoder_set_compression_level(encoder_, kFLACCompressionLevel);

  // Initializing the encoder will cause sync bytes to be written to
  // its output stream, so we wait until the first call to this method
  // before doing so.
}

FLACEncoder::~FLACEncoder() {
  FLAC__stream_encoder_delete(encoder_);
}

void FLACEncoder::Encode(const AudioChunk& raw_audio) {
  DCHECK_EQ(raw_audio.bytes_per_sample(), 2);
  if (!is_encoder_initialized_) {
    const FLAC__StreamEncoderInitStatus encoder_status =
        FLAC__stream_encoder_init_stream(encoder_, WriteCallback, NULL, NULL,
                                         NULL, this);
    DCHECK_EQ(encoder_status, FLAC__STREAM_ENCODER_INIT_STATUS_OK);
    is_encoder_initialized_ = true;
  }

  // FLAC encoder wants samples as int32s.
  const int num_samples = raw_audio.NumSamples();
  scoped_ptr<FLAC__int32[]> flac_samples(new FLAC__int32[num_samples]);
  FLAC__int32* flac_samples_ptr = flac_samples.get();
  for (int i = 0; i < num_samples; ++i)
    flac_samples_ptr[i] = static_cast<FLAC__int32>(raw_audio.GetSample16(i));

  FLAC__stream_encoder_process(encoder_, &flac_samples_ptr, num_samples);
}

void FLACEncoder::Flush() {
  FLAC__stream_encoder_finish(encoder_);
}

//-------------------------------- SpeexEncoder --------------------------------

const char* const kContentTypeSpeex = "audio/x-speex-with-header-byte; rate=";
const int kSpeexEncodingQuality = 8;
const int kMaxSpeexFrameLength = 110;  // (44kbps rate sampled at 32kHz).

// Since the frame length gets written out as a byte in the encoded packet,
// make sure it is within the byte range.
COMPILE_ASSERT(kMaxSpeexFrameLength <= 0xFF, invalidLength);

class SpeexEncoder : public AudioEncoder {
 public:
  explicit SpeexEncoder(int sampling_rate, int bits_per_sample);
  virtual ~SpeexEncoder();
  virtual void Encode(const AudioChunk& raw_audio) OVERRIDE;
  virtual void Flush() OVERRIDE {}

 private:
  void* encoder_state_;
  SpeexBits bits_;
  int samples_per_frame_;
  char encoded_frame_data_[kMaxSpeexFrameLength + 1];  // +1 for the frame size.
  DISALLOW_COPY_AND_ASSIGN(SpeexEncoder);
};

SpeexEncoder::SpeexEncoder(int sampling_rate, int bits_per_sample)
    : AudioEncoder(std::string(kContentTypeSpeex) +
                   base::IntToString(sampling_rate),
                   bits_per_sample) {
   // speex_bits_init() does not initialize all of the |bits_| struct.
   memset(&bits_, 0, sizeof(bits_));
   speex_bits_init(&bits_);
   encoder_state_ = speex_encoder_init(&speex_wb_mode);
   DCHECK(encoder_state_);
   speex_encoder_ctl(encoder_state_, SPEEX_GET_FRAME_SIZE, &samples_per_frame_);
   DCHECK(samples_per_frame_ > 0);
   int quality = kSpeexEncodingQuality;
   speex_encoder_ctl(encoder_state_, SPEEX_SET_QUALITY, &quality);
   int vbr = 1;
   speex_encoder_ctl(encoder_state_, SPEEX_SET_VBR, &vbr);
   memset(encoded_frame_data_, 0, sizeof(encoded_frame_data_));
}

SpeexEncoder::~SpeexEncoder() {
  speex_bits_destroy(&bits_);
  speex_encoder_destroy(encoder_state_);
}

void SpeexEncoder::Encode(const AudioChunk& raw_audio) {
  spx_int16_t* src_buffer =
      const_cast<spx_int16_t*>(raw_audio.SamplesData16());
  int num_samples = raw_audio.NumSamples();
  // Drop incomplete frames, typically those which come in when recording stops.
  num_samples -= (num_samples % samples_per_frame_);
  for (int i = 0; i < num_samples; i += samples_per_frame_) {
    speex_bits_reset(&bits_);
    speex_encode_int(encoder_state_, src_buffer + i, &bits_);

    // Encode the frame and place the size of the frame as the first byte. This
    // is the packet format for MIME type x-speex-with-header-byte.
    int frame_length = speex_bits_write(&bits_, encoded_frame_data_ + 1,
                                        kMaxSpeexFrameLength);
    encoded_frame_data_[0] = static_cast<char>(frame_length);
    encoded_audio_buffer_.Enqueue(
        reinterpret_cast<uint8*>(&encoded_frame_data_[0]), frame_length + 1);
  }
}

}  // namespace

AudioEncoder* AudioEncoder::Create(Codec codec,
                                   int sampling_rate,
                                   int bits_per_sample) {
  if (codec == CODEC_FLAC)
    return new FLACEncoder(sampling_rate, bits_per_sample);
  return new SpeexEncoder(sampling_rate, bits_per_sample);
}

AudioEncoder::AudioEncoder(const std::string& mime_type, int bits_per_sample)
    : encoded_audio_buffer_(1), /* Byte granularity of encoded samples. */
      mime_type_(mime_type),
      bits_per_sample_(bits_per_sample) {
}

AudioEncoder::~AudioEncoder() {
}

scoped_refptr<AudioChunk> AudioEncoder::GetEncodedDataAndClear() {
  return encoded_audio_buffer_.DequeueAll();
}

}  // namespace content
