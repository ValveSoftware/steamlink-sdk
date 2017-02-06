// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

void CompareBytes(uint8_t* original_data, uint8_t* result_data, size_t length) {
  EXPECT_GT(length, 0u);
  EXPECT_EQ(memcmp(original_data, result_data, length), 0);
}

// Compare the actual video frame bytes (|rows| rows of |row|bytes| data),
// skipping any padding that may be in either frame.
void CompareRowBytes(uint8_t* original_data,
                     uint8_t* result_data,
                     size_t rows,
                     size_t row_bytes,
                     size_t original_stride,
                     size_t result_stride) {
  DCHECK_GE(original_stride, row_bytes);
  DCHECK_GE(result_stride, row_bytes);

  for (size_t i = 0; i < rows; ++i) {
    CompareBytes(original_data, result_data, row_bytes);
    original_data += original_stride;
    result_data += result_stride;
  }
}

void CompareAudioBuffers(SampleFormat sample_format,
                         const scoped_refptr<AudioBuffer>& original,
                         const scoped_refptr<AudioBuffer>& result) {
  EXPECT_EQ(original->frame_count(), result->frame_count());
  EXPECT_EQ(original->timestamp(), result->timestamp());
  EXPECT_EQ(original->duration(), result->duration());
  EXPECT_EQ(original->sample_rate(), result->sample_rate());
  EXPECT_EQ(original->channel_count(), result->channel_count());
  EXPECT_EQ(original->channel_layout(), result->channel_layout());
  EXPECT_EQ(original->end_of_stream(), result->end_of_stream());

  // Compare bytes in buffer.
  int bytes_per_channel =
      original->frame_count() * SampleFormatToBytesPerChannel(sample_format);
  if (IsPlanar(sample_format)) {
    for (int i = 0; i < original->channel_count(); ++i) {
      CompareBytes(original->channel_data()[i], result->channel_data()[i],
                   bytes_per_channel);
    }
    return;
  }

  DCHECK(IsInterleaved(sample_format)) << sample_format;
  CompareBytes(original->channel_data()[0], result->channel_data()[0],
               bytes_per_channel * original->channel_count());
}

void CompareVideoPlane(size_t plane,
                       const scoped_refptr<VideoFrame>& original,
                       const scoped_refptr<VideoFrame>& result) {
  EXPECT_EQ(original->stride(plane), result->stride(plane));
  EXPECT_EQ(original->row_bytes(plane), result->row_bytes(plane));
  EXPECT_EQ(original->rows(plane), result->rows(plane));
  CompareRowBytes(original->data(plane), result->data(plane),
                  original->rows(plane), original->row_bytes(plane),
                  original->stride(plane), result->stride(plane));
}

void CompareVideoFrames(const scoped_refptr<VideoFrame>& original,
                        const scoped_refptr<VideoFrame>& result) {
  if (original->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
    EXPECT_TRUE(result->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
    return;
  }

  EXPECT_EQ(original->format(), result->format());
  EXPECT_EQ(original->coded_size().height(), result->coded_size().height());
  EXPECT_EQ(original->coded_size().width(), result->coded_size().width());
  EXPECT_EQ(original->visible_rect().height(), result->visible_rect().height());
  EXPECT_EQ(original->visible_rect().width(), result->visible_rect().width());
  EXPECT_EQ(original->natural_size().height(), result->natural_size().height());
  EXPECT_EQ(original->natural_size().width(), result->natural_size().width());

  CompareVideoPlane(VideoFrame::kYPlane, original, result);
  CompareVideoPlane(VideoFrame::kUPlane, original, result);
  CompareVideoPlane(VideoFrame::kVPlane, original, result);
}

// Returns a color VideoFrame that stores the data in a
// mojo::SharedBufferHandle.
scoped_refptr<VideoFrame> CreateMojoSharedBufferColorFrame() {
  // Create a color VideoFrame to use as reference (data will need to be copied
  // to a mojo::SharedBufferHandle).
  const int kWidth = 16;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromSeconds(26);
  scoped_refptr<VideoFrame> color_frame(VideoFrame::CreateColorFrame(
      gfx::Size(kWidth, kHeight), 255, 128, 24, kTimestamp));

  // Allocate a mojo::SharedBufferHandle big enough to contain
  // |color_frame|'s data.
  const size_t allocation_size = VideoFrame::AllocationSize(
      color_frame->format(), color_frame->coded_size());
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(allocation_size);
  EXPECT_TRUE(handle.is_valid());

  // Create a MojoSharedBufferVideoFrame whose dimensions match |color_frame|.
  const size_t y_plane_size = color_frame->rows(VideoFrame::kYPlane) *
                              color_frame->stride(VideoFrame::kYPlane);
  const size_t u_plane_size = color_frame->rows(VideoFrame::kUPlane) *
                              color_frame->stride(VideoFrame::kUPlane);
  const size_t v_plane_size = color_frame->rows(VideoFrame::kVPlane) *
                              color_frame->stride(VideoFrame::kVPlane);
  scoped_refptr<VideoFrame> frame(MojoSharedBufferVideoFrame::Create(
      color_frame->format(), color_frame->coded_size(),
      color_frame->visible_rect(), color_frame->natural_size(),
      std::move(handle), allocation_size, 0, y_plane_size,
      y_plane_size + u_plane_size, color_frame->stride(VideoFrame::kYPlane),
      color_frame->stride(VideoFrame::kUPlane),
      color_frame->stride(VideoFrame::kVPlane), color_frame->timestamp()));
  EXPECT_EQ(color_frame->coded_size(), frame->coded_size());
  EXPECT_EQ(color_frame->visible_rect(), frame->visible_rect());
  EXPECT_EQ(color_frame->natural_size(), frame->natural_size());
  EXPECT_EQ(color_frame->rows(VideoFrame::kYPlane),
            frame->rows(VideoFrame::kYPlane));
  EXPECT_EQ(color_frame->rows(VideoFrame::kUPlane),
            frame->rows(VideoFrame::kUPlane));
  EXPECT_EQ(color_frame->rows(VideoFrame::kVPlane),
            frame->rows(VideoFrame::kVPlane));
  EXPECT_EQ(color_frame->stride(VideoFrame::kYPlane),
            frame->stride(VideoFrame::kYPlane));
  EXPECT_EQ(color_frame->stride(VideoFrame::kUPlane),
            frame->stride(VideoFrame::kUPlane));
  EXPECT_EQ(color_frame->stride(VideoFrame::kVPlane),
            frame->stride(VideoFrame::kVPlane));

  // Copy all the data from |color_frame| into |frame|.
  memcpy(frame->data(VideoFrame::kYPlane),
         color_frame->data(VideoFrame::kYPlane), y_plane_size);
  memcpy(frame->data(VideoFrame::kUPlane),
         color_frame->data(VideoFrame::kUPlane), u_plane_size);
  memcpy(frame->data(VideoFrame::kVPlane),
         color_frame->data(VideoFrame::kVPlane), v_plane_size);
  return frame;
}

}  // namespace

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_Normal) {
  const uint8_t kData[] = "hello, world";
  const uint8_t kSideData[] = "sideshow bob";
  const size_t kDataSize = arraysize(kData);
  const size_t kSideDataSize = arraysize(kSideData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize,
      reinterpret_cast<const uint8_t*>(&kSideData), kSideDataSize));
  buffer->set_timestamp(base::TimeDelta::FromMilliseconds(123));
  buffer->set_duration(base::TimeDelta::FromMilliseconds(456));
  buffer->set_splice_timestamp(base::TimeDelta::FromMilliseconds(200));
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_EQ(kSideDataSize, result->side_data_size());
  EXPECT_EQ(0, memcmp(result->side_data(), kSideData, kSideDataSize));
  EXPECT_EQ(buffer->timestamp(), result->timestamp());
  EXPECT_EQ(buffer->duration(), result->duration());
  EXPECT_EQ(buffer->is_key_frame(), result->is_key_frame());
  EXPECT_EQ(buffer->splice_timestamp(), result->splice_timestamp());
  EXPECT_EQ(buffer->discard_padding(), result->discard_padding());

  // Both |buffer| and |result| are not encrypted.
  EXPECT_FALSE(buffer->decrypt_config());
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS) {
  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_KeyFrame) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_TRUE(result->is_key_frame());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EncryptedBuffer) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_decrypt_config(
      base::WrapUnique(new DecryptConfig(kKeyId, kIv, subsamples)));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));

  // Test empty IV. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(base::WrapUnique(
      new DecryptConfig(kKeyId, "", std::vector<SubsampleEntry>())));
  result =
      mojom::DecoderBuffer::From(buffer).To<scoped_refptr<DecoderBuffer>>();
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));
  EXPECT_TRUE(buffer->decrypt_config()->iv().empty());
}

// TODO(tim): Check other properties.

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_Normal) {
  const uint8_t kExtraData[] = "config extra data";
  const std::vector<uint8_t> kExtraDataVector(
      &kExtraData[0], &kExtraData[0] + arraysize(kExtraData));

  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    kExtraDataVector, Unencrypted(), base::TimeDelta(), 0);
  mojom::AudioDecoderConfigPtr ptr(mojom::AudioDecoderConfig::From(config));
  EXPECT_FALSE(ptr->extra_data.is_null());
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_EmptyExtraData) {
  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    EmptyExtraData(), Unencrypted(), base::TimeDelta(), 0);
  mojom::AudioDecoderConfigPtr ptr(mojom::AudioDecoderConfig::From(config));
  EXPECT_TRUE(ptr->extra_data.is_null());
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_Encrypted) {
  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    EmptyExtraData(), AesCtrEncryptionScheme(),
                    base::TimeDelta(), 0);
  mojom::AudioDecoderConfigPtr ptr(mojom::AudioDecoderConfig::From(config));
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertVideoDecoderConfig_Normal) {
  const uint8_t kExtraData[] = "config extra data";
  const std::vector<uint8_t> kExtraDataVector(
      &kExtraData[0], &kExtraData[0] + arraysize(kExtraData));

  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_ANY, PIXEL_FORMAT_YV12,
                            COLOR_SPACE_UNSPECIFIED, kCodedSize, kVisibleRect,
                            kNaturalSize, kExtraDataVector, Unencrypted());
  mojom::VideoDecoderConfigPtr ptr(mojom::VideoDecoderConfig::From(config));
  EXPECT_FALSE(ptr->extra_data.is_null());
  VideoDecoderConfig result(ptr.To<VideoDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertVideoDecoderConfig_EmptyExtraData) {
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_ANY, PIXEL_FORMAT_YV12,
                            COLOR_SPACE_UNSPECIFIED, kCodedSize, kVisibleRect,
                            kNaturalSize, EmptyExtraData(), Unencrypted());
  mojom::VideoDecoderConfigPtr ptr(mojom::VideoDecoderConfig::From(config));
  EXPECT_TRUE(ptr->extra_data.is_null());
  VideoDecoderConfig result(ptr.To<VideoDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertVideoDecoderConfig_Encrypted) {
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_ANY, PIXEL_FORMAT_YV12,
                            COLOR_SPACE_UNSPECIFIED, kCodedSize, kVisibleRect,
                            kNaturalSize, EmptyExtraData(),
                            AesCtrEncryptionScheme());
  mojom::VideoDecoderConfigPtr ptr(mojom::VideoDecoderConfig::From(config));
  VideoDecoderConfig result(ptr.To<VideoDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertCdmConfig) {
  CdmConfig config;
  config.allow_distinctive_identifier = true;
  config.allow_persistent_state = false;
  config.use_hw_secure_codecs = true;

  mojom::CdmConfigPtr ptr(mojom::CdmConfig::From(config));
  CdmConfig result(ptr.To<CdmConfig>());

  EXPECT_EQ(config.allow_distinctive_identifier,
            result.allow_distinctive_identifier);
  EXPECT_EQ(config.allow_persistent_state, result.allow_persistent_state);
  EXPECT_EQ(config.use_hw_secure_codecs, result.use_hw_secure_codecs);
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_EOS) {
  // Original.
  scoped_refptr<AudioBuffer> buffer(AudioBuffer::CreateEOSBuffer());

  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_MONO) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  const int kSampleRate = 48000;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());

  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatU8, buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_FLOAT) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_4_0;
  const int kSampleRate = 48000;
  const base::TimeDelta start_time = base::TimeDelta::FromSecondsD(1000.0);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(
      kSampleFormatPlanarF32, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 0.0f, 1.0f,
      kSampleRate / 10, start_time);
  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatPlanarF32, buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_EOS) {
  // Original.
  scoped_refptr<VideoFrame> buffer(VideoFrame::CreateEOSFrame());

  // Convert to and back.
  mojom::VideoFramePtr ptr(mojom::VideoFrame::From(buffer));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());

  // Compare.
  CompareVideoFrames(buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_EmptyFrame) {
  // Original.
  scoped_refptr<VideoFrame> frame(MojoSharedBufferVideoFrame::CreateDefaultI420(
      gfx::Size(100, 100), base::TimeDelta::FromSeconds(100)));

  // Convert to and back.
  mojom::VideoFramePtr ptr(mojom::VideoFrame::From(frame));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());
  EXPECT_NE(result.get(), nullptr);

  // Compare.
  CompareVideoFrames(frame, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_ColorFrame) {
  scoped_refptr<VideoFrame> frame(CreateMojoSharedBufferColorFrame());

  // Convert to and back.
  mojom::VideoFramePtr ptr(mojom::VideoFrame::From(frame));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());
  EXPECT_NE(result.get(), nullptr);

  // Compare.
  CompareVideoFrames(frame, result);
}

TEST(MediaTypeConvertersTest, ConvertEncryptionSchemeAesCbcWithPattern) {
  // Original.
  EncryptionScheme scheme(EncryptionScheme::CIPHER_MODE_AES_CBC,
                          EncryptionScheme::Pattern(1, 9));

  // Convert to and back.
  mojom::EncryptionSchemePtr ptr(mojom::EncryptionScheme::From(scheme));
  EncryptionScheme result(ptr.To<EncryptionScheme>());

  EXPECT_TRUE(result.Matches(scheme));

  // Verify a couple of negative cases.
  EXPECT_FALSE(result.Matches(Unencrypted()));
  EXPECT_FALSE(result.Matches(AesCtrEncryptionScheme()));
}

}  // namespace media
