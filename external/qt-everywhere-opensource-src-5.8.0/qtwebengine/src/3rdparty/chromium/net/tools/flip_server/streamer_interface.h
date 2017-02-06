// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/balsa/balsa_visitor_interface.h"
#include "net/tools/flip_server/sm_interface.h"

namespace net {

class BalsaFrame;
class FlipAcceptor;
class MemCacheIter;
class SMConnection;
class EpollServer;

class StreamerSM : public BalsaVisitorInterface, public SMInterface {
 public:
  StreamerSM(SMConnection* connection,
             SMInterface* sm_other_interface,
             EpollServer* epoll_server,
             FlipAcceptor* acceptor);
  ~StreamerSM() override;

  void AddToOutputOrder(const MemCacheIter& mci) {}

  void InitSMInterface(SMInterface* sm_other_interface,
                       int32_t server_idx) override;
  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        std::string server_ip,
                        std::string server_port,
                        std::string remote_ip,
                        bool use_ssl) override;

  size_t ProcessReadInput(const char* data, size_t len) override;
  size_t ProcessWriteInput(const char* data, size_t len) override;
  bool MessageFullyRead() const override;
  void SetStreamID(uint32_t stream_id) override {}
  bool Error() const override;
  const char* ErrorAsString() const override;
  void Reset() override;
  void ResetForNewInterface(int32_t server_idx) override {}
  void ResetForNewConnection() override;
  void Cleanup() override;
  int PostAcceptHook() override;
  void NewStream(uint32_t stream_id,
                 uint32_t priority,
                 const std::string& filename) override {}
  void SendEOF(uint32_t stream_id) override {}
  void SendErrorNotFound(uint32_t stream_id) override {}
  virtual void SendOKResponse(uint32_t stream_id, const std::string& output) {}
  size_t SendSynStream(uint32_t stream_id,
                       const BalsaHeaders& headers) override;
  size_t SendSynReply(uint32_t stream_id, const BalsaHeaders& headers) override;
  void SendDataFrame(uint32_t stream_id,
                     const char* data,
                     int64_t len,
                     uint32_t flags,
                     bool compress) override {}
  void set_is_request() override;
  static std::string forward_ip_header() { return forward_ip_header_; }
  static void set_forward_ip_header(const std::string& value) {
    forward_ip_header_ = value;
  }

 private:
  void SendEOFImpl(uint32_t stream_id) {}
  void SendErrorNotFoundImpl(uint32_t stream_id) {}
  void SendOKResponseImpl(uint32_t stream_id, std::string* output) {}
  size_t SendSynReplyImpl(uint32_t stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  size_t SendSynStreamImpl(uint32_t stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  void SendDataFrameImpl(uint32_t stream_id,
                         const char* data,
                         int64_t len,
                         uint32_t flags,
                         bool compress) {}
  void GetOutput() override {}

  void ProcessBodyInput(const char* input, size_t size) override;
  void MessageDone() override;
  void ProcessHeaders(const BalsaHeaders& headers) override;
  void ProcessBodyData(const char* input, size_t size) override {}
  void ProcessHeaderInput(const char* input, size_t size) override {}
  void ProcessTrailerInput(const char* input, size_t size) override {}
  void ProcessRequestFirstLine(const char* line_input,
                               size_t line_length,
                               const char* method_input,
                               size_t method_length,
                               const char* request_uri_input,
                               size_t request_uri_length,
                               const char* version_input,
                               size_t version_length) override {}
  void ProcessResponseFirstLine(const char* line_input,
                                size_t line_length,
                                const char* version_input,
                                size_t version_length,
                                const char* status_input,
                                size_t status_length,
                                const char* reason_input,
                                size_t reason_length) override {}
  void ProcessChunkLength(size_t chunk_length) override {}
  void ProcessChunkExtensions(const char* input, size_t size) override {}
  void HeaderDone() override {}
  void HandleHeaderError(BalsaFrame* framer) override;
  void HandleHeaderWarning(BalsaFrame* framer) override {}
  void HandleChunkingError(BalsaFrame* framer) override;
  void HandleBodyError(BalsaFrame* framer) override;
  void HandleError();

  SMConnection* connection_;
  SMInterface* sm_other_interface_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
  bool is_request_;
  BalsaFrame* http_framer_;
  BalsaHeaders headers_;
  static std::string forward_ip_header_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_H_
