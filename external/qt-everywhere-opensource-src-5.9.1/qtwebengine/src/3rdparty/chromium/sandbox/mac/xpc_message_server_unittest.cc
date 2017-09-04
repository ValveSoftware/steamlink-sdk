// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/xpc_message_server.h"

#include <Block.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "sandbox/mac/xpc.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

// A MessageDemuxer that manages a test server and executes a block for every
// message.
class BlockDemuxer : public MessageDemuxer {
 public:
  BlockDemuxer()
      : demux_block_(NULL),
        server_(this, MACH_PORT_NULL),
        pipe_(NULL) {
  }

  ~BlockDemuxer() override {
    if (pipe_)
      xpc_release(pipe_);
    if (demux_block_)
      Block_release(demux_block_);
  }

  // Starts running the server, given a block to handle incoming IPC messages.
  bool Initialize(void (^demux_block)(IPCMessage request)) {
    if (!server_.Initialize())
      return false;

    // Create a send right on the port so that the XPC pipe can be created.
    if (mach_port_insert_right(mach_task_self(), server_.GetServerPort(),
            server_.GetServerPort(), MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
      return false;
    }
    scoped_send_right_.reset(server_.GetServerPort());

    demux_block_ = Block_copy(demux_block);
    pipe_ = xpc_pipe_create_from_port(server_.GetServerPort(), 0);

    return true;
  }

  void DemuxMessage(IPCMessage request) override {
    demux_block_(request);
  }

  xpc_pipe_t pipe() { return pipe_; }

  XPCMessageServer* server() { return &server_; }

 private:
  void (^demux_block_)(IPCMessage request);

  XPCMessageServer server_;

  base::mac::ScopedMachSendRight scoped_send_right_;

  xpc_pipe_t pipe_;
};

TEST(XPCMessageServerTest, ReceiveMessage) {
  BlockDemuxer fixture;
  XPCMessageServer* server = fixture.server();

  uint64_t __block value = 0;
  ASSERT_TRUE(fixture.Initialize(^(IPCMessage request) {
      value = xpc_dictionary_get_uint64(request.xpc, "test_value");
      server->SendReply(server->CreateReply(request));
  }));

  xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
  xpc_dictionary_set_uint64(request, "test_value", 42);

  xpc_object_t reply;
  EXPECT_EQ(0, xpc_pipe_routine(fixture.pipe(), request, &reply));

  EXPECT_EQ(42u, value);

  xpc_release(request);
  xpc_release(reply);
}

TEST(XPCMessageServerTest, RejectMessage) {
  BlockDemuxer fixture;
  XPCMessageServer* server = fixture.server();
  ASSERT_TRUE(fixture.Initialize(^(IPCMessage request) {
      server->RejectMessage(request, EPERM);
  }));

  xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
  xpc_object_t reply;
  EXPECT_EQ(0, xpc_pipe_routine(fixture.pipe(), request, &reply));

  EXPECT_EQ(EPERM, xpc_dictionary_get_int64(reply, "error"));

  xpc_release(request);
  xpc_release(reply);
}

TEST(XPCMessageServerTest, RejectMessageSimpleRoutine) {
  BlockDemuxer fixture;
  XPCMessageServer* server = fixture.server();
  ASSERT_TRUE(fixture.Initialize(^(IPCMessage request) {
      server->RejectMessage(request, -99);
  }));

  // Create a message that is not expecting a reply.
  xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
  EXPECT_EQ(0, xpc_pipe_simpleroutine(fixture.pipe(), request));

  xpc_release(request);
}

char kGetSenderPID[] = "org.chromium.sandbox.test.GetSenderPID";

TEST(XPCMessageServerTest, GetSenderPID) {
  BlockDemuxer fixture;
  XPCMessageServer* server = fixture.server();

  pid_t __block sender_pid = 0;
  int64_t __block child_pid = 0;
  ASSERT_TRUE(fixture.Initialize(^(IPCMessage request) {
      sender_pid = server->GetMessageSenderPID(request);
      child_pid = xpc_dictionary_get_int64(request.xpc, "child_pid");
  }));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  kern_return_t kr = bootstrap_register(bootstrap_port, kGetSenderPID,
      server->GetServerPort());
#pragma GCC diagnostic pop
  ASSERT_EQ(KERN_SUCCESS, kr);

  base::Process child = base::SpawnMultiProcessTestChild(
      "GetSenderPID",
      base::GetMultiProcessTestChildBaseCommandLine(),
      base::LaunchOptions());
  ASSERT_TRUE(child.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(child.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);

  EXPECT_EQ(child.Pid(), sender_pid);
  EXPECT_EQ(child.Pid(), child_pid);
  EXPECT_EQ(sender_pid, child_pid);
}

MULTIPROCESS_TEST_MAIN(GetSenderPID) {
  mach_port_t port = MACH_PORT_NULL;
  CHECK_EQ(KERN_SUCCESS, bootstrap_look_up(bootstrap_port, kGetSenderPID,
      &port));
  base::mac::ScopedMachSendRight scoped_port(port);

  xpc_pipe_t pipe = xpc_pipe_create_from_port(port, 0);

  xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
  xpc_dictionary_set_int64(message, "child_pid", getpid());
  CHECK_EQ(0, xpc_pipe_simpleroutine(pipe, message));

  xpc_release(message);
  xpc_release(pipe);

  return 0;
}

TEST(XPCMessageServerTest, ForwardMessage) {
  BlockDemuxer first;
  XPCMessageServer* first_server = first.server();

  BlockDemuxer second;
  XPCMessageServer* second_server = second.server();

  ASSERT_TRUE(first.Initialize(^(IPCMessage request) {
      xpc_dictionary_set_int64(request.xpc, "seen_by_first", 1);
      first_server->ForwardMessage(request, second_server->GetServerPort());
  }));
  ASSERT_TRUE(second.Initialize(^(IPCMessage request) {
      IPCMessage reply = second_server->CreateReply(request);
      xpc_dictionary_set_int64(reply.xpc, "seen_by_first",
          xpc_dictionary_get_int64(request.xpc, "seen_by_first"));
      xpc_dictionary_set_int64(reply.xpc, "seen_by_second", 2);
      second_server->SendReply(reply);
  }));

  xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
  xpc_object_t reply;
  ASSERT_EQ(0, xpc_pipe_routine(first.pipe(), request, &reply));

  EXPECT_EQ(1, xpc_dictionary_get_int64(reply, "seen_by_first"));
  EXPECT_EQ(2, xpc_dictionary_get_int64(reply, "seen_by_second"));

  xpc_release(request);
  xpc_release(reply);
}

}  // namespace sandbox
