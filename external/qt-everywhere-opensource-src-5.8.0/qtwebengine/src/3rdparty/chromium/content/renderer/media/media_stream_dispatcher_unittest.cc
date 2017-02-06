// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const int kRouteId = 0;
const int kAudioSessionId = 3;
const int kVideoSessionId = 5;
const int kRequestId1 = 10;
const int kRequestId2 = 20;
const int kRequestId3 = 30;
const int kRequestId4 = 40;

const MediaStreamType kAudioType = MEDIA_DEVICE_AUDIO_CAPTURE;
const MediaStreamType kVideoType = MEDIA_DEVICE_VIDEO_CAPTURE;

class MockMediaStreamDispatcherEventHandler
    : public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MockMediaStreamDispatcherEventHandler> {
 public:
  MockMediaStreamDispatcherEventHandler()
      : request_id_(-1), did_receive_devices_changed_(false) {}

  void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_device_array,
      const StreamDeviceInfoArray& video_device_array) override {
    request_id_ = request_id;
    label_ = label;
    if (audio_device_array.size()) {
      DCHECK(audio_device_array.size() == 1);
      audio_device_ = audio_device_array[0];
    }
    if (video_device_array.size()) {
      DCHECK(video_device_array.size() == 1);
      video_device_ = video_device_array[0];
    }
  }

  void OnStreamGenerationFailed(
      int request_id,
      content::MediaStreamRequestResult result) override {
    request_id_ = request_id;
  }

  void OnDeviceStopped(const std::string& label,
                       const StreamDeviceInfo& device_info) override {
    device_stopped_label_ = label;
    if (IsVideoMediaType(device_info.device.type)) {
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(video_device_, device_info));
    }
    if (IsAudioInputMediaType(device_info.device.type)) {
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(audio_device_, device_info));
    }
  }

  void OnDevicesEnumerated(int request_id,
                           const StreamDeviceInfoArray& device_array) override {
    request_id_ = request_id;
  }

  void OnDeviceOpened(int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& video_device) override {
    request_id_ = request_id;
    label_ = label;
  }

  void OnDeviceOpenFailed(int request_id) override { request_id_ = request_id; }

  void OnDevicesChanged() override { did_receive_devices_changed_ = true; }

  void ResetStoredParameters() {
    request_id_ = -1;
    label_ = "";
    device_stopped_label_ = "";
    audio_device_ = StreamDeviceInfo();
    video_device_ = StreamDeviceInfo();
  }

  int request_id_;
  std::string label_;
  std::string device_stopped_label_;
  StreamDeviceInfo audio_device_;
  StreamDeviceInfo video_device_;
  bool did_receive_devices_changed_;
};

class MediaStreamDispatcherUnderTest : public MediaStreamDispatcher {
 public:
  MediaStreamDispatcherUnderTest() : MediaStreamDispatcher(NULL) {}

  using MediaStreamDispatcher::GetNextIpcIdForTest;
  using RenderFrameObserver::OnMessageReceived;
};

class MediaStreamDispatcherTest : public ::testing::Test {
 public:
  MediaStreamDispatcherTest()
      : dispatcher_(new MediaStreamDispatcherUnderTest()),
        handler_(new MockMediaStreamDispatcherEventHandler),
        security_origin_(GURL("http://test.com")) {}

  // Generates a request for a MediaStream and returns the request id that is
  // used in IPC. Use this returned id in CompleteGenerateStream to identify
  // the request.
  int GenerateStream(const StreamControls& controls, int request_id) {
    int next_ipc_id = dispatcher_->GetNextIpcIdForTest();
    dispatcher_->GenerateStream(request_id, handler_.get()->AsWeakPtr(),
                                controls, security_origin_);
    return next_ipc_id;
  }

  // CompleteGenerateStream create a MediaStreamMsg_StreamGenerated instance
  // and call the MediaStreamDispathcer::OnMessageReceived. |ipc_id| must be the
  // the id returned by GenerateStream.
  std::string CompleteGenerateStream(int ipc_id,
                                     const StreamControls& controls,
                                     int request_id) {
    StreamDeviceInfoArray audio_device_array(controls.audio.requested ? 1 : 0);
    if (controls.audio.requested) {
      StreamDeviceInfo audio_device_info;
      audio_device_info.device.name = "Microphone";
      audio_device_info.device.type = kAudioType;
      audio_device_info.session_id = kAudioSessionId;
      audio_device_array[0] = audio_device_info;
    }

    StreamDeviceInfoArray video_device_array(controls.video.requested ? 1 : 0);
    if (controls.video.requested) {
      StreamDeviceInfo video_device_info;
      video_device_info.device.name = "Camera";
      video_device_info.device.type = kVideoType;
      video_device_info.session_id = kVideoSessionId;
      video_device_array[0] = video_device_info;
    }

    std::string label = "stream" + base::IntToString(ipc_id);

    handler_->ResetStoredParameters();
    dispatcher_->OnMessageReceived(MediaStreamMsg_StreamGenerated(
        kRouteId, ipc_id, label,
        audio_device_array, video_device_array));

    EXPECT_EQ(handler_->request_id_, request_id);
    EXPECT_EQ(handler_->label_, label);

    if (controls.audio.requested)
      EXPECT_EQ(dispatcher_->audio_session_id(label, 0), kAudioSessionId);

    if (controls.video.requested)
      EXPECT_EQ(dispatcher_->video_session_id(label, 0), kVideoSessionId);

    return label;
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<MediaStreamDispatcherUnderTest> dispatcher_;
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler_;
  url::Origin security_origin_;
};

}  // namespace

TEST_F(MediaStreamDispatcherTest, GenerateStreamAndStopDevices) {
  StreamControls controls(true, true);

  int ipc_request_id1 = GenerateStream(controls, kRequestId1);
  int ipc_request_id2 = GenerateStream(controls, kRequestId2);
  EXPECT_NE(ipc_request_id1, ipc_request_id2);

  // Complete the creation of stream1.
  const std::string& label1 =
      CompleteGenerateStream(ipc_request_id1, controls, kRequestId1);

  // Complete the creation of stream2.
  const std::string& label2 =
      CompleteGenerateStream(ipc_request_id2, controls, kRequestId2);

  // Stop the actual audio device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->audio_device_);
  EXPECT_EQ(dispatcher_->audio_session_id(label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher_->audio_session_id(label2, 0),
            StreamDeviceInfo::kNoId);

  // Stop the actual video device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->video_device_);
  EXPECT_EQ(dispatcher_->video_session_id(label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher_->video_session_id(label2, 0),
            StreamDeviceInfo::kNoId);
}

TEST_F(MediaStreamDispatcherTest, BasicVideoDevice) {
  std::unique_ptr<MediaStreamDispatcher> dispatcher(
      new MediaStreamDispatcher(NULL));
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler1(
      new MockMediaStreamDispatcherEventHandler);
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler2(
      new MockMediaStreamDispatcherEventHandler);
  url::Origin security_origin;

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->EnumerateDevices(
      kRequestId1, handler1.get()->AsWeakPtr(),
      kVideoType,
      security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->EnumerateDevices(
      kRequestId2, handler2.get()->AsWeakPtr(),
      kVideoType,
      security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  StreamDeviceInfoArray video_device_array(1);
  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.id = "device_path";
  video_device_info.device.type = kVideoType;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the first enumeration request.
  dispatcher->OnMessageReceived(MediaStreamMsg_DevicesEnumerated(
      kRouteId, ipc_request_id1, video_device_array));
  EXPECT_EQ(handler1->request_id_, kRequestId1);

  dispatcher->OnMessageReceived(MediaStreamMsg_DevicesEnumerated(
        kRouteId, ipc_request_id2, video_device_array));
  EXPECT_EQ(handler2->request_id_, kRequestId2);

  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  int ipc_request_id3 = dispatcher->next_ipc_id_;
  dispatcher->OpenDevice(kRequestId3, handler1.get()->AsWeakPtr(),
                         video_device_info.device.id,
                         kVideoType,
                         security_origin);
  int ipc_request_id4 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id3, ipc_request_id4);
  dispatcher->OpenDevice(kRequestId4, handler1.get()->AsWeakPtr(),
                         video_device_info.device.id,
                         kVideoType,
                         security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(4));

  // Complete the OpenDevice of request 1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnMessageReceived(MediaStreamMsg_DeviceOpened(
      kRouteId, ipc_request_id3, stream_label1, video_device_info));
  EXPECT_EQ(handler1->request_id_, kRequestId3);

  // Complete the OpenDevice of request 2.
  std::string stream_label2 = std::string("stream2");
  dispatcher->OnMessageReceived(MediaStreamMsg_DeviceOpened(
      kRouteId, ipc_request_id4, stream_label2, video_device_info));
  EXPECT_EQ(handler1->request_id_, kRequestId4);

  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(2));

  // Check the video_session_id.
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0), kVideoSessionId);

  // Close the device from request 2.
  dispatcher->CloseDevice(stream_label2);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0),
            StreamDeviceInfo::kNoId);

  // Close the device from request 1.
  dispatcher->CloseDevice(stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));
}

TEST_F(MediaStreamDispatcherTest, TestFailure) {
  std::unique_ptr<MediaStreamDispatcher> dispatcher(
      new MediaStreamDispatcher(NULL));
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler(
      new MockMediaStreamDispatcherEventHandler);
  StreamControls components(true, true);
  url::Origin security_origin;

  // Test failure when creating a stream.
  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin);
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerationFailed(
      kRouteId, ipc_request_id1, MEDIA_DEVICE_PERMISSION_DENIED));

  // Verify that the request have been completed.
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));

  // Create a new stream.
  ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin);

  StreamDeviceInfoArray audio_device_array(1);
  StreamDeviceInfo audio_device_info;
  audio_device_info.device.name = "Microphone";
  audio_device_info.device.type = kAudioType;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  StreamDeviceInfoArray video_device_array(1);
  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.type = kVideoType;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the creation of stream1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
      kRouteId, ipc_request_id1, stream_label1,
      audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);
}

TEST_F(MediaStreamDispatcherTest, CancelGenerateStream) {
  std::unique_ptr<MediaStreamDispatcher> dispatcher(
      new MediaStreamDispatcher(NULL));
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler(
      new MockMediaStreamDispatcherEventHandler);
  StreamControls components(true, true);
  int ipc_request_id1 = dispatcher->next_ipc_id_;

  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, url::Origin());
  dispatcher->GenerateStream(kRequestId2, handler.get()->AsWeakPtr(),
                             components, url::Origin());

  EXPECT_EQ(2u, dispatcher->requests_.size());
  dispatcher->CancelGenerateStream(kRequestId2, handler.get()->AsWeakPtr());
  EXPECT_EQ(1u, dispatcher->requests_.size());

  // Complete the creation of stream1.
  StreamDeviceInfo audio_device_info;
  audio_device_info.device.name = "Microphone";
  audio_device_info.device.type = kAudioType;
  audio_device_info.session_id = kAudioSessionId;
  StreamDeviceInfoArray audio_device_array(1);
  audio_device_array[0] = audio_device_info;

  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.type = kVideoType;
  video_device_info.session_id = kVideoSessionId;
  StreamDeviceInfoArray video_device_array(1);
  video_device_array[0] = video_device_info;

  std::string stream_label1 = "stream1";
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
      kRouteId, ipc_request_id1, stream_label1,
      audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(0u, dispatcher->requests_.size());
}

// Test that the MediaStreamDispatcherEventHandler is notified when the message
// MediaStreamMsg_DeviceStopped is received.
TEST_F(MediaStreamDispatcherTest, DeviceClosed) {
  StreamControls controls(true, true);

  int ipc_request_id = GenerateStream(controls, kRequestId1);
  const std::string& label =
      CompleteGenerateStream(ipc_request_id, controls, kRequestId1);

  dispatcher_->OnMessageReceived(
      MediaStreamMsg_DeviceStopped(kRouteId, label, handler_->video_device_));
  // Verify that MediaStreamDispatcherEventHandler::OnDeviceStopped has been
  // called.
  EXPECT_EQ(label, handler_->device_stopped_label_);
  EXPECT_EQ(dispatcher_->video_session_id(label, 0),
            StreamDeviceInfo::kNoId);
}

// Test that the MediaStreamDispatcherEventHandler is notified when the message
// MediaStreamMsg_DevicesChanged is received.
TEST_F(MediaStreamDispatcherTest, DevicesChanged) {
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler1(
      new MockMediaStreamDispatcherEventHandler);
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler2(
      new MockMediaStreamDispatcherEventHandler);
  dispatcher_->SubscribeToDeviceChangeNotifications(handler1->AsWeakPtr(),
                                                    security_origin_);
  dispatcher_->SubscribeToDeviceChangeNotifications(handler2->AsWeakPtr(),
                                                    security_origin_);
  dispatcher_->OnMessageReceived(MediaStreamMsg_DevicesChanged(kRouteId));
  dispatcher_->CancelDeviceChangeNotifications(handler1->AsWeakPtr());
  dispatcher_->CancelDeviceChangeNotifications(handler2->AsWeakPtr());

  EXPECT_TRUE(handler1->did_receive_devices_changed_);
  EXPECT_TRUE(handler2->did_receive_devices_changed_);
}

}  // namespace content
