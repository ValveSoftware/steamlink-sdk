// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_tracker.h"

#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebRTCOfferOptions.h"

using ::testing::_;

namespace content {

namespace {

class MockSendTargetThread : public MockRenderThread {
 public:
  MOCK_METHOD3(OnUpdatePeerConnection, void(int, std::string, std::string));
  MOCK_METHOD1(OnAddPeerConnection, void(PeerConnectionInfo));

 private:
  bool OnMessageReceived(const IPC::Message& msg) override;
};

bool MockSendTargetThread::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockSendTargetThread, msg)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_UpdatePeerConnection,
                        OnUpdatePeerConnection)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_AddPeerConnection,
                        OnAddPeerConnection)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

class MockPeerConnectionHandler : public RTCPeerConnectionHandler {
 public:
  MockPeerConnectionHandler() : RTCPeerConnectionHandler(&client_, nullptr) {}

 private:
  MockWebRTCPeerConnectionHandlerClient client_;
};

}  // namespace

TEST(PeerConnectionTrackerTest, CreatingObject) {
  PeerConnectionTracker tracker;
}

TEST(PeerConnectionTrackerTest, TrackCreateOffer) {
  PeerConnectionTracker tracker;
  // Note: blink::WebRTCOfferOptions is not mockable. So we can't write
  // tests for anything but a null options parameter.
  blink::WebRTCOfferOptions options(0, 0, false, false);
  // Initialization stuff. This can be separated into a test class.
  MockPeerConnectionHandler pc_handler;
  MockSendTargetThread target_thread;
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  blink::WebMediaConstraints constraints;
  tracker.OverrideSendTargetForTesting(&target_thread);
  EXPECT_CALL(target_thread, OnAddPeerConnection(_));
  tracker.RegisterPeerConnection(&pc_handler, config, constraints, nullptr);
  // Back to the test.
  EXPECT_CALL(target_thread,
              OnUpdatePeerConnection(
                  _, "createOffer",
                  "options: {offerToReceiveVideo: 0, offerToReceiveAudio: 0, "
                  "voiceActivityDetection: false, iceRestart: false}"));
  tracker.TrackCreateOffer(&pc_handler, options);
}

// TODO(hta): Write tests for the other tracking functions.

}  // namespace
