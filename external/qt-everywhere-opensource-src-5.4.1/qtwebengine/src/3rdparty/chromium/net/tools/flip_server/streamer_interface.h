// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_H_

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
  virtual ~StreamerSM();

  void AddToOutputOrder(const MemCacheIter& mci) {}

  virtual void InitSMInterface(SMInterface* sm_other_interface,
                               int32 server_idx) OVERRIDE;
  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl) OVERRIDE;

  virtual size_t ProcessReadInput(const char* data, size_t len) OVERRIDE;
  virtual size_t ProcessWriteInput(const char* data, size_t len) OVERRIDE;
  virtual bool MessageFullyRead() const OVERRIDE;
  virtual void SetStreamID(uint32 stream_id) OVERRIDE {}
  virtual bool Error() const OVERRIDE;
  virtual const char* ErrorAsString() const OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void ResetForNewInterface(int32 server_idx) OVERRIDE {}
  virtual void ResetForNewConnection() OVERRIDE;
  virtual void Cleanup() OVERRIDE;
  virtual int PostAcceptHook() OVERRIDE;
  virtual void NewStream(uint32 stream_id,
                         uint32 priority,
                         const std::string& filename) OVERRIDE {}
  virtual void SendEOF(uint32 stream_id) OVERRIDE {}
  virtual void SendErrorNotFound(uint32 stream_id) OVERRIDE {}
  virtual void SendOKResponse(uint32 stream_id, std::string output) {}
  virtual size_t SendSynStream(uint32 stream_id,
                               const BalsaHeaders& headers) OVERRIDE;
  virtual size_t SendSynReply(uint32 stream_id,
                              const BalsaHeaders& headers) OVERRIDE;
  virtual void SendDataFrame(uint32 stream_id,
                             const char* data,
                             int64 len,
                             uint32 flags,
                             bool compress) OVERRIDE {}
  virtual void set_is_request() OVERRIDE;
  static std::string forward_ip_header() { return forward_ip_header_; }
  static void set_forward_ip_header(std::string value) {
    forward_ip_header_ = value;
  }

 private:
  void SendEOFImpl(uint32 stream_id) {}
  void SendErrorNotFoundImpl(uint32 stream_id) {}
  void SendOKResponseImpl(uint32 stream_id, std::string* output) {}
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  void SendDataFrameImpl(uint32 stream_id,
                         const char* data,
                         int64 len,
                         uint32 flags,
                         bool compress) {}
  virtual void GetOutput() OVERRIDE {}

  virtual void ProcessBodyInput(const char* input, size_t size) OVERRIDE;
  virtual void MessageDone() OVERRIDE;
  virtual void ProcessHeaders(const BalsaHeaders& headers) OVERRIDE;
  virtual void ProcessBodyData(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessHeaderInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessTrailerInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) OVERRIDE {}
  virtual void ProcessResponseFirstLine(const char* line_input,
                                        size_t line_length,
                                        const char* version_input,
                                        size_t version_length,
                                        const char* status_input,
                                        size_t status_length,
                                        const char* reason_input,
                                        size_t reason_length) OVERRIDE {}
  virtual void ProcessChunkLength(size_t chunk_length) OVERRIDE {}
  virtual void ProcessChunkExtensions(const char* input, size_t size) OVERRIDE {
  }
  virtual void HeaderDone() OVERRIDE {}
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE {}
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE;
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
