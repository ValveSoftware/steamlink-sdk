// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/buffered_spdy_framer.h"
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
  virtual ~SpdySM();

  virtual void InitSMInterface(SMInterface* sm_http_interface,
                               int32 server_idx) OVERRIDE {}

  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl) OVERRIDE;

  // Create new SPDY framer after reusing SpdySM and negotiating new version
  void CreateFramer(SpdyMajorVersion spdy_version);

 private:
  virtual void set_is_request() OVERRIDE {}
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
  virtual void OnError(SpdyFramer::SpdyError error_code) OVERRIDE {}
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) OVERRIDE {}
  // Called after all the header data for SYN_STREAM control frame is received.
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) OVERRIDE;

  // Called after all the header data for SYN_REPLY control frame is received.
  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) OVERRIDE;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) OVERRIDE;

  // Called when data frame header is received.
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {}

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE;

  // Called when a SETTINGS frame is received.
  // |clear_persisted| True if the respective flag is set on the SETTINGS frame.
  virtual void OnSettings(bool clear_persisted) OVERRIDE {}

  // Called when an individual setting within a SETTINGS frame has been parsed
  // and validated.
  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {}

  // Called when a PING frame has been parsed.
  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {}

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {}

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {}

  // Called when a PUSH_PROMISE frame has been parsed.
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             const SpdyHeaderBlock& headers) OVERRIDE {}

 public:
  virtual size_t ProcessReadInput(const char* data, size_t len) OVERRIDE;
  virtual size_t ProcessWriteInput(const char* data, size_t len) OVERRIDE;
  virtual bool MessageFullyRead() const OVERRIDE;
  virtual void SetStreamID(uint32 stream_id) OVERRIDE {}
  virtual bool Error() const OVERRIDE;
  virtual const char* ErrorAsString() const OVERRIDE;
  virtual void Reset() OVERRIDE {}
  virtual void ResetForNewInterface(int32 server_idx) OVERRIDE;
  virtual void ResetForNewConnection() OVERRIDE;
  // SMInterface's Cleanup is currently only called by SMConnection after a
  // protocol message as been fully read. Spdy's SMInterface does not need
  // to do any cleanup at this time.
  // TODO(klindsay) This method is probably not being used properly and
  // some logic review and method renaming is probably in order.
  virtual void Cleanup() OVERRIDE {}
  // Send a settings frame
  virtual int PostAcceptHook() OVERRIDE;
  virtual void NewStream(uint32 stream_id,
                         uint32 priority,
                         const std::string& filename) OVERRIDE;
  void AddToOutputOrder(const MemCacheIter& mci);
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
  void SendEOFImpl(uint32 stream_id);
  void SendErrorNotFoundImpl(uint32 stream_id);
  void KillStream(uint32 stream_id);
  void CopyHeaders(SpdyHeaderBlock& dest, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers);
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32 stream_id,
                         const char* data,
                         int64 len,
                         SpdyDataFlags flags,
                         bool compress);
  void EnqueueDataFrame(DataFrame* df);
  virtual void GetOutput() OVERRIDE;

 private:
  scoped_ptr<BufferedSpdyFramer> buffered_spdy_framer_;
  bool valid_spdy_session_;  // True if we have seen valid data on this session.
                             // Use this to fail fast when junk is sent to our
                             // port.

  SMConnection* connection_;
  OutputList* client_output_list_;
  OutputOrdering client_output_ordering_;
  uint32 next_outgoing_stream_id_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
  MemoryCache* memory_cache_;
  std::vector<SMInterface*> server_interface_list;
  std::vector<int32> unused_server_interface_list;
  typedef std::map<uint32, SMInterface*> StreamToSmif;
  StreamToSmif stream_to_smif_;
  bool close_on_error_;

  static std::string forward_ip_header_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
