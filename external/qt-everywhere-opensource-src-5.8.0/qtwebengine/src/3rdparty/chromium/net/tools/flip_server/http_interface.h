// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/balsa/balsa_visitor_interface.h"
#include "net/tools/flip_server/output_ordering.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/sm_interface.h"

namespace net {

class BalsaFrame;
class DataFrame;
class EpollServer;
class FlipAcceptor;
class MemoryCache;

class HttpSM : public BalsaVisitorInterface, public SMInterface {
 public:
  HttpSM(SMConnection* connection,
         SMInterface* sm_spdy_interface,
         MemoryCache* memory_cache,
         FlipAcceptor* acceptor);
  ~HttpSM() override;

 private:
  // BalsaVisitorInterface:
  void ProcessBodyInput(const char* input, size_t size) override {}
  void ProcessBodyData(const char* input, size_t size) override;
  void ProcessHeaderInput(const char* input, size_t size) override {}
  void ProcessTrailerInput(const char* input, size_t size) override {}
  void ProcessHeaders(const BalsaHeaders& headers) override;
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
  void MessageDone() override;
  void HandleHeaderError(BalsaFrame* framer) override;
  void HandleHeaderWarning(BalsaFrame* framer) override {}
  void HandleChunkingError(BalsaFrame* framer) override;
  void HandleBodyError(BalsaFrame* framer) override;

  void HandleError();

 public:
  void AddToOutputOrder(const MemCacheIter& mci);
  BalsaFrame* spdy_framer() { return http_framer_.get(); }
  void set_is_request() override {}
  const OutputOrdering& output_ordering() const { return output_ordering_; }

  // SMInterface:
  void InitSMInterface(SMInterface* sm_spdy_interface,
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
  void SetStreamID(uint32_t stream_id) override;
  bool Error() const override;
  const char* ErrorAsString() const override;
  void Reset() override;
  void ResetForNewInterface(int32_t server_idx) override {}
  void ResetForNewConnection() override;
  void Cleanup() override;
  int PostAcceptHook() override;

  void NewStream(uint32_t stream_id,
                 uint32_t priority,
                 const std::string& filename) override;
  void SendEOF(uint32_t stream_id) override;
  void SendErrorNotFound(uint32_t stream_id) override;
  size_t SendSynStream(uint32_t stream_id,
                       const BalsaHeaders& headers) override;
  size_t SendSynReply(uint32_t stream_id, const BalsaHeaders& headers) override;
  void SendDataFrame(uint32_t stream_id,
                     const char* data,
                     int64_t len,
                     uint32_t flags,
                     bool compress) override;

 private:
  void SendEOFImpl(uint32_t stream_id);
  void SendErrorNotFoundImpl(uint32_t stream_id);
  void SendOKResponseImpl(uint32_t stream_id, const std::string& output);
  size_t SendSynReplyImpl(uint32_t stream_id, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32_t stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32_t stream_id,
                         const char* data,
                         int64_t len,
                         uint32_t flags,
                         bool compress);
  void EnqueueDataFrame(DataFrame* df);
  void GetOutput() override;

 private:
  std::unique_ptr<BalsaFrame> http_framer_;
  BalsaHeaders headers_;
  uint32_t stream_id_;
  int32_t server_idx_;

  SMConnection* connection_;
  SMInterface* sm_spdy_interface_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
  FlipAcceptor* acceptor_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_
