// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/http_interface.h"

#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/dump_cache/url_utilities.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/spdy_util.h"

namespace net {

HttpSM::HttpSM(SMConnection* connection,
               SMInterface* sm_spdy_interface,
               MemoryCache* memory_cache,
               FlipAcceptor* acceptor)
    : http_framer_(new BalsaFrame),
      stream_id_(0),
      server_idx_(-1),
      connection_(connection),
      sm_spdy_interface_(sm_spdy_interface),
      output_list_(connection->output_list()),
      output_ordering_(connection),
      memory_cache_(connection->memory_cache()),
      acceptor_(acceptor) {
  http_framer_->set_balsa_visitor(this);
  http_framer_->set_balsa_headers(&headers_);
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY)
    http_framer_->set_is_request(false);
}
HttpSM::~HttpSM() {
  Reset();
  delete http_framer_;
}

void HttpSM::ProcessBodyData(const char* input, size_t size) {
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process Body Data: stream "
            << stream_id_ << ": size " << size;
    sm_spdy_interface_->SendDataFrame(stream_id_, input, size, 0, false);
  }
}

void HttpSM::ProcessHeaders(const BalsaHeaders& headers) {
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
    std::string host =
        UrlUtilities::GetUrlHost(headers.GetHeader("Host").as_string());
    std::string method = headers.request_method().as_string();
    VLOG(1) << ACCEPTOR_CLIENT_IDENT
            << "Received Request: " << headers.request_uri().as_string() << " "
            << method;
    std::string filename =
        EncodeURL(headers.request_uri().as_string(), host, method);
    NewStream(stream_id_, 0, filename);
    stream_id_ += 2;
  } else {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Received Response from "
            << connection_->server_ip_ << ":" << connection_->server_port_
            << " ";
    sm_spdy_interface_->SendSynReply(stream_id_, headers);
  }
}

void HttpSM::MessageDone() {
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: MessageDone. Sending EOF: "
            << "stream " << stream_id_;
    sm_spdy_interface_->SendEOF(stream_id_);
  } else {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: MessageDone.";
  }
}

void HttpSM::HandleHeaderError(BalsaFrame* framer) { HandleError(); }

void HttpSM::HandleChunkingError(BalsaFrame* framer) { HandleError(); }

void HttpSM::HandleBodyError(BalsaFrame* framer) { HandleError(); }

void HttpSM::HandleError() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Error detected";
}

void HttpSM::AddToOutputOrder(const MemCacheIter& mci) {
  output_ordering_.AddToOutputOrder(mci);
}

void HttpSM::InitSMInterface(SMInterface* sm_spdy_interface, int32 server_idx) {
  sm_spdy_interface_ = sm_spdy_interface;
  server_idx_ = server_idx;
}

void HttpSM::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                              SMInterface* sm_interface,
                              EpollServer* epoll_server,
                              int fd,
                              std::string server_ip,
                              std::string server_port,
                              std::string remote_ip,
                              bool use_ssl) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Initializing server "
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

size_t HttpSM::ProcessReadInput(const char* data, size_t len) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process read input: stream "
          << stream_id_;
  return http_framer_->ProcessInput(data, len);
}

size_t HttpSM::ProcessWriteInput(const char* data, size_t len) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process write input: size "
          << len << ": stream " << stream_id_;
  char* dataPtr = new char[len];
  memcpy(dataPtr, data, len);
  DataFrame* data_frame = new DataFrame;
  data_frame->data = dataPtr;
  data_frame->size = len;
  data_frame->delete_when_done = true;
  connection_->EnqueueDataFrame(data_frame);
  return len;
}

bool HttpSM::MessageFullyRead() const {
  return http_framer_->MessageFullyRead();
}

void HttpSM::SetStreamID(uint32 stream_id) { stream_id_ = stream_id; }

bool HttpSM::Error() const { return http_framer_->Error(); }

const char* HttpSM::ErrorAsString() const {
  return BalsaFrameEnums::ErrorCodeToString(http_framer_->ErrorCode());
}

void HttpSM::Reset() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Reset: stream " << stream_id_;
  http_framer_->Reset();
}

void HttpSM::ResetForNewConnection() {
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Server connection closing "
            << "to: " << connection_->server_ip_ << ":"
            << connection_->server_port_ << " ";
  }
  // Message has not been fully read, either it is incomplete or the
  // server is closing the connection to signal message end.
  if (!MessageFullyRead()) {
    VLOG(2) << "HTTP response closed before end of file detected. "
            << "Sending EOF to spdy.";
    sm_spdy_interface_->SendEOF(stream_id_);
  }
  output_ordering_.Reset();
  http_framer_->Reset();
  if (sm_spdy_interface_) {
    sm_spdy_interface_->ResetForNewInterface(server_idx_);
  }
}

void HttpSM::Cleanup() {
  if (!(acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER)) {
    VLOG(2) << "HttpSM Request Fully Read; stream_id: " << stream_id_;
    connection_->Cleanup("request complete");
  }
}

int HttpSM::PostAcceptHook() { return 1; }

void HttpSM::NewStream(uint32 stream_id,
                       uint32 priority,
                       const std::string& filename) {
  MemCacheIter mci;
  mci.stream_id = stream_id;
  mci.priority = priority;
  if (!memory_cache_->AssignFileData(filename, &mci)) {
    // error creating new stream.
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending ErrorNotFound";
    SendErrorNotFound(stream_id);
  } else {
    AddToOutputOrder(mci);
  }
}

void HttpSM::SendEOF(uint32 stream_id) {
  SendEOFImpl(stream_id);
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    sm_spdy_interface_->ResetForNewInterface(server_idx_);
  }
}

void HttpSM::SendErrorNotFound(uint32 stream_id) {
  SendErrorNotFoundImpl(stream_id);
}

size_t HttpSM::SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
  return 0;
}

size_t HttpSM::SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
  return SendSynReplyImpl(stream_id, headers);
}

void HttpSM::SendDataFrame(uint32 stream_id,
                           const char* data,
                           int64 len,
                           uint32 flags,
                           bool compress) {
  SendDataFrameImpl(stream_id, data, len, flags, compress);
}

void HttpSM::SendEOFImpl(uint32 stream_id) {
  DataFrame* df = new DataFrame;
  df->data = "0\r\n\r\n";
  df->size = 5;
  df->delete_when_done = false;
  EnqueueDataFrame(df);
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
    Reset();
  }
}

void HttpSM::SendErrorNotFoundImpl(uint32 stream_id) {
  BalsaHeaders my_headers;
  my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
  my_headers.RemoveAllOfHeader("content-length");
  my_headers.AppendHeader("transfer-encoding", "chunked");
  SendSynReplyImpl(stream_id, my_headers);
  SendDataFrame(stream_id, "page not found", 14, 0, false);
  SendEOFImpl(stream_id);
  output_ordering_.RemoveStreamId(stream_id);
}

size_t HttpSM::SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
  SimpleBuffer sb;
  headers.WriteHeaderAndEndingToBuffer(&sb);
  DataFrame* df = new DataFrame;
  df->size = sb.ReadableBytes();
  char* buffer = new char[df->size];
  df->data = buffer;
  df->delete_when_done = true;
  sb.Read(buffer, df->size);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending HTTP Reply header "
          << stream_id_;
  size_t df_size = df->size;
  EnqueueDataFrame(df);
  return df_size;
}

size_t HttpSM::SendSynStreamImpl(uint32 stream_id,
                                 const BalsaHeaders& headers) {
  SimpleBuffer sb;
  headers.WriteHeaderAndEndingToBuffer(&sb);
  DataFrame* df = new DataFrame;
  df->size = sb.ReadableBytes();
  char* buffer = new char[df->size];
  df->data = buffer;
  df->delete_when_done = true;
  sb.Read(buffer, df->size);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending HTTP Reply header "
          << stream_id_;
  size_t df_size = df->size;
  EnqueueDataFrame(df);
  return df_size;
}

void HttpSM::SendDataFrameImpl(uint32 stream_id,
                               const char* data,
                               int64 len,
                               uint32 flags,
                               bool compress) {
  char chunk_buf[128];
  snprintf(chunk_buf, sizeof(chunk_buf), "%x\r\n", (unsigned int)len);
  std::string chunk_description(chunk_buf);
  DataFrame* df = new DataFrame;
  df->size = chunk_description.size() + len + 2;
  char* buffer = new char[df->size];
  df->data = buffer;
  df->delete_when_done = true;
  memcpy(buffer, chunk_description.data(), chunk_description.size());
  memcpy(buffer + chunk_description.size(), data, len);
  memcpy(buffer + chunk_description.size() + len, "\r\n", 2);
  EnqueueDataFrame(df);
}

void HttpSM::EnqueueDataFrame(DataFrame* df) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Enqueue data frame: stream "
          << stream_id_;
  connection_->EnqueueDataFrame(df);
}

void HttpSM::GetOutput() {
  MemCacheIter* mci = output_ordering_.GetIter();
  if (mci == NULL) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput: nothing to "
            << "output!?: stream " << stream_id_;
    return;
  }
  if (!mci->transformed_header) {
    mci->bytes_sent =
        SendSynReply(mci->stream_id, *(mci->file_data->headers()));
    mci->transformed_header = true;
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput transformed "
            << "header stream_id: [" << mci->stream_id << "]";
    return;
  }
  if (mci->body_bytes_consumed >= mci->file_data->body().size()) {
    SendEOF(mci->stream_id);
    output_ordering_.RemoveStreamId(mci->stream_id);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "GetOutput remove_stream_id: ["
            << mci->stream_id << "]";
    return;
  }
  size_t num_to_write =
      mci->file_data->body().size() - mci->body_bytes_consumed;
  if (num_to_write > mci->max_segment_size)
    num_to_write = mci->max_segment_size;

  SendDataFrame(mci->stream_id,
                mci->file_data->body().data() + mci->body_bytes_consumed,
                num_to_write,
                0,
                true);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput SendDataFrame["
          << mci->stream_id << "]: " << num_to_write;
  mci->body_bytes_consumed += num_to_write;
  mci->bytes_sent += num_to_write;
}

}  // namespace net
