// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_eventlog_host.h"

#include <tuple>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

namespace {

// Get the expected Rtc eventlog file name. The name will be
// <temporary path>.<render process id>.<peer connection local id>
base::FilePath GetExpectedEventLogFileName(const base::FilePath& base_file,
                                           int render_process_id,
                                           int peer_connection_local_id) {
  return base_file.AddExtension(IntToStringType(render_process_id))
      .AddExtension(IntToStringType(peer_connection_local_id));
}

}  // namespace

class WebRtcEventlogHostTest : public testing::Test {
 public:
  WebRtcEventlogHostTest()
      : mock_render_process_host_(static_cast<MockRenderProcessHost*>(
            mock_render_process_factory_.CreateRenderProcessHost(
                &test_browser_context_,
                nullptr))),
        render_id_(mock_render_process_host_->GetID()),
        event_log_host_(render_id_) {}
  TestBrowserThreadBundle thread_bundle_;
  MockRenderProcessHostFactory mock_render_process_factory_;
  TestBrowserContext test_browser_context_;
  std::unique_ptr<MockRenderProcessHost> mock_render_process_host_;
  const int render_id_;
  WebRTCEventLogHost event_log_host_;
  base::FilePath base_file_;

  void StartLogging() {
    ASSERT_TRUE(base::CreateTemporaryFile(&base_file_));
    EXPECT_TRUE(base::DeleteFile(base_file_, false));
    EXPECT_FALSE(base::PathExists(base_file_));
    EXPECT_TRUE(event_log_host_.StartWebRTCEventLog(base_file_));
    base::RunLoop().RunUntilIdle();
  }

  void StopLogging() {
    EXPECT_TRUE(event_log_host_.StopWebRTCEventLog());
    base::RunLoop().RunUntilIdle();
  }

  void ValidateStartIPCMessageAndCloseFile(const IPC::Message* msg,
                                           const int peer_connection_id) {
    ASSERT_TRUE(msg);
    std::tuple<int, IPC::PlatformFileForTransit> start_params;
    PeerConnectionTracker_StartEventLog::Read(msg, &start_params);
    EXPECT_EQ(peer_connection_id, std::get<0>(start_params));
    ASSERT_NE(IPC::InvalidPlatformFileForTransit(), std::get<1>(start_params));
    IPC::PlatformFileForTransitToFile(std::get<1>(start_params)).Close();
  }

  void ValidateStopIPCMessage(const IPC::Message* msg,
                              const int peer_connection_id) {
    ASSERT_TRUE(msg);
    std::tuple<int> stop_params;
    PeerConnectionTracker_StopEventLog::Read(msg, &stop_params);
    EXPECT_EQ(peer_connection_id, std::get<0>(stop_params));
  }
};

// This test calls StartWebRTCEventLog() and StopWebRTCEventLog() without having
// added any PeerConnections. It is expected that no IPC messages will be sent.
TEST_F(WebRtcEventlogHostTest, NoPeerConnectionTest) {
  mock_render_process_host_->sink().ClearMessages();

  // Start logging and check that no IPC messages were sent.
  StartLogging();
  EXPECT_EQ(size_t(0), mock_render_process_host_->sink().message_count());

  // Stop logging and check that no IPC messages were sent.
  StopLogging();
  EXPECT_EQ(size_t(0), mock_render_process_host_->sink().message_count());
}

// This test calls StartWebRTCEventLog() and StopWebRTCEventLog() after adding a
// single PeerConnection. It is expected that one IPC message will be sent for
// each of the Start and Stop calls, and that a logfile is created.
TEST_F(WebRtcEventlogHostTest, OnePeerConnectionTest) {
  const int kTestPeerConnectionId = 123;
  mock_render_process_host_->sink().ClearMessages();

  // Add a PeerConnection and start logging.
  event_log_host_.PeerConnectionAdded(kTestPeerConnectionId);
  StartLogging();

  // Check that the correct IPC message was sent.
  EXPECT_EQ(size_t(1), mock_render_process_host_->sink().message_count());
  const IPC::Message* start_msg =
      mock_render_process_host_->sink().GetMessageAt(0);
  ValidateStartIPCMessageAndCloseFile(start_msg, kTestPeerConnectionId);

  // Stop logging.
  mock_render_process_host_->sink().ClearMessages();
  StopLogging();

  // Check that the correct IPC message was sent.
  EXPECT_EQ(size_t(1), mock_render_process_host_->sink().message_count());
  const IPC::Message* stop_msg =
      mock_render_process_host_->sink().GetMessageAt(0);
  ValidateStopIPCMessage(stop_msg, kTestPeerConnectionId);

  // Clean up the logfile.
  base::FilePath expected_file = GetExpectedEventLogFileName(
      base_file_, render_id_, kTestPeerConnectionId);
  ASSERT_TRUE(base::PathExists(expected_file));
  EXPECT_TRUE(base::DeleteFile(expected_file, false));
}

// This test calls StartWebRTCEventLog() and StopWebRTCEventLog() after adding
// two PeerConnections. It is expected that two IPC messages will be sent for
// each of the Start and Stop calls, and that a file is created for both
// PeerConnections.
TEST_F(WebRtcEventlogHostTest, TwoPeerConnectionsTest) {
  const int kTestPeerConnectionId1 = 123;
  const int kTestPeerConnectionId2 = 321;
  mock_render_process_host_->sink().ClearMessages();

  // Add two PeerConnections and start logging.
  event_log_host_.PeerConnectionAdded(kTestPeerConnectionId1);
  event_log_host_.PeerConnectionAdded(kTestPeerConnectionId2);
  StartLogging();

  // Check that the correct IPC messages were sent.
  EXPECT_EQ(size_t(2), mock_render_process_host_->sink().message_count());
  const IPC::Message* start_msg1 =
      mock_render_process_host_->sink().GetMessageAt(0);
  ValidateStartIPCMessageAndCloseFile(start_msg1, kTestPeerConnectionId1);
  const IPC::Message* start_msg2 =
      mock_render_process_host_->sink().GetMessageAt(1);
  ValidateStartIPCMessageAndCloseFile(start_msg2, kTestPeerConnectionId2);

  // Stop logging.
  mock_render_process_host_->sink().ClearMessages();
  StopLogging();

  // Check that the correct IPC messages were sent.
  EXPECT_EQ(size_t(2), mock_render_process_host_->sink().message_count());
  const IPC::Message* stop_msg1 =
      mock_render_process_host_->sink().GetMessageAt(0);
  ValidateStopIPCMessage(stop_msg1, kTestPeerConnectionId1);
  const IPC::Message* stop_msg2 =
      mock_render_process_host_->sink().GetMessageAt(1);
  ValidateStopIPCMessage(stop_msg2, kTestPeerConnectionId2);

  // Clean up the logfiles.
  base::FilePath expected_file1 = GetExpectedEventLogFileName(
      base_file_, render_id_, kTestPeerConnectionId1);
  base::FilePath expected_file2 = GetExpectedEventLogFileName(
      base_file_, render_id_, kTestPeerConnectionId2);
  ASSERT_TRUE(base::PathExists(expected_file1));
  EXPECT_TRUE(base::DeleteFile(expected_file1, false));
  ASSERT_TRUE(base::PathExists(expected_file2));
  EXPECT_TRUE(base::DeleteFile(expected_file2, false));
}

// This test calls StartWebRTCEventLog() and StopWebRTCEventLog() after adding
// more PeerConnections than the maximum allowed. It is expected that only the
// maximum allowed number of IPC messages and log files will be opened, but we
// expect the number of stop IPC messages to be equal to the actual number of
// PeerConnections.
TEST_F(WebRtcEventlogHostTest, ExceedMaxPeerConnectionsTest) {
#if defined(OS_ANDROID)
  const int kMaxNumberLogFiles = 3;
#else
  const int kMaxNumberLogFiles = 5;
#endif
  const int kNumberOfPeerConnections = kMaxNumberLogFiles + 1;
  mock_render_process_host_->sink().ClearMessages();

  // Add the maximum number + 1 PeerConnections and start logging.
  for (int i = 0; i < kNumberOfPeerConnections; ++i)
    event_log_host_.PeerConnectionAdded(i);
  StartLogging();

  // Check that the correct IPC messages were sent.
  ASSERT_EQ(size_t(kMaxNumberLogFiles),
            mock_render_process_host_->sink().message_count());
  for (int i = 0; i < kMaxNumberLogFiles; ++i) {
    const IPC::Message* start_msg =
        mock_render_process_host_->sink().GetMessageAt(i);
    ValidateStartIPCMessageAndCloseFile(start_msg, i);
  }

  // Stop logging.
  mock_render_process_host_->sink().ClearMessages();
  StopLogging();

  // Check that the correct IPC messages were sent.
  ASSERT_EQ(size_t(kNumberOfPeerConnections),
            mock_render_process_host_->sink().message_count());
  for (int i = 0; i < kNumberOfPeerConnections; ++i) {
    const IPC::Message* stop_msg =
        mock_render_process_host_->sink().GetMessageAt(i);
    ValidateStopIPCMessage(stop_msg, i);
  }

  // Clean up the logfiles.
  for (int i = 0; i < kMaxNumberLogFiles; ++i) {
    base::FilePath expected_file =
        GetExpectedEventLogFileName(base_file_, render_id_, i);
    ASSERT_TRUE(base::PathExists(expected_file));
    EXPECT_TRUE(base::DeleteFile(expected_file, false));
  }

  // Check that not too many files were created.
  for (int i = kMaxNumberLogFiles; i < kNumberOfPeerConnections; ++i) {
    base::FilePath expected_file =
        GetExpectedEventLogFileName(base_file_, render_id_, i);
    EXPECT_FALSE(base::PathExists(expected_file));
  }
}

// This test calls StartWebRTCEventLog() and StopWebRTCEventLog() after first
// adding and then removing a single PeerConnection. It is expected that no IPC
// message will be sent.
TEST_F(WebRtcEventlogHostTest, AddRemovePeerConnectionTest) {
  const int kTestPeerConnectionId = 123;
  mock_render_process_host_->sink().ClearMessages();

  // Add and immediately remove a PeerConnection.
  event_log_host_.PeerConnectionAdded(kTestPeerConnectionId);
  event_log_host_.PeerConnectionRemoved(kTestPeerConnectionId);

  // Start logging and check that no IPC messages were sent.
  StartLogging();
  EXPECT_EQ(size_t(0), mock_render_process_host_->sink().message_count());

  // Stop logging and check that no IPC messages were sent.
  StopLogging();
  EXPECT_EQ(size_t(0), mock_render_process_host_->sink().message_count());

  // Check that no logfile was created.
  base::FilePath expected_file = GetExpectedEventLogFileName(
      base_file_, render_id_, kTestPeerConnectionId);
  ASSERT_FALSE(base::PathExists(expected_file));
}

}  // namespace content
