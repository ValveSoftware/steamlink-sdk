// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kTestComponentID = 0;
const char kTestDeviceID[] = "test-device-id";
}  // namespace

namespace content {

class MediaInternalsTest
    : public testing::TestWithParam<media::AudioLogFactory::AudioComponent> {
 public:
  MediaInternalsTest()
      : media_internals_(MediaInternals::GetInstance()),
        update_cb_(base::Bind(&MediaInternalsTest::UpdateCallbackImpl,
                              base::Unretained(this))),
        test_params_(media::AudioParameters::AUDIO_PCM_LINEAR,
                     media::CHANNEL_LAYOUT_MONO,
                     0,
                     48000,
                     16,
                     128,
                     media::AudioParameters::ECHO_CANCELLER |
                     media::AudioParameters::DUCKING),
        test_component_(GetParam()),
        audio_log_(media_internals_->CreateAudioLog(test_component_)) {
    media_internals_->AddUpdateCallback(update_cb_);
  }

  virtual ~MediaInternalsTest() {
    media_internals_->RemoveUpdateCallback(update_cb_);
  }

 protected:
  // Extracts and deserializes the JSON update data; merges into |update_data_|.
  void UpdateCallbackImpl(const base::string16& update) {
    // Each update string looks like "<JavaScript Function Name>({<JSON>});", to
    // use the JSON reader we need to strip out the JavaScript code.
    std::string utf8_update = base::UTF16ToUTF8(update);
    const std::string::size_type first_brace = utf8_update.find('{');
    const std::string::size_type last_brace = utf8_update.rfind('}');
    scoped_ptr<base::Value> output_value(base::JSONReader::Read(
        utf8_update.substr(first_brace, last_brace - first_brace + 1)));
    CHECK(output_value);

    base::DictionaryValue* output_dict = NULL;
    CHECK(output_value->GetAsDictionary(&output_dict));
    update_data_.MergeDictionary(output_dict);
  }

  void ExpectInt(const std::string& key, int expected_value) {
    int actual_value = 0;
    ASSERT_TRUE(update_data_.GetInteger(key, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  void ExpectString(const std::string& key, const std::string& expected_value) {
    std::string actual_value;
    ASSERT_TRUE(update_data_.GetString(key, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  void ExpectStatus(const std::string& expected_value) {
    ExpectString("status", expected_value);
  }

  TestBrowserThreadBundle thread_bundle_;
  MediaInternals* const media_internals_;
  MediaInternals::UpdateCallback update_cb_;
  base::DictionaryValue update_data_;
  const media::AudioParameters test_params_;
  const media::AudioLogFactory::AudioComponent test_component_;
  scoped_ptr<media::AudioLog> audio_log_;
};

TEST_P(MediaInternalsTest, AudioLogCreateStartStopErrorClose) {
  audio_log_->OnCreated(
      kTestComponentID, test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();

  ExpectString("channel_layout",
               media::ChannelLayoutToString(test_params_.channel_layout()));
  ExpectInt("sample_rate", test_params_.sample_rate());
  ExpectInt("frames_per_buffer", test_params_.frames_per_buffer());
  ExpectInt("channels", test_params_.channels());
  ExpectInt("input_channels", test_params_.input_channels());
  ExpectString("effects", "ECHO_CANCELLER | DUCKING");
  ExpectString("device_id", kTestDeviceID);
  ExpectInt("component_id", kTestComponentID);
  ExpectInt("component_type", test_component_);
  ExpectStatus("created");

  // Verify OnStarted().
  audio_log_->OnStarted(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("started");

  // Verify OnStopped().
  audio_log_->OnStopped(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("stopped");

  // Verify OnError().
  const char kErrorKey[] = "error_occurred";
  std::string no_value;
  ASSERT_FALSE(update_data_.GetString(kErrorKey, &no_value));
  audio_log_->OnError(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectString(kErrorKey, "true");

  // Verify OnClosed().
  audio_log_->OnClosed(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("closed");
}

TEST_P(MediaInternalsTest, AudioLogCreateClose) {
  audio_log_->OnCreated(
      kTestComponentID, test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("created");

  audio_log_->OnClosed(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("closed");
}

INSTANTIATE_TEST_CASE_P(
    MediaInternalsTest, MediaInternalsTest, testing::Values(
        media::AudioLogFactory::AUDIO_INPUT_CONTROLLER,
        media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER,
        media::AudioLogFactory::AUDIO_OUTPUT_STREAM));

}  // namespace content
