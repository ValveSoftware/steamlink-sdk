// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SM_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_SM_INTERFACE_H_

// State Machine Interfaces

#include <string>

#include "net/tools/balsa/balsa_headers.h"

namespace net {

class EpollServer;
class SMConnectionPoolInterface;
class SMConnection;

class SMInterface {
 public:
  virtual void InitSMInterface(SMInterface* sm_other_interface,
                               int32 server_idx) = 0;
  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl) = 0;
  virtual size_t ProcessReadInput(const char* data, size_t len) = 0;
  virtual size_t ProcessWriteInput(const char* data, size_t len) = 0;
  virtual void SetStreamID(uint32 stream_id) = 0;
  virtual bool MessageFullyRead() const = 0;
  virtual bool Error() const = 0;
  virtual const char* ErrorAsString() const = 0;
  virtual void Reset() = 0;
  virtual void ResetForNewInterface(int32 server_idx) = 0;
  // ResetForNewConnection is used for interfaces which control SMConnection
  // objects. When called an interface may put its connection object into
  // a reusable instance pool. Currently this is what the HttpSM interface
  // does.
  virtual void ResetForNewConnection() = 0;
  virtual void Cleanup() = 0;

  virtual int PostAcceptHook() = 0;

  virtual void NewStream(uint32 stream_id,
                         uint32 priority,
                         const std::string& filename) = 0;
  virtual void SendEOF(uint32 stream_id) = 0;
  virtual void SendErrorNotFound(uint32 stream_id) = 0;
  virtual size_t SendSynStream(uint32 stream_id,
                               const BalsaHeaders& headers) = 0;
  virtual size_t SendSynReply(uint32 stream_id,
                              const BalsaHeaders& headers) = 0;
  virtual void SendDataFrame(uint32 stream_id,
                             const char* data,
                             int64 len,
                             uint32 flags,
                             bool compress) = 0;
  virtual void GetOutput() = 0;
  virtual void set_is_request() = 0;

  virtual ~SMInterface() {}
};

class SMConnectionInterface {
 public:
  virtual ~SMConnectionInterface() {}
  virtual void ReadyToSend() = 0;
  virtual EpollServer* epoll_server() = 0;
};

class SMConnectionPoolInterface {
 public:
  virtual ~SMConnectionPoolInterface() {}
  virtual void SMConnectionDone(SMConnection* connection) = 0;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SM_INTERFACE_H_
