// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc/proto_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/remoting/remoting_rpc_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

class ProtoUtilsTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ProtoUtilsTest, PassEOSDecoderBuffer) {
  // 1. To DecoderBuffer
  scoped_refptr<::media::DecoderBuffer> input_buffer =
      ::media::DecoderBuffer::CreateEOSBuffer();

  // 2. To Byte Array
  std::vector<uint8_t> data = DecoderBufferToByteArray(input_buffer);

  // 3. To DecoderBuffer
  scoped_refptr<::media::DecoderBuffer> output_buffer =
      ByteArrayToDecoderBuffer(data.data(), data.size());
  DCHECK(output_buffer);

  ASSERT_TRUE(output_buffer->end_of_stream());
}

TEST_F(ProtoUtilsTest, PassValidDecoderBuffer) {
  const uint8_t buffer[] = {
      0,   0,   0,   1,   9,   224, 0,   0,   0,   1,   103, 77,  64,  21,  217,
      1,   177, 254, 78,  16,  0,   0,   62,  144, 0,   11,  184, 0,   241, 98,
      228, 128, 0,   0,   0,   1,   104, 235, 143, 32,  0,   0,   0,   1,   103,
      77,  64,  21,  217, 1,   177, 254, 78,  16,  0,   0,   62,  144, 0,   11,
      184, 0,   241, 98,  228, 128, 0,   0,   0,   1,   104, 235, 143, 32,  0,
      0,   0,   1,   101, 136, 132, 25,  255, 0,   191, 98,  0,   6,   29,  63,
      252, 65,  246, 207, 255, 235, 63,  172, 35,  112, 198, 115, 222, 243, 159,
      232, 208, 32,  0,   0,   3,   0,   0,   203, 255, 149, 20,  71,  203, 213,
      40,  0,   0,   139, 0,   24,  117, 166, 249, 227, 68,  230, 177, 134, 161,
      162, 1,   22,  105, 78,  66,  183, 130, 158, 108, 252, 112, 113, 58,  159,
      72,  116, 78,  141, 133, 76,  225, 209, 13,  221, 49,  187, 83,  123, 193,
      112, 123, 112, 74,  121, 133};
  size_t buffer_size = sizeof(buffer) / sizeof(uint8_t);
  const uint8_t side_buffer[] = "XX";
  size_t side_buffer_size = sizeof(side_buffer) / sizeof(uint8_t);
  base::TimeDelta pts = base::TimeDelta::FromMilliseconds(5);

  // 1. To DecoderBuffer
  scoped_refptr<::media::DecoderBuffer> input_buffer =
      ::media::DecoderBuffer::CopyFrom(buffer, buffer_size, side_buffer,
                                       side_buffer_size);
  input_buffer->set_timestamp(pts);
  input_buffer->set_is_key_frame(true);

  // 2. To Byte Array
  std::vector<uint8_t> data = DecoderBufferToByteArray(input_buffer);

  // 3. To DecoderBuffer
  scoped_refptr<::media::DecoderBuffer> output_buffer =
      ByteArrayToDecoderBuffer(data.data(), data.size());
  DCHECK(output_buffer);

  ASSERT_FALSE(output_buffer->end_of_stream());
  ASSERT_TRUE(output_buffer->is_key_frame());
  ASSERT_EQ(output_buffer->timestamp(), pts);
  ASSERT_EQ(output_buffer->data_size(), buffer_size);
  const uint8_t* output_data = output_buffer->data();
  for (size_t i = 0; i < buffer_size; i++) {
    ASSERT_EQ(output_data[i], buffer[i]);
  }
  ASSERT_EQ(output_buffer->side_data_size(), side_buffer_size);
  const uint8_t* output_side_data = output_buffer->side_data();
  for (size_t i = 0; i < side_buffer_size; i++) {
    ASSERT_EQ(output_side_data[i], side_buffer[i]);
  }
}

TEST_F(ProtoUtilsTest, AudioDecoderConfigConversionTest) {
  const std::string extra_data = "ACEG";
  const EncryptionScheme encryption_scheme(
      EncryptionScheme::CIPHER_MODE_AES_CTR, EncryptionScheme::Pattern(20, 40));
  AudioDecoderConfig audio_config(
      kCodecAAC, kSampleFormatF32, CHANNEL_LAYOUT_MONO, 48000,
      std::vector<uint8_t>(extra_data.begin(), extra_data.end()),
      encryption_scheme);
  ASSERT_TRUE(audio_config.IsValidConfig());

  pb::AudioDecoderConfig audio_message;
  ConvertAudioDecoderConfigToProto(audio_config, &audio_message);

  AudioDecoderConfig audio_output_config;
  ASSERT_TRUE(
      ConvertProtoToAudioDecoderConfig(audio_message, &audio_output_config));

  ASSERT_TRUE(audio_config.Matches(audio_output_config));
}

TEST_F(ProtoUtilsTest, CdmPromiseResultConversion) {
  CdmPromiseResult success_result = CdmPromiseResult::SuccessResult();

  pb::CdmPromise promise_message;
  ConvertCdmPromiseToProto(success_result, &promise_message);

  CdmPromiseResult output_result;
  ASSERT_TRUE(ConvertProtoToCdmPromise(promise_message, &output_result));

  ASSERT_EQ(success_result.success(), output_result.success());
  ASSERT_EQ(success_result.exception(), output_result.exception());
  ASSERT_EQ(success_result.system_code(), output_result.system_code());
  ASSERT_EQ(success_result.error_message(), output_result.error_message());
}

TEST_F(ProtoUtilsTest, CdmKeyInformationConversion) {
  std::unique_ptr<CdmKeyInformation> cdm_key_info_1(new CdmKeyInformation(
      "key_1", CdmKeyInformation::OUTPUT_RESTRICTED, 100));
  std::unique_ptr<CdmKeyInformation> cdm_key_info_2(
      new CdmKeyInformation("key_2", CdmKeyInformation::EXPIRED, 11));
  std::unique_ptr<CdmKeyInformation> cdm_key_info_3(
      new CdmKeyInformation("key_3", CdmKeyInformation::RELEASED, 22));
  CdmKeysInfo keys_information;
  keys_information.push_back(std::move(cdm_key_info_1));
  keys_information.push_back(std::move(cdm_key_info_2));
  keys_information.push_back(std::move(cdm_key_info_3));

  pb::CdmClientOnSessionKeysChange key_message;
  ConvertCdmKeyInfoToProto(keys_information, &key_message);

  CdmKeysInfo key_output_information;
  ConvertProtoToCdmKeyInfo(key_message, &key_output_information);

  ASSERT_EQ(keys_information.size(), key_output_information.size());
  for (uint32_t i = 0; i < 3; i++) {
    ASSERT_EQ(keys_information[i]->key_id, key_output_information[i]->key_id);
    ASSERT_EQ(keys_information[i]->status, key_output_information[i]->status);
    ASSERT_EQ(keys_information[i]->system_code,
              key_output_information[i]->system_code);
  }
}

}  // namespace remoting
}  // namespace media
