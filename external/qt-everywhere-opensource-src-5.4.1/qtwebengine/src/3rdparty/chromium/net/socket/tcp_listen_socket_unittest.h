// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LISTEN_SOCKET_UNITTEST_H_
#define NET_BASE_LISTEN_SOCKET_UNITTEST_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#endif

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "net/base/net_util.h"
#include "net/base/winsock_init.h"
#include "net/socket/tcp_listen_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

enum ActionType {
  ACTION_NONE = 0,
  ACTION_LISTEN = 1,
  ACTION_ACCEPT = 2,
  ACTION_READ = 3,
  ACTION_SEND = 4,
  ACTION_CLOSE = 5,
  ACTION_SHUTDOWN = 6
};

class TCPListenSocketTestAction {
 public:
  TCPListenSocketTestAction() : action_(ACTION_NONE) {}
  explicit TCPListenSocketTestAction(ActionType action) : action_(action) {}
  TCPListenSocketTestAction(ActionType action, std::string data)
      : action_(action),
        data_(data) {}

  const std::string data() const { return data_; }
  ActionType type() const { return action_; }

 private:
  ActionType action_;
  std::string data_;
};


// This had to be split out into a separate class because I couldn't
// make the testing::Test class refcounted.
class TCPListenSocketTester :
    public StreamListenSocket::Delegate,
    public base::RefCountedThreadSafe<TCPListenSocketTester> {

 public:
  TCPListenSocketTester();

  void SetUp();
  void TearDown();

  void ReportAction(const TCPListenSocketTestAction& action);
  void NextAction();

  // read all pending data from the test socket
  int ClearTestSocket();
  // Release the connection and server sockets
  void Shutdown();
  void Listen();
  void SendFromTester();
  // verify the send/read from client to server
  void TestClientSend();
  // verify send/read of a longer string
  void TestClientSendLong();
  // verify a send/read from server to client
  void TestServerSend();
  // verify multiple sends and reads from server to client.
  void TestServerSendMultiple();

  virtual bool Send(SocketDescriptor sock, const std::string& str);

  // StreamListenSocket::Delegate:
  virtual void DidAccept(StreamListenSocket* server,
                         scoped_ptr<StreamListenSocket> connection) OVERRIDE;
  virtual void DidRead(StreamListenSocket* connection, const char* data,
                       int len) OVERRIDE;
  virtual void DidClose(StreamListenSocket* sock) OVERRIDE;

  scoped_ptr<base::Thread> thread_;
  base::MessageLoopForIO* loop_;
  scoped_ptr<TCPListenSocket> server_;
  scoped_ptr<StreamListenSocket> connection_;
  TCPListenSocketTestAction last_action_;

  SocketDescriptor test_socket_;

  base::Lock lock_;  // Protects |queue_| and |server_port_|. Wraps |cv_|.
  base::ConditionVariable cv_;
  std::deque<TCPListenSocketTestAction> queue_;

 private:
  friend class base::RefCountedThreadSafe<TCPListenSocketTester>;

  virtual ~TCPListenSocketTester();

  virtual scoped_ptr<TCPListenSocket> DoListen();

  // Getters/setters for |server_port_|. They use |lock_| for thread safety.
  int GetServerPort();
  void SetServerPort(int server_port);

  // Port the server is using. Must have |lock_| to access. Set by Listen() on
  // the server's thread.
  int server_port_;
};

}  // namespace net

#endif  // NET_BASE_LISTEN_SOCKET_UNITTEST_H_
