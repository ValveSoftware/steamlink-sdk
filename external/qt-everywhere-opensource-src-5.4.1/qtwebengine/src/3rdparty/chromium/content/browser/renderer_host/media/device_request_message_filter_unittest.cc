// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/media/device_request_message_filter.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace content {

static const std::string kAudioLabel = "audio_label";
static const std::string kVideoLabel = "video_label";

class MockMediaStreamManager : public MediaStreamManager {
 public:
  MockMediaStreamManager() {}

  virtual ~MockMediaStreamManager() {}

  MOCK_METHOD8(EnumerateDevices,
               std::string(MediaStreamRequester* requester,
                           int render_process_id,
                           int render_view_id,
                           const ResourceContext::SaltCallback& rc,
                           int page_request_id,
                           MediaStreamType type,
                           const GURL& security_origin,
                           bool have_permission));

  std::string DoEnumerateDevices(MediaStreamRequester* requester,
                                 int render_process_id,
                                 int render_view_id,
                                 const ResourceContext::SaltCallback& rc,
                                 int page_request_id,
                                 MediaStreamType type,
                                 const GURL& security_origin,
                                 bool have_permission) {
    if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
      return kAudioLabel;
    } else {
      return kVideoLabel;
    }
  }
};

class MockDeviceRequestMessageFilter : public DeviceRequestMessageFilter {
 public:
  MockDeviceRequestMessageFilter(MockResourceContext* context,
                                 MockMediaStreamManager* manager)
      : DeviceRequestMessageFilter(context, manager, 0), received_id_(-1) {}
  StreamDeviceInfoArray requested_devices() { return requested_devices_; }
  int received_id() { return received_id_; }

 private:
  virtual ~MockDeviceRequestMessageFilter() {}

  // Override the Send() method to intercept the message that we're sending to
  // the renderer.
  virtual bool Send(IPC::Message* reply_msg) OVERRIDE {
    CHECK(reply_msg);

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockDeviceRequestMessageFilter, *reply_msg)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_GetSourcesACK, SaveDevices)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete reply_msg;
    return true;
  }

  void SaveDevices(int request_id, const StreamDeviceInfoArray& devices) {
    received_id_ = request_id;
    requested_devices_ = devices;
  }

  int received_id_;
  StreamDeviceInfoArray requested_devices_;
};

class DeviceRequestMessageFilterTest : public testing::Test {
 public:
  DeviceRequestMessageFilterTest() : next_device_id_(0) {}

  void RunTest(int number_audio_devices, int number_video_devices) {
    AddAudioDevices(number_audio_devices);
    AddVideoDevices(number_video_devices);
    GURL origin("https://test.com");
    EXPECT_CALL(*media_stream_manager_,
                EnumerateDevices(_, _, _, _, _, MEDIA_DEVICE_AUDIO_CAPTURE,
                                 _, _))
        .Times(1);
    EXPECT_CALL(*media_stream_manager_,
                EnumerateDevices(_, _, _, _, _, MEDIA_DEVICE_VIDEO_CAPTURE,
                                 _, _))
        .Times(1);
    // Send message to get devices. Should trigger 2 EnumerateDevice() requests.
    const int kRequestId = 123;
    SendGetSourcesMessage(kRequestId, origin);

    // Run audio callback. Because there's still an outstanding video request,
    // this should not populate |message|.
    FireAudioDeviceCallback();
    EXPECT_EQ(0u, host_->requested_devices().size());

    // After the video device callback is fired, |message| should be populated.
    FireVideoDeviceCallback();
    EXPECT_EQ(static_cast<size_t>(number_audio_devices + number_video_devices),
              host_->requested_devices().size());

    EXPECT_EQ(kRequestId, host_->received_id());
  }

 protected:
  virtual ~DeviceRequestMessageFilterTest() {}

  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoopForIO);
    io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO, message_loop_.get()));

    media_stream_manager_.reset(new MockMediaStreamManager());
    ON_CALL(*media_stream_manager_, EnumerateDevices(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(media_stream_manager_.get(),
                              &MockMediaStreamManager::DoEnumerateDevices));

    resource_context_.reset(new MockResourceContext(NULL));
    host_ = new MockDeviceRequestMessageFilter(resource_context_.get(),
                                               media_stream_manager_.get());
  }

  scoped_refptr<MockDeviceRequestMessageFilter> host_;
  scoped_ptr<MockMediaStreamManager> media_stream_manager_;
  scoped_ptr<MockResourceContext> resource_context_;
  StreamDeviceInfoArray physical_audio_devices_;
  StreamDeviceInfoArray physical_video_devices_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestBrowserThread> io_thread_;

 private:
  void AddAudioDevices(int number_of_devices) {
    for (int i = 0; i < number_of_devices; i++) {
      physical_audio_devices_.push_back(
          StreamDeviceInfo(
              MEDIA_DEVICE_AUDIO_CAPTURE,
              "/dev/audio/" + base::IntToString(next_device_id_),
              "Audio Device" + base::IntToString(next_device_id_)));
      next_device_id_++;
    }
  }

  void AddVideoDevices(int number_of_devices) {
    for (int i = 0; i < number_of_devices; i++) {
      physical_video_devices_.push_back(
          StreamDeviceInfo(
              MEDIA_DEVICE_VIDEO_CAPTURE,
              "/dev/video/" + base::IntToString(next_device_id_),
              "Video Device" + base::IntToString(next_device_id_)));
      next_device_id_++;
    }
  }

  void SendGetSourcesMessage(int request_id, const GURL& origin) {
    host_->OnMessageReceived(MediaStreamHostMsg_GetSources(request_id, origin));
  }

  void FireAudioDeviceCallback() {
    host_->DevicesEnumerated(-1, -1, kAudioLabel, physical_audio_devices_);
  }

  void FireVideoDeviceCallback() {
    host_->DevicesEnumerated(-1, -1, kVideoLabel, physical_video_devices_);
  }

  int next_device_id_;
};

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_AudioAndVideoDevices) {
  // Runs through test with 1 audio and 1 video device.
  RunTest(1, 1);
}

TEST_F(DeviceRequestMessageFilterTest,
       TestGetSources_MultipleAudioAndVideoDevices) {
  // Runs through test with 3 audio devices and 2 video devices.
  RunTest(3, 2);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoVideoDevices) {
  // Runs through test with 4 audio devices and 0 video devices.
  RunTest(4, 0);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoAudioDevices) {
  // Runs through test with 0 audio devices and 3 video devices.
  RunTest(0, 3);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoDevices) {
  // Runs through test with no devices.
  RunTest(0, 0);
}

};  // namespace content
