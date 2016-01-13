// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_server_socket.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_unittest.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/test/net_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class UDPSocketTest : public PlatformTest {
 public:
  UDPSocketTest()
      : buffer_(new IOBufferWithSize(kMaxRead)) {
  }

  // Blocks until data is read from the socket.
  std::string RecvFromSocket(UDPServerSocket* socket) {
    TestCompletionCallback callback;

    int rv = socket->RecvFrom(
        buffer_.get(), kMaxRead, &recv_from_address_, callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return std::string();  // error!
    return std::string(buffer_->data(), rv);
  }

  // Loop until |msg| has been written to the socket or until an
  // error occurs.
  // If |address| is specified, then it is used for the destination
  // to send to. Otherwise, will send to the last socket this server
  // received from.
  int SendToSocket(UDPServerSocket* socket, std::string msg) {
    return SendToSocket(socket, msg, recv_from_address_);
  }

  int SendToSocket(UDPServerSocket* socket,
                   std::string msg,
                   const IPEndPoint& address) {
    TestCompletionCallback callback;

    int length = msg.length();
    scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(msg));
    scoped_refptr<DrainableIOBuffer> buffer(
        new DrainableIOBuffer(io_buffer.get(), length));

    int bytes_sent = 0;
    while (buffer->BytesRemaining()) {
      int rv = socket->SendTo(
          buffer.get(), buffer->BytesRemaining(), address, callback.callback());
      if (rv == ERR_IO_PENDING)
        rv = callback.WaitForResult();
      if (rv <= 0)
        return bytes_sent > 0 ? bytes_sent : rv;
      bytes_sent += rv;
      buffer->DidConsume(rv);
    }
    return bytes_sent;
  }

  std::string ReadSocket(UDPClientSocket* socket) {
    TestCompletionCallback callback;

    int rv = socket->Read(buffer_.get(), kMaxRead, callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return std::string();  // error!
    return std::string(buffer_->data(), rv);
  }

  // Loop until |msg| has been written to the socket or until an
  // error occurs.
  int WriteSocket(UDPClientSocket* socket, std::string msg) {
    TestCompletionCallback callback;

    int length = msg.length();
    scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(msg));
    scoped_refptr<DrainableIOBuffer> buffer(
        new DrainableIOBuffer(io_buffer.get(), length));

    int bytes_sent = 0;
    while (buffer->BytesRemaining()) {
      int rv = socket->Write(
          buffer.get(), buffer->BytesRemaining(), callback.callback());
      if (rv == ERR_IO_PENDING)
        rv = callback.WaitForResult();
      if (rv <= 0)
        return bytes_sent > 0 ? bytes_sent : rv;
      bytes_sent += rv;
      buffer->DidConsume(rv);
    }
    return bytes_sent;
  }

 protected:
  static const int kMaxRead = 1024;
  scoped_refptr<IOBufferWithSize> buffer_;
  IPEndPoint recv_from_address_;
};

// Creates and address from an ip/port and returns it in |address|.
void CreateUDPAddress(std::string ip_str, int port, IPEndPoint* address) {
  IPAddressNumber ip_number;
  bool rv = ParseIPLiteralToNumber(ip_str, &ip_number);
  if (!rv)
    return;
  *address = IPEndPoint(ip_number, port);
}

TEST_F(UDPSocketTest, Connect) {
  const int kPort = 9999;
  std::string simple_message("hello world!");

  // Setup the server to listen.
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", kPort, &bind_address);
  CapturingNetLog server_log;
  scoped_ptr<UDPServerSocket> server(
      new UDPServerSocket(&server_log, NetLog::Source()));
  server->AllowAddressReuse();
  int rv = server->Listen(bind_address);
  ASSERT_EQ(OK, rv);

  // Setup the client.
  IPEndPoint server_address;
  CreateUDPAddress("127.0.0.1", kPort, &server_address);
  CapturingNetLog client_log;
  scoped_ptr<UDPClientSocket> client(
      new UDPClientSocket(DatagramSocket::DEFAULT_BIND,
                          RandIntCallback(),
                          &client_log,
                          NetLog::Source()));
  rv = client->Connect(server_address);
  EXPECT_EQ(OK, rv);

  // Client sends to the server.
  rv = WriteSocket(client.get(), simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Server waits for message.
  std::string str = RecvFromSocket(server.get());
  DCHECK(simple_message == str);

  // Server echoes reply.
  rv = SendToSocket(server.get(), simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Client waits for response.
  str = ReadSocket(client.get());
  DCHECK(simple_message == str);

  // Delete sockets so they log their final events.
  server.reset();
  client.reset();

  // Check the server's log.
  CapturingNetLog::CapturedEntryList server_entries;
  server_log.GetEntries(&server_entries);
  EXPECT_EQ(4u, server_entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      server_entries, 0, NetLog::TYPE_SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(
      server_entries, 1, NetLog::TYPE_UDP_BYTES_RECEIVED, NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEvent(
      server_entries, 2, NetLog::TYPE_UDP_BYTES_SENT, NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      server_entries, 3, NetLog::TYPE_SOCKET_ALIVE));

  // Check the client's log.
  CapturingNetLog::CapturedEntryList client_entries;
  client_log.GetEntries(&client_entries);
  EXPECT_EQ(6u, client_entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      client_entries, 0, NetLog::TYPE_SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsBeginEvent(
      client_entries, 1, NetLog::TYPE_UDP_CONNECT));
  EXPECT_TRUE(LogContainsEndEvent(
      client_entries, 2, NetLog::TYPE_UDP_CONNECT));
  EXPECT_TRUE(LogContainsEvent(
      client_entries, 3, NetLog::TYPE_UDP_BYTES_SENT, NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEvent(
      client_entries, 4, NetLog::TYPE_UDP_BYTES_RECEIVED, NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      client_entries, 5, NetLog::TYPE_SOCKET_ALIVE));
}

#if defined(OS_MACOSX)
// UDPSocketPrivate_Broadcast is disabled for OSX because it requires
// root permissions on OSX 10.7+.
TEST_F(UDPSocketTest, DISABLED_Broadcast) {
#elif defined(OS_ANDROID)
// It is also disabled for Android because it is extremely flaky.
// The first call to SendToSocket returns -109 (Address not reachable)
// in some unpredictable cases. crbug.com/139144.
TEST_F(UDPSocketTest, DISABLED_Broadcast) {
#else
TEST_F(UDPSocketTest, Broadcast) {
#endif
  const int kPort = 9999;
  std::string first_message("first message"), second_message("second message");

  IPEndPoint broadcast_address;
  CreateUDPAddress("255.255.255.255", kPort, &broadcast_address);
  IPEndPoint listen_address;
  CreateUDPAddress("0.0.0.0", kPort, &listen_address);

  CapturingNetLog server1_log, server2_log;
  scoped_ptr<UDPServerSocket> server1(
      new UDPServerSocket(&server1_log, NetLog::Source()));
  scoped_ptr<UDPServerSocket> server2(
      new UDPServerSocket(&server2_log, NetLog::Source()));
  server1->AllowAddressReuse();
  server1->AllowBroadcast();
  server2->AllowAddressReuse();
  server2->AllowBroadcast();

  int rv = server1->Listen(listen_address);
  EXPECT_EQ(OK, rv);
  rv = server2->Listen(listen_address);
  EXPECT_EQ(OK, rv);

  rv = SendToSocket(server1.get(), first_message, broadcast_address);
  ASSERT_EQ(static_cast<int>(first_message.size()), rv);
  std::string str = RecvFromSocket(server1.get());
  ASSERT_EQ(first_message, str);
  str = RecvFromSocket(server2.get());
  ASSERT_EQ(first_message, str);

  rv = SendToSocket(server2.get(), second_message, broadcast_address);
  ASSERT_EQ(static_cast<int>(second_message.size()), rv);
  str = RecvFromSocket(server1.get());
  ASSERT_EQ(second_message, str);
  str = RecvFromSocket(server2.get());
  ASSERT_EQ(second_message, str);
}

// In this test, we verify that random binding logic works, which attempts
// to bind to a random port and returns if succeeds, otherwise retries for
// |kBindRetries| number of times.

// To generate the scenario, we first create |kBindRetries| number of
// UDPClientSockets with default binding policy and connect to the same
// peer and save the used port numbers.  Then we get rid of the last
// socket, making sure that the local port it was bound to is available.
// Finally, we create a socket with random binding policy, passing it a
// test PRNG that would serve used port numbers in the array, one after
// another.  At the end, we make sure that the test socket was bound to the
// port that became available after deleting the last socket with default
// binding policy.

// We do not test the randomness of bound ports, but that we are using
// passed in PRNG correctly, thus, it's the duty of PRNG to produce strong
// random numbers.
static const int kBindRetries = 10;

class TestPrng {
 public:
  explicit TestPrng(const std::deque<int>& numbers) : numbers_(numbers) {}
  int GetNext(int /* min */, int /* max */) {
    DCHECK(!numbers_.empty());
    int rv = numbers_.front();
    numbers_.pop_front();
    return rv;
  }
 private:
  std::deque<int> numbers_;

  DISALLOW_COPY_AND_ASSIGN(TestPrng);
};

#if defined(OS_ANDROID)
// Disabled on Android for lack of 192.168.1.13. crbug.com/161245
TEST_F(UDPSocketTest, DISABLED_ConnectRandomBind) {
#else
TEST_F(UDPSocketTest, ConnectRandomBind) {
#endif
  std::vector<UDPClientSocket*> sockets;
  IPEndPoint peer_address;
  CreateUDPAddress("192.168.1.13", 53, &peer_address);

  // Create and connect sockets and save port numbers.
  std::deque<int> used_ports;
  for (int i = 0; i < kBindRetries; ++i) {
    UDPClientSocket* socket =
        new UDPClientSocket(DatagramSocket::DEFAULT_BIND,
                            RandIntCallback(),
                            NULL,
                            NetLog::Source());
    sockets.push_back(socket);
    EXPECT_EQ(OK, socket->Connect(peer_address));

    IPEndPoint client_address;
    EXPECT_EQ(OK, socket->GetLocalAddress(&client_address));
    used_ports.push_back(client_address.port());
  }

  // Free the last socket, its local port is still in |used_ports|.
  delete sockets.back();
  sockets.pop_back();

  TestPrng test_prng(used_ports);
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));

  // Create a socket with random binding policy and connect.
  scoped_ptr<UDPClientSocket> test_socket(
      new UDPClientSocket(DatagramSocket::RANDOM_BIND,
                          rand_int_cb,
                          NULL,
                          NetLog::Source()));
  EXPECT_EQ(OK, test_socket->Connect(peer_address));

  // Make sure that the last port number in the |used_ports| was used.
  IPEndPoint client_address;
  EXPECT_EQ(OK, test_socket->GetLocalAddress(&client_address));
  EXPECT_EQ(used_ports.back(), client_address.port());

  STLDeleteElements(&sockets);
}

// Return a privileged port (under 1024) so binding will fail.
int PrivilegedRand(int min, int max) {
  // Chosen by fair dice roll.  Guaranteed to be random.
  return 4;
}

TEST_F(UDPSocketTest, ConnectFail) {
  IPEndPoint peer_address;
  CreateUDPAddress("0.0.0.0", 53, &peer_address);

  scoped_ptr<UDPSocket> socket(
      new UDPSocket(DatagramSocket::RANDOM_BIND,
                    base::Bind(&PrivilegedRand),
                    NULL,
                    NetLog::Source()));
  int rv = socket->Connect(peer_address);
  // Connect should have failed since we couldn't bind to that port,
  EXPECT_NE(OK, rv);
  // Make sure that UDPSocket actually closed the socket.
  EXPECT_FALSE(socket->is_connected());
}

// In this test, we verify that connect() on a socket will have the effect
// of filtering reads on this socket only to data read from the destination
// we connected to.
//
// The purpose of this test is that some documentation indicates that connect
// binds the client's sends to send to a particular server endpoint, but does
// not bind the client's reads to only be from that endpoint, and that we need
// to always use recvfrom() to disambiguate.
TEST_F(UDPSocketTest, VerifyConnectBindsAddr) {
  const int kPort1 = 9999;
  const int kPort2 = 10000;
  std::string simple_message("hello world!");
  std::string foreign_message("BAD MESSAGE TO GET!!");

  // Setup the first server to listen.
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", kPort1, &bind_address);
  UDPServerSocket server1(NULL, NetLog::Source());
  server1.AllowAddressReuse();
  int rv = server1.Listen(bind_address);
  ASSERT_EQ(OK, rv);

  // Setup the second server to listen.
  CreateUDPAddress("127.0.0.1", kPort2, &bind_address);
  UDPServerSocket server2(NULL, NetLog::Source());
  server2.AllowAddressReuse();
  rv = server2.Listen(bind_address);
  ASSERT_EQ(OK, rv);

  // Setup the client, connected to server 1.
  IPEndPoint server_address;
  CreateUDPAddress("127.0.0.1", kPort1, &server_address);
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND,
                         RandIntCallback(),
                         NULL,
                         NetLog::Source());
  rv = client.Connect(server_address);
  EXPECT_EQ(OK, rv);

  // Client sends to server1.
  rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Server1 waits for message.
  std::string str = RecvFromSocket(&server1);
  DCHECK(simple_message == str);

  // Get the client's address.
  IPEndPoint client_address;
  rv = client.GetLocalAddress(&client_address);
  EXPECT_EQ(OK, rv);

  // Server2 sends reply.
  rv = SendToSocket(&server2, foreign_message,
                    client_address);
  EXPECT_EQ(foreign_message.length(), static_cast<size_t>(rv));

  // Server1 sends reply.
  rv = SendToSocket(&server1, simple_message,
                    client_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Client waits for response.
  str = ReadSocket(&client);
  DCHECK(simple_message == str);
}

TEST_F(UDPSocketTest, ClientGetLocalPeerAddresses) {
  struct TestData {
    std::string remote_address;
    std::string local_address;
    bool may_fail;
  } tests[] = {
    { "127.0.00.1", "127.0.0.1", false },
    { "::1", "::1", true },
#if !defined(OS_ANDROID)
    // Addresses below are disabled on Android. See crbug.com/161248
    { "192.168.1.1", "127.0.0.1", false },
    { "2001:db8:0::42", "::1", true },
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    SCOPED_TRACE(std::string("Connecting from ") +  tests[i].local_address +
                 std::string(" to ") + tests[i].remote_address);

    IPAddressNumber ip_number;
    ParseIPLiteralToNumber(tests[i].remote_address, &ip_number);
    IPEndPoint remote_address(ip_number, 80);
    ParseIPLiteralToNumber(tests[i].local_address, &ip_number);
    IPEndPoint local_address(ip_number, 80);

    UDPClientSocket client(DatagramSocket::DEFAULT_BIND,
                           RandIntCallback(),
                           NULL,
                           NetLog::Source());
    int rv = client.Connect(remote_address);
    if (tests[i].may_fail && rv == ERR_ADDRESS_UNREACHABLE) {
      // Connect() may return ERR_ADDRESS_UNREACHABLE for IPv6
      // addresses if IPv6 is not configured.
      continue;
    }

    EXPECT_LE(ERR_IO_PENDING, rv);

    IPEndPoint fetched_local_address;
    rv = client.GetLocalAddress(&fetched_local_address);
    EXPECT_EQ(OK, rv);

    // TODO(mbelshe): figure out how to verify the IP and port.
    //                The port is dynamically generated by the udp stack.
    //                The IP is the real IP of the client, not necessarily
    //                loopback.
    //EXPECT_EQ(local_address.address(), fetched_local_address.address());

    IPEndPoint fetched_remote_address;
    rv = client.GetPeerAddress(&fetched_remote_address);
    EXPECT_EQ(OK, rv);

    EXPECT_EQ(remote_address, fetched_remote_address);
  }
}

TEST_F(UDPSocketTest, ServerGetLocalAddress) {
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", 0, &bind_address);
  UDPServerSocket server(NULL, NetLog::Source());
  int rv = server.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  IPEndPoint local_address;
  rv = server.GetLocalAddress(&local_address);
  EXPECT_EQ(rv, 0);

  // Verify that port was allocated.
  EXPECT_GT(local_address.port(), 0);
  EXPECT_EQ(local_address.address(), bind_address.address());
}

TEST_F(UDPSocketTest, ServerGetPeerAddress) {
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", 0, &bind_address);
  UDPServerSocket server(NULL, NetLog::Source());
  int rv = server.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  IPEndPoint peer_address;
  rv = server.GetPeerAddress(&peer_address);
  EXPECT_EQ(rv, ERR_SOCKET_NOT_CONNECTED);
}

// Close the socket while read is pending.
TEST_F(UDPSocketTest, CloseWithPendingRead) {
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", 0, &bind_address);
  UDPServerSocket server(NULL, NetLog::Source());
  int rv = server.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  TestCompletionCallback callback;
  IPEndPoint from;
  rv = server.RecvFrom(buffer_.get(), kMaxRead, &from, callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);

  server.Close();

  EXPECT_FALSE(callback.have_result());
}

#if defined(OS_ANDROID)
// Some Android devices do not support multicast socket.
// The ones supporting multicast need WifiManager.MulitcastLock to enable it.
// http://goo.gl/jjAk9
#define MAYBE_JoinMulticastGroup DISABLED_JoinMulticastGroup
#else
#define MAYBE_JoinMulticastGroup JoinMulticastGroup
#endif  // defined(OS_ANDROID)

TEST_F(UDPSocketTest, MAYBE_JoinMulticastGroup) {
  const int kPort = 9999;
  const char* const kGroup = "237.132.100.17";

  IPEndPoint bind_address;
  CreateUDPAddress("0.0.0.0", kPort, &bind_address);
  IPAddressNumber group_ip;
  EXPECT_TRUE(ParseIPLiteralToNumber(kGroup, &group_ip));

  UDPSocket socket(DatagramSocket::DEFAULT_BIND,
                   RandIntCallback(),
                   NULL,
                   NetLog::Source());
  EXPECT_EQ(OK, socket.Bind(bind_address));
  EXPECT_EQ(OK, socket.JoinGroup(group_ip));
  // Joining group multiple times.
  EXPECT_NE(OK, socket.JoinGroup(group_ip));
  EXPECT_EQ(OK, socket.LeaveGroup(group_ip));
  // Leaving group multiple times.
  EXPECT_NE(OK, socket.LeaveGroup(group_ip));

  socket.Close();
}

TEST_F(UDPSocketTest, MulticastOptions) {
  const int kPort = 9999;
  IPEndPoint bind_address;
  CreateUDPAddress("0.0.0.0", kPort, &bind_address);

  UDPSocket socket(DatagramSocket::DEFAULT_BIND,
                   RandIntCallback(),
                   NULL,
                   NetLog::Source());
  // Before binding.
  EXPECT_EQ(OK, socket.SetMulticastLoopbackMode(false));
  EXPECT_EQ(OK, socket.SetMulticastLoopbackMode(true));
  EXPECT_EQ(OK, socket.SetMulticastTimeToLive(0));
  EXPECT_EQ(OK, socket.SetMulticastTimeToLive(3));
  EXPECT_NE(OK, socket.SetMulticastTimeToLive(-1));
  EXPECT_EQ(OK, socket.SetMulticastInterface(0));

  EXPECT_EQ(OK, socket.Bind(bind_address));

  EXPECT_NE(OK, socket.SetMulticastLoopbackMode(false));
  EXPECT_NE(OK, socket.SetMulticastTimeToLive(0));
  EXPECT_NE(OK, socket.SetMulticastInterface(0));

  socket.Close();
}

}  // namespace

}  // namespace net
