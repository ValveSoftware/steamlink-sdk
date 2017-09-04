// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_output_ipc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kRenderFrameId = 2;
const char kEmptyMatchedDeviceId[] = "";

class MockAudioDelegate : public media::AudioOutputIPCDelegate {
 public:
  MockAudioDelegate() {
    Reset();
  }

  void OnError() override { error_received_ = true; }

  void OnDeviceAuthorized(media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id) override {
    device_authorized_received_ = true;
    device_status_ = device_status;
    output_params_ = output_params;
    matched_device_id_ = matched_device_id;
  }

  void OnStreamCreated(base::SharedMemoryHandle handle,
                       base::SyncSocket::Handle,
                       int length) override {
    created_received_ = true;
    handle_ = handle;
    length_ = length;
  }

  void OnIPCClosed() override {}

  void Reset() {
    error_received_ = false;

    device_authorized_received_ = false;
    output_params_ = media::AudioParameters();
    device_status_ = media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    volume_ = 0;
  }

  bool error_received() { return error_received_; }

  bool device_authorized_received() { return device_authorized_received_; }
  media::AudioParameters output_params() { return output_params_; }
  media::OutputDeviceStatus device_status() { return device_status_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  uint32_t length() { return length_; }

 private:
  bool error_received_;

  bool device_authorized_received_;
  media::AudioParameters output_params_;
  media::OutputDeviceStatus device_status_;
  std::string matched_device_id_;

  bool created_received_;
  base::SharedMemoryHandle handle_;
  int length_;

  bool volume_received_;
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDelegate);
};

media::AudioParameters MockOutputParams() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO,
                                media::AudioParameters::kAudioCDSampleRate, 16,
                                100);
}

}  // namespace

TEST(AudioMessageFilterTest, Basic) {
  base::MessageLoopForIO message_loop;

  scoped_refptr<AudioMessageFilter> filter(
      new AudioMessageFilter(message_loop.task_runner()));

  MockAudioDelegate delegate;
  const std::unique_ptr<media::AudioOutputIPC> ipc =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  static const int kSessionId = 0;
  static const std::string kDeviceId;
  static const url::Origin kSecurityOrigin;
  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId,
                                  kSecurityOrigin);
  ipc->CreateStream(&delegate, media::AudioParameters());
  static const int kStreamId = 1;
  EXPECT_EQ(&delegate, filter->delegates_.Lookup(kStreamId));

  // AudioMsg_NotifyDeviceAuthorized
  EXPECT_FALSE(delegate.device_authorized_received());
  filter->OnMessageReceived(AudioMsg_NotifyDeviceAuthorized(
      kStreamId, media::OUTPUT_DEVICE_STATUS_OK, MockOutputParams(),
      kEmptyMatchedDeviceId));
  EXPECT_TRUE(delegate.device_authorized_received());
  EXPECT_TRUE(delegate.output_params().Equals(MockOutputParams()));
  delegate.Reset();

  // AudioMsg_NotifyStreamCreated
  base::SyncSocket::TransitDescriptor socket_descriptor;
  const uint32_t kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(AudioMsg_NotifyStreamCreated(
      kStreamId, base::SharedMemory::NULLHandle(), socket_descriptor, kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // AudioMsg_NotifyStreamStateChanged
  EXPECT_FALSE(delegate.error_received());
  filter->OnMessageReceived(AudioMsg_NotifyStreamError(kStreamId));
  EXPECT_TRUE(delegate.error_received());
  delegate.Reset();

  ipc->CloseStream();
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId));
}

TEST(AudioMessageFilterTest, Delegates) {
  base::MessageLoopForIO message_loop;

  scoped_refptr<AudioMessageFilter> filter(
      new AudioMessageFilter(message_loop.task_runner()));

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;
  const std::unique_ptr<media::AudioOutputIPC> ipc1 =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  const std::unique_ptr<media::AudioOutputIPC> ipc2 =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  ipc1->CreateStream(&delegate1, media::AudioParameters());
  ipc2->CreateStream(&delegate2, media::AudioParameters());
  static const int kStreamId1 = 1;
  static const int kStreamId2 = 2;
  EXPECT_EQ(&delegate1, filter->delegates_.Lookup(kStreamId1));
  EXPECT_EQ(&delegate2, filter->delegates_.Lookup(kStreamId2));

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.error_received());
  EXPECT_FALSE(delegate2.error_received());
  filter->OnMessageReceived(AudioMsg_NotifyStreamError(kStreamId1));
  EXPECT_TRUE(delegate1.error_received());
  EXPECT_FALSE(delegate2.error_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.error_received());
  EXPECT_FALSE(delegate2.error_received());
  filter->OnMessageReceived(AudioMsg_NotifyStreamError(kStreamId2));
  EXPECT_FALSE(delegate1.error_received());
  EXPECT_TRUE(delegate2.error_received());
  delegate2.Reset();

  base::RunLoop().RunUntilIdle();

  ipc1->CloseStream();
  ipc2->CloseStream();
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId1));
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId2));
}

}  // namespace content
