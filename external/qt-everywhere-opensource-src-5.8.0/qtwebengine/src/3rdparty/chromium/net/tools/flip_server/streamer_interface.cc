// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/streamer_interface.h"

#include <string>

#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/sm_connection.h"

namespace net {

std::string StreamerSM::forward_ip_header_;

StreamerSM::StreamerSM(SMConnection* connection,
                       SMInterface* sm_other_interface,
                       EpollServer* epoll_server,
                       FlipAcceptor* acceptor)
    : connection_(connection),
      sm_other_interface_(sm_other_interface),
      epoll_server_(epoll_server),
      acceptor_(acceptor),
      is_request_(false),
      http_framer_(new BalsaFrame) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Creating StreamerSM object";
  http_framer_->set_balsa_visitor(this);
  http_framer_->set_balsa_headers(&headers_);
  http_framer_->set_is_request(false);
}

StreamerSM::~StreamerSM() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Destroying StreamerSM object";
  Reset();
  delete http_framer_;
}

void StreamerSM::set_is_request() {
  is_request_ = true;
  http_framer_->set_is_request(true);
}

void StreamerSM::InitSMInterface(SMInterface* sm_other_interface,
                                 int32_t server_idx) {
  sm_other_interface_ = sm_other_interface;
}

void StreamerSM::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                  SMInterface* sm_interface,
                                  EpollServer* epoll_server,
                                  int fd,
                                  std::string server_ip,
                                  std::string server_port,
                                  std::string remote_ip,
                                  bool use_ssl) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Initializing server "
          << "connection.";
  connection_->InitSMConnection(connection_pool,
                                sm_interface,
                                epoll_server,
                                fd,
                                server_ip,
                                server_port,
                                remote_ip,
                                use_ssl);
}

size_t StreamerSM::ProcessReadInput(const char* data, size_t len) {
  // For now we only want to parse http requests. Just stream responses
  if (is_request_) {
    return http_framer_->ProcessInput(data, len);
  } else {
    return sm_other_interface_->ProcessWriteInput(data, len);
  }
}

size_t StreamerSM::ProcessWriteInput(const char* data, size_t len) {
  char* dataPtr = new char[len];
  memcpy(dataPtr, data, len);
  DataFrame* df = new DataFrame;
  df->data = (const char*)dataPtr;
  df->size = len;
  df->delete_when_done = true;
  connection_->EnqueueDataFrame(df);
  return len;
}

bool StreamerSM::Error() const { return false; }

const char* StreamerSM::ErrorAsString() const { return "(none)"; }

bool StreamerSM::MessageFullyRead() const {
  if (is_request_) {
    return http_framer_->MessageFullyRead();
  } else {
    return false;
  }
}

void StreamerSM::Reset() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Reset";
  connection_->Cleanup("Server Reset");
  http_framer_->Reset();
}

void StreamerSM::ResetForNewConnection() {
  http_framer_->Reset();
  sm_other_interface_->Reset();
}

void StreamerSM::Cleanup() {
  if (is_request_)
    http_framer_->Reset();
}

int StreamerSM::PostAcceptHook() {
  if (!sm_other_interface_) {
    SMConnection* server_connection = SMConnection::NewSMConnection(
        epoll_server_, NULL, NULL, acceptor_, "server_conn: ");
    if (server_connection == NULL) {
      LOG(ERROR) << "StreamerSM: Could not create server conenction.";
      return 0;
    }
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Creating new server "
            << "connection.";
    sm_other_interface_ =
        new StreamerSM(server_connection, this, epoll_server_, acceptor_);
    sm_other_interface_->InitSMInterface(this, 0);
  }
  // The Streamer interface is used to stream HTTPS connections, so we
  // will always use the https_server_ip/port here.
  sm_other_interface_->InitSMConnection(NULL,
                                        sm_other_interface_,
                                        epoll_server_,
                                        -1,
                                        acceptor_->https_server_ip_,
                                        acceptor_->https_server_port_,
                                        std::string(),
                                        false);

  return 1;
}

size_t StreamerSM::SendSynStream(uint32_t stream_id,
                                 const BalsaHeaders& headers) {
  return 0;
}

size_t StreamerSM::SendSynReply(uint32_t stream_id,
                                const BalsaHeaders& headers) {
  return 0;
}

void StreamerSM::ProcessBodyInput(const char* input, size_t size) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT
          << "StreamerHttpSM: Process Body Input Data: "
          << "size " << size;
  sm_other_interface_->ProcessWriteInput(input, size);
}

void StreamerSM::MessageDone() {
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerHttpSM: MessageDone.";
    // TODO(kelindsay): anything need to be done ehre?
  } else {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StraemerHttpSM: MessageDone.";
  }
}

void StreamerSM::ProcessHeaders(const BalsaHeaders& headers) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpStreamerSM: Process Headers";
  BalsaHeaders mod_headers;
  mod_headers.CopyFrom(headers);
  if (forward_ip_header_.length()) {
    LOG(INFO) << "Adding forward header: " << forward_ip_header_;
    mod_headers.ReplaceOrAppendHeader(forward_ip_header_,
                                      connection_->client_ip());
  } else {
    LOG(INFO) << "NOT adding forward header.";
  }
  SimpleBuffer sb;
  char* buffer;
  int size;
  mod_headers.WriteHeaderAndEndingToBuffer(&sb);
  sb.GetReadablePtr(&buffer, &size);
  sm_other_interface_->ProcessWriteInput(buffer, size);
}

void StreamerSM::HandleHeaderError(BalsaFrame* framer) { HandleError(); }

void StreamerSM::HandleChunkingError(BalsaFrame* framer) { HandleError(); }

void StreamerSM::HandleBodyError(BalsaFrame* framer) { HandleError(); }

void StreamerSM::HandleError() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Error detected";
}

}  // namespace net
