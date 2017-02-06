// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_alt_svc_wire_format.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/balsa/balsa_visitor_interface.h"
#include "net/tools/flip_server/output_ordering.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/sm_interface.h"

namespace net {

class FlipAcceptor;
class MemoryCache;

class SpdySM : public BufferedSpdyFramerVisitorInterface, public SMInterface {
 public:
  SpdySM(SMConnection* connection,
         SMInterface* sm_http_interface,
         EpollServer* epoll_server,
         MemoryCache* memory_cache,
         FlipAcceptor* acceptor,
         SpdyMajorVersion spdy_version);
  ~SpdySM() override;

  void InitSMInterface(SMInterface* sm_http_interface,
                       int32_t server_idx) override {}

  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        std::string server_ip,
                        std::string server_port,
                        std::string remote_ip,
                        bool use_ssl) override;

  // Create new SPDY framer after reusing SpdySM and negotiating new version
  void CreateFramer(SpdyMajorVersion spdy_version);

 private:
  void set_is_request() override {}
  SMInterface* NewConnectionInterface();
  // virtual for tests
  virtual SMInterface* FindOrMakeNewSMConnectionInterface(
      const std::string& server_ip,
      const std::string& server_port);
  int SpdyHandleNewStream(SpdyStreamId stream_id,
                          SpdyPriority priority,
                          const SpdyHeaderBlock& headers,
                          std::string& http_data,
                          bool* is_https_scheme);

  // BufferedSpdyFramerVisitorInterface:
  void OnError(SpdyFramer::SpdyError error_code) override {}
  void OnStreamError(SpdyStreamId stream_id,
                     const std::string& description) override {}
  // Called after all the header data for SYN_STREAM control frame is received.
  void OnSynStream(SpdyStreamId stream_id,
                   SpdyStreamId associated_stream_id,
                   SpdyPriority priority,
                   bool fin,
                   bool unidirectional,
                   const SpdyHeaderBlock& headers) override;

  // Called after all the header data for SYN_REPLY control frame is received.
  void OnSynReply(SpdyStreamId stream_id,
                  bool fin,
                  const SpdyHeaderBlock& headers) override;

  // Called after all the header data for HEADERS control frame is received.
  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 const SpdyHeaderBlock& headers) override;

  // Called when data frame header is received.
  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {}

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override;

  // Called when the other side has finished sending data on this stream.
  // |stream_id| The stream that was receivin data.
  void OnStreamEnd(SpdyStreamId stream_id) override;

  // Called when padding is received (padding length field or padding octets).
  // |stream_id| The stream receiving data.
  // |len| The number of padding octets.
  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override;

  // Called when a SETTINGS frame is received.
  // |clear_persisted| True if the respective flag is set on the SETTINGS frame.
  void OnSettings(bool clear_persisted) override {}

  // Called when an individual setting within a SETTINGS frame has been parsed
  // and validated.
  void OnSetting(SpdySettingsIds id, uint8_t flags, uint32_t value) override {}

  // Called when a PING frame has been parsed.
  void OnPing(SpdyPingId unique_id, bool is_ack) override {}

  // Called when a RST_STREAM frame has been parsed.
  void OnRstStream(SpdyStreamId stream_id, SpdyRstStreamStatus status) override;

  // Called when a GOAWAY frame has been parsed.
  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyGoAwayStatus status,
                base::StringPiece debug_data) override {}

  // Called when a WINDOW_UPDATE frame has been parsed.
  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {}

  // Called when a PUSH_PROMISE frame has been parsed.
  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     const SpdyHeaderBlock& headers) override {}

  // Called when an ALTSVC frame has been parsed.
  void OnAltSvc(SpdyStreamId stream_id,
                base::StringPiece origin,
                const SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override {}

  bool OnUnknownFrame(SpdyStreamId stream_id, int frame_type) override;

 public:
  size_t ProcessReadInput(const char* data, size_t len) override;
  size_t ProcessWriteInput(const char* data, size_t len) override;
  bool MessageFullyRead() const override;
  void SetStreamID(uint32_t stream_id) override {}
  bool Error() const override;
  const char* ErrorAsString() const override;
  void Reset() override {}
  void ResetForNewInterface(int32_t server_idx) override;
  void ResetForNewConnection() override;
  // SMInterface's Cleanup is currently only called by SMConnection after a
  // protocol message as been fully read. Spdy's SMInterface does not need
  // to do any cleanup at this time.
  // TODO(klindsay) This method is probably not being used properly and
  // some logic review and method renaming is probably in order.
  void Cleanup() override {}
  // Send a settings frame
  int PostAcceptHook() override;
  void NewStream(uint32_t stream_id,
                 uint32_t priority,
                 const std::string& filename) override;
  void AddToOutputOrder(const MemCacheIter& mci);
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
  BufferedSpdyFramer* spdy_framer() { return buffered_spdy_framer_.get(); }

  const OutputOrdering& output_ordering() const {
    return client_output_ordering_;
  }

  static std::string forward_ip_header() { return forward_ip_header_; }
  static void set_forward_ip_header(const std::string& value) {
    forward_ip_header_ = value;
  }
  SpdyMajorVersion spdy_version() const {
    DCHECK(buffered_spdy_framer_);
    return buffered_spdy_framer_->protocol_version();
  }

 private:
  void SendEOFImpl(uint32_t stream_id);
  void SendErrorNotFoundImpl(uint32_t stream_id);
  void KillStream(uint32_t stream_id);
  void CopyHeaders(SpdyHeaderBlock& dest, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32_t stream_id, const BalsaHeaders& headers);
  size_t SendSynReplyImpl(uint32_t stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32_t stream_id,
                         const char* data,
                         int64_t len,
                         SpdyDataFlags flags,
                         bool compress);
  void EnqueueDataFrame(DataFrame* df);
  void GetOutput() override;

 private:
  std::unique_ptr<BufferedSpdyFramer> buffered_spdy_framer_;
  bool valid_spdy_session_;  // True if we have seen valid data on this session.
                             // Use this to fail fast when junk is sent to our
                             // port.

  SMConnection* connection_;
  OutputList* client_output_list_;
  OutputOrdering client_output_ordering_;
  uint32_t next_outgoing_stream_id_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
  MemoryCache* memory_cache_;
  std::vector<SMInterface*> server_interface_list;
  std::vector<int32_t> unused_server_interface_list;
  typedef std::map<uint32_t, SMInterface*> StreamToSmif;
  StreamToSmif stream_to_smif_;
  bool close_on_error_;

  static std::string forward_ip_header_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
