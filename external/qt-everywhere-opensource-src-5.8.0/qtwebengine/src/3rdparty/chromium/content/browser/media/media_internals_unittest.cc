// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace {
const int kTestComponentID = 0;
const char kTestDeviceID[] = "test-device-id";

// This class encapsulates a MediaInternals reference. It also has some useful
// methods to receive a callback, deserialize its associated data and expect
// integer/string values.
class MediaInternalsTestBase {
 public:
  MediaInternalsTestBase()
    : media_internals_(content::MediaInternals::GetInstance()) {
  }
  virtual ~MediaInternalsTestBase() {}

 protected:
  // Extracts and deserializes the JSON update data; merges into |update_data_|.
  void UpdateCallbackImpl(const base::string16& update) {
    // Each update string looks like "<JavaScript Function Name>({<JSON>});"
    // or for video capabilities: "<JavaScript Function Name>([{<JSON>}]);".
    // In the second case we will be able to extract the dictionary if it is the
    // only member of the list.
    // To use the JSON reader we need to strip out the JS function name and ().
    std::string utf8_update = base::UTF16ToUTF8(update);
    const std::string::size_type first_brace = utf8_update.find('{');
    const std::string::size_type last_brace = utf8_update.rfind('}');
    std::unique_ptr<base::Value> output_value = base::JSONReader::Read(
        utf8_update.substr(first_brace, last_brace - first_brace + 1));
    CHECK(output_value);

    base::DictionaryValue* output_dict = NULL;
    CHECK(output_value->GetAsDictionary(&output_dict));
    update_data_.MergeDictionary(output_dict);
  }

  void ExpectInt(const std::string& key, int expected_value) const {
    int actual_value = 0;
    ASSERT_TRUE(update_data_.GetInteger(key, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  void ExpectString(const std::string& key,
                    const std::string& expected_value) const {
    std::string actual_value;
    ASSERT_TRUE(update_data_.GetString(key, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  void ExpectStatus(const std::string& expected_value) const {
    ExpectString("status", expected_value);
  }

  void ExpectListOfStrings(const std::string& key,
                           const base::ListValue& expected_list) const {
    const base::ListValue* actual_list;
    ASSERT_TRUE(update_data_.GetList(key, &actual_list));
    const size_t expected_size = expected_list.GetSize();
    const size_t actual_size = actual_list->GetSize();
    ASSERT_EQ(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; ++i) {
      std::string expected_value, actual_value;
      ASSERT_TRUE(expected_list.GetString(i, &expected_value));
      ASSERT_TRUE(actual_list->GetString(i, &actual_value));
      EXPECT_EQ(expected_value, actual_value);
    }
  }

  const content::TestBrowserThreadBundle thread_bundle_;
  base::DictionaryValue update_data_;
  content::MediaInternals* const media_internals_;
};

}  // namespace

namespace content {

class MediaInternalsVideoCaptureDeviceTest : public testing::Test,
                                             public MediaInternalsTestBase {
 public:
  MediaInternalsVideoCaptureDeviceTest()
      : update_cb_(base::Bind(
            &MediaInternalsVideoCaptureDeviceTest::UpdateCallbackImpl,
            base::Unretained(this))) {
    media_internals_->AddUpdateCallback(update_cb_);
  }

  ~MediaInternalsVideoCaptureDeviceTest() override {
    media_internals_->RemoveUpdateCallback(update_cb_);
  }

 protected:
  MediaInternals::UpdateCallback update_cb_;
};

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
TEST_F(MediaInternalsVideoCaptureDeviceTest,
       AllCaptureApiTypesHaveProperStringRepresentation) {
  typedef media::VideoCaptureDevice::Name VideoCaptureDeviceName;
  typedef std::map<VideoCaptureDeviceName::CaptureApiType, std::string>
      CaptureApiTypeStringMap;
  CaptureApiTypeStringMap m;
#if defined(OS_LINUX)
  m[VideoCaptureDeviceName::V4L2_SINGLE_PLANE] = "V4L2 SPLANE";
#elif defined(OS_WIN)
  m[VideoCaptureDeviceName::MEDIA_FOUNDATION] = "Media Foundation";
  m[VideoCaptureDeviceName::DIRECT_SHOW] = "Direct Show";
#elif defined(OS_MACOSX)
  m[VideoCaptureDeviceName::AVFOUNDATION] = "AV Foundation";
  m[VideoCaptureDeviceName::DECKLINK] = "DeckLink";
#elif defined(OS_ANDROID)
  m[VideoCaptureDeviceName::API1] = "Camera API1";
  m[VideoCaptureDeviceName::API2_LEGACY] = "Camera API2 Legacy";
  m[VideoCaptureDeviceName::API2_FULL] = "Camera API2 Full";
  m[VideoCaptureDeviceName::API2_LIMITED] = "Camera API2 Limited";
  m[VideoCaptureDeviceName::TANGO] = "Tango API";
#endif
  EXPECT_EQ(media::VideoCaptureDevice::Name::API_TYPE_UNKNOWN, m.size());
  for (CaptureApiTypeStringMap::iterator it = m.begin(); it != m.end(); ++it) {
    const VideoCaptureDeviceName device_name("dummy", "dummy", it->first);
    EXPECT_EQ(it->second, device_name.GetCaptureApiTypeString());
  }
}
#endif

TEST_F(MediaInternalsVideoCaptureDeviceTest,
       VideoCaptureFormatStringIsInExpectedFormat) {
  // Since media internals will send video capture capabilities to JavaScript in
  // an expected format and there are no public methods for accessing the
  // resolutions, frame rates or pixel formats, this test checks that the format
  // has not changed. If the test fails because of the changed format, it should
  // be updated at the same time as the media internals JS files.
  const float kFrameRate = 30.0f;
  const gfx::Size kFrameSize(1280, 720);
  const media::VideoPixelFormat kPixelFormat =
      media::PIXEL_FORMAT_I420;
  const media::VideoPixelStorage kPixelStorage = media::PIXEL_STORAGE_CPU;
  const media::VideoCaptureFormat capture_format(kFrameSize, kFrameRate,
                                                 kPixelFormat, kPixelStorage);
  const std::string expected_string = base::StringPrintf(
      "(%s)@%.3ffps, pixel format: %s, storage: %s",
      kFrameSize.ToString().c_str(), kFrameRate,
      media::VideoPixelFormatToString(kPixelFormat).c_str(),
      media::VideoCaptureFormat::PixelStorageToString(kPixelStorage).c_str());
  EXPECT_EQ(expected_string,
            media::VideoCaptureFormat::ToString(capture_format));
}

TEST_F(MediaInternalsVideoCaptureDeviceTest,
       NotifyVideoCaptureDeviceCapabilitiesEnumerated) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const float kFrameRate = 30.0f;
  const media::VideoPixelFormat kPixelFormat =
      media::PIXEL_FORMAT_I420;
  const media::VideoCaptureFormat format_hd({kWidth, kHeight},
      kFrameRate, kPixelFormat);
  media::VideoCaptureFormats formats{};
  formats.push_back(format_hd);
  const media::VideoCaptureDeviceInfo device_info(
#if defined(OS_MACOSX)
      media::VideoCaptureDevice::Name(
          "dummy", "dummy", media::VideoCaptureDevice::Name::AVFOUNDATION),
#elif defined(OS_WIN)
      media::VideoCaptureDevice::Name("dummy", "dummy",
          media::VideoCaptureDevice::Name::DIRECT_SHOW),
#elif defined(OS_LINUX)
      media::VideoCaptureDevice::Name(
          "dummy", "/dev/dummy",
          media::VideoCaptureDevice::Name::V4L2_SINGLE_PLANE),
#elif defined(OS_ANDROID)
      media::VideoCaptureDevice::Name("dummy", "dummy",
          media::VideoCaptureDevice::Name::API2_LEGACY),
#else
      media::VideoCaptureDevice::Name("dummy", "dummy"),
#endif
      formats);
  media::VideoCaptureDeviceInfos device_infos{};
  device_infos.push_back(device_info);

  // When updating video capture capabilities, the update will serialize
  // a JSON array of objects to string. So here, the |UpdateCallbackImpl| will
  // deserialize the first object in the array. This means we have to have
  // exactly one device_info in the |device_infos|.
  media_internals_->UpdateVideoCaptureDeviceCapabilities(device_infos);

#if defined(OS_LINUX)
  ExpectString("id", "/dev/dummy");
#else
  ExpectString("id", "dummy");
#endif
  ExpectString("name", "dummy");
  base::ListValue expected_list;
  expected_list.AppendString(media::VideoCaptureFormat::ToString(format_hd));
  ExpectListOfStrings("formats", expected_list);
#if defined(OS_LINUX)
  ExpectString("captureApi", "V4L2 SPLANE");
#elif defined(OS_WIN)
  ExpectString("captureApi", "Direct Show");
#elif defined(OS_MACOSX)
  ExpectString("captureApi", "AV Foundation");
#elif defined(OS_ANDROID)
  ExpectString("captureApi", "Camera API2 Legacy");
#endif
}

class MediaInternalsAudioLogTest
    : public MediaInternalsTestBase,
      public testing::TestWithParam<media::AudioLogFactory::AudioComponent> {
 public:
  MediaInternalsAudioLogTest()
      : update_cb_(base::Bind(&MediaInternalsAudioLogTest::UpdateCallbackImpl,
                              base::Unretained(this))),
        test_params_(MakeAudioParams()),
        test_component_(GetParam()),
        audio_log_(media_internals_->CreateAudioLog(test_component_)) {
    media_internals_->AddUpdateCallback(update_cb_);
  }

  virtual ~MediaInternalsAudioLogTest() {
    media_internals_->RemoveUpdateCallback(update_cb_);
  }

 protected:
  MediaInternals::UpdateCallback update_cb_;
  const media::AudioParameters test_params_;
  const media::AudioLogFactory::AudioComponent test_component_;
  std::unique_ptr<media::AudioLog> audio_log_;

 private:
  static media::AudioParameters MakeAudioParams() {
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LINEAR,
                                  media::CHANNEL_LAYOUT_MONO, 48000, 16, 128);
    params.set_effects(media::AudioParameters::ECHO_CANCELLER |
                       media::AudioParameters::DUCKING);
    return params;
  }
};

TEST_P(MediaInternalsAudioLogTest, AudioLogCreateStartStopErrorClose) {
  audio_log_->OnCreated(kTestComponentID, test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();

  ExpectString("channel_layout",
               media::ChannelLayoutToString(test_params_.channel_layout()));
  ExpectInt("sample_rate", test_params_.sample_rate());
  ExpectInt("frames_per_buffer", test_params_.frames_per_buffer());
  ExpectInt("channels", test_params_.channels());
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

TEST_P(MediaInternalsAudioLogTest, AudioLogCreateClose) {
  audio_log_->OnCreated(kTestComponentID, test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("created");

  audio_log_->OnClosed(kTestComponentID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("closed");
}

INSTANTIATE_TEST_CASE_P(
    MediaInternalsAudioLogTest, MediaInternalsAudioLogTest, testing::Values(
        media::AudioLogFactory::AUDIO_INPUT_CONTROLLER,
        media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER,
        media::AudioLogFactory::AUDIO_OUTPUT_STREAM));

}  // namespace content
