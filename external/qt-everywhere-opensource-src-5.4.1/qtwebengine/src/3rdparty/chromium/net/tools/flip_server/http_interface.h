// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_

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
  virtual ~HttpSM();

 private:
  // BalsaVisitorInterface:
  virtual void ProcessBodyInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessBodyData(const char* input, size_t size) OVERRIDE;
  virtual void ProcessHeaderInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessTrailerInput(const char* input, size_t size) OVERRIDE {}
  virtual void ProcessHeaders(const BalsaHeaders& headers) OVERRIDE;
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
  virtual void MessageDone() OVERRIDE;
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE {}
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE;

  void HandleError();

 public:
  void AddToOutputOrder(const MemCacheIter& mci);
  BalsaFrame* spdy_framer() { return http_framer_; }
  virtual void set_is_request() OVERRIDE {}
  const OutputOrdering& output_ordering() const { return output_ordering_; }

  // SMInterface:
  virtual void InitSMInterface(SMInterface* sm_spdy_interface,
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
  virtual void SetStreamID(uint32 stream_id) OVERRIDE;
  virtual bool Error() const OVERRIDE;
  virtual const char* ErrorAsString() const OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void ResetForNewInterface(int32 server_idx) OVERRIDE {}
  virtual void ResetForNewConnection() OVERRIDE;
  virtual void Cleanup() OVERRIDE;
  virtual int PostAcceptHook() OVERRIDE;

  virtual void NewStream(uint32 stream_id,
                         uint32 priority,
                         const std::string& filename) OVERRIDE;
  virtual void SendEOF(uint32 stream_id) OVERRIDE;
  virtual void SendErrorNotFound(uint32 stream_id) OVERRIDE;
  virtual size_t SendSynStream(uint32 stream_id,
                               const BalsaHeaders& headers) OVERRIDE;
  virtual size_t SendSynReply(uint32 stream_id,
                              const BalsaHeaders& headers) OVERRIDE;
  virtual void SendDataFrame(uint32 stream_id,
                             const char* data,
                             int64 len,
                             uint32 flags,
                             bool compress) OVERRIDE;

 private:
  void SendEOFImpl(uint32 stream_id);
  void SendErrorNotFoundImpl(uint32 stream_id);
  void SendOKResponseImpl(uint32 stream_id, const std::string& output);
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32 stream_id,
                         const char* data,
                         int64 len,
                         uint32 flags,
                         bool compress);
  void EnqueueDataFrame(DataFrame* df);
  virtual void GetOutput() OVERRIDE;

 private:
  BalsaFrame* http_framer_;
  BalsaHeaders headers_;
  uint32 stream_id_;
  int32 server_idx_;

  SMConnection* connection_;
  SMInterface* sm_spdy_interface_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
  FlipAcceptor* acceptor_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_H_
