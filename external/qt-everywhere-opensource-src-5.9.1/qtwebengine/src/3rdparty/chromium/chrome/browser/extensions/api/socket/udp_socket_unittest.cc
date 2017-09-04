// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// UDPSocketUnitTest exists solely to make it easier to pass a specific
// gtest_filter argument during development.
class UDPSocketUnitTest : public BrowserWithTestWindowTest {
};

static void OnConnected(int result) {
  EXPECT_EQ(0, result);
}

static void OnCompleted(int bytes_read,
                        scoped_refptr<net::IOBuffer> io_buffer,
                        bool socket_destroying,
                        const std::string& address,
                        uint16_t port) {
  // Do nothing; don't care.
}

static const char test_message[] = "$$TESTMESSAGETESTMESSAGETESTMESSAGETEST$$";
static const int test_message_length = arraysize(test_message);

net::AddressList CreateAddressList(const char* address_string, int port) {
  net::IPAddress ip;
  EXPECT_TRUE(ip.AssignFromIPLiteral(address_string));
  return net::AddressList::CreateFromIPAddress(ip, port);
}

static void OnSendCompleted(int result) {
  EXPECT_EQ(test_message_length, result);
}

TEST(UDPSocketUnitTest, TestUDPSocketRecvFrom) {
  base::MessageLoopForIO io_loop;  // For RecvFrom to do its threaded work.
  UDPSocket socket("abcdefghijklmnopqrst");

  // Confirm that we can call two RecvFroms in quick succession without
  // triggering crbug.com/146606.
  socket.Connect(CreateAddressList("127.0.0.1", 40000),
                 base::Bind(&OnConnected));
  socket.RecvFrom(4096, base::Bind(&OnCompleted));
  socket.RecvFrom(4096, base::Bind(&OnCompleted));
}

TEST(UDPSocketUnitTest, TestUDPMulticastJoinGroup) {
  const char kGroup[] = "237.132.100.17";
  UDPSocket src("abcdefghijklmnopqrst");
  UDPSocket dest("abcdefghijklmnopqrst");

  EXPECT_EQ(0, dest.Bind("0.0.0.0", 13333));
  EXPECT_EQ(0, dest.JoinGroup(kGroup));
  std::vector<std::string> groups = dest.GetJoinedGroups();
  EXPECT_EQ(static_cast<size_t>(1), groups.size());
  EXPECT_EQ(kGroup, *groups.begin());
  EXPECT_NE(0, dest.LeaveGroup("237.132.100.13"));
  EXPECT_EQ(0, dest.LeaveGroup(kGroup));
  groups = dest.GetJoinedGroups();
  EXPECT_EQ(static_cast<size_t>(0), groups.size());
}

TEST(UDPSocketUnitTest, TestUDPMulticastTimeToLive) {
  const char kGroup[] = "237.132.100.17";
  UDPSocket socket("abcdefghijklmnopqrst");
  EXPECT_NE(0, socket.SetMulticastTimeToLive(-1));  // Negative TTL shall fail.
  EXPECT_EQ(0, socket.SetMulticastTimeToLive(3));
  socket.Connect(CreateAddressList(kGroup, 13333), base::Bind(&OnConnected));
}

TEST(UDPSocketUnitTest, TestUDPMulticastLoopbackMode) {
  const char kGroup[] = "237.132.100.17";
  UDPSocket socket("abcdefghijklmnopqrst");
  EXPECT_EQ(0, socket.SetMulticastLoopbackMode(false));
  socket.Connect(CreateAddressList(kGroup, 13333), base::Bind(&OnConnected));
}

// Send a test multicast packet every second.
// Once the target socket received the packet, the message loop will exit.
static void SendMulticastPacket(const base::Closure& quit_run_loop,
                                UDPSocket* src,
                                int result) {
  if (result == 0) {
    scoped_refptr<net::IOBuffer> data = new net::WrappedIOBuffer(test_message);
    src->Write(data, test_message_length, base::Bind(&OnSendCompleted));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&SendMulticastPacket, quit_run_loop, src, result),
        base::TimeDelta::FromSeconds(1));
  } else {
    quit_run_loop.Run();
    FAIL() << "Failed to connect to multicast address. Error code: " << result;
  }
}

static void OnMulticastReadCompleted(const base::Closure& quit_run_loop,
                                     bool* packet_received,
                                     int count,
                                     scoped_refptr<net::IOBuffer> io_buffer,
                                     bool socket_destroying) {
  EXPECT_EQ(test_message_length, count);
  EXPECT_EQ(0, strncmp(io_buffer->data(), test_message, test_message_length));
  *packet_received = true;
  quit_run_loop.Run();
}

TEST(UDPSocketUnitTest, TestUDPMulticastRecv) {
  const int kPort = 9999;
  const char kGroup[] = "237.132.100.17";
  bool packet_received = false;
  base::MessageLoopForIO io_loop;  // For Read to do its threaded work.
  UDPSocket dest("abcdefghijklmnopqrst");
  UDPSocket src("abcdefghijklmnopqrst");

  base::RunLoop run_loop;

  // Receiver
  EXPECT_EQ(0, dest.Bind("0.0.0.0", kPort));
  EXPECT_EQ(0, dest.JoinGroup(kGroup));
  dest.Read(1024, base::Bind(&OnMulticastReadCompleted, run_loop.QuitClosure(),
                             &packet_received));

  // Sender
  EXPECT_EQ(0, src.SetMulticastTimeToLive(0));
  src.Connect(CreateAddressList(kGroup, kPort),
              base::Bind(&SendMulticastPacket, run_loop.QuitClosure(), &src));

  // If not received within the test action timeout, quit the message loop.
  io_loop.task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                         TestTimeouts::action_timeout());

  run_loop.Run();

  EXPECT_TRUE(packet_received) << "Failed to receive from multicast address";
}

}  // namespace extensions
