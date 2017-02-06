// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/spdy_interface.h"

#include <algorithm>
#include <string>
#include <utility>

#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/http_interface.h"
#include "net/tools/flip_server/spdy_util.h"
#include "net/tools/flip_server/url_utilities.h"

namespace net {

// static
std::string SpdySM::forward_ip_header_;

class SpdyFrameDataFrame : public DataFrame {
 public:
  explicit SpdyFrameDataFrame(SpdySerializedFrame* spdy_frame)
      : frame(spdy_frame) {
    data = spdy_frame->data();
    size = spdy_frame->size();
  }

  ~SpdyFrameDataFrame() override { delete frame; }

  const SpdySerializedFrame* frame;
};

SpdySM::SpdySM(SMConnection* connection,
               SMInterface* sm_http_interface,
               EpollServer* epoll_server,
               MemoryCache* memory_cache,
               FlipAcceptor* acceptor,
               SpdyMajorVersion spdy_version)
    : buffered_spdy_framer_(new BufferedSpdyFramer(spdy_version)),
      valid_spdy_session_(false),
      connection_(connection),
      client_output_list_(connection->output_list()),
      client_output_ordering_(connection),
      next_outgoing_stream_id_(2),
      epoll_server_(epoll_server),
      acceptor_(acceptor),
      memory_cache_(memory_cache),
      close_on_error_(false) {
  buffered_spdy_framer_->set_visitor(this);
}

SpdySM::~SpdySM() { }

void SpdySM::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                              SMInterface* sm_interface,
                              EpollServer* epoll_server,
                              int fd,
                              std::string server_ip,
                              std::string server_port,
                              std::string remote_ip,
                              bool use_ssl) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Initializing server connection.";
  connection_->InitSMConnection(connection_pool,
                                sm_interface,
                                epoll_server,
                                fd,
                                server_ip,
                                server_port,
                                remote_ip,
                                use_ssl);
}

SMInterface* SpdySM::NewConnectionInterface() {
  SMConnection* server_connection =
      SMConnection::NewSMConnection(epoll_server_,
                                    NULL,
                                    memory_cache_,
                                    acceptor_,
                                    "http_conn: ");
  if (server_connection == NULL) {
    LOG(ERROR) << "SpdySM: Could not create server connection";
    return NULL;
  }
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Creating new HTTP interface";
  SMInterface* sm_http_interface =
      new HttpSM(server_connection, this, memory_cache_, acceptor_);
  return sm_http_interface;
}

SMInterface* SpdySM::FindOrMakeNewSMConnectionInterface(
    const std::string& server_ip,
    const std::string& server_port) {
  SMInterface* sm_http_interface;
  int32_t server_idx;
  if (unused_server_interface_list.empty()) {
    sm_http_interface = NewConnectionInterface();
    server_idx = server_interface_list.size();
    server_interface_list.push_back(sm_http_interface);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT
            << "SpdySM: Making new server connection on index: " << server_idx;
  } else {
    server_idx = unused_server_interface_list.back();
    unused_server_interface_list.pop_back();
    sm_http_interface = server_interface_list.at(server_idx);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Reusing connection on "
            << "index: " << server_idx;
  }

  sm_http_interface->InitSMInterface(this, server_idx);
  sm_http_interface->InitSMConnection(NULL,
                                      sm_http_interface,
                                      epoll_server_,
                                      -1,
                                      server_ip,
                                      server_port,
                                      std::string(),
                                      false);

  return sm_http_interface;
}

int SpdySM::SpdyHandleNewStream(SpdyStreamId stream_id,
                                SpdyPriority priority,
                                const SpdyHeaderBlock& headers,
                                std::string& http_data,
                                bool* is_https_scheme) {
  *is_https_scheme = false;
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSyn(" << stream_id << ")";
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: # headers: " << headers.size();

  SpdyHeaderBlock::const_iterator method = headers.end();
  SpdyHeaderBlock::const_iterator host = headers.end();
  SpdyHeaderBlock::const_iterator path = headers.end();
  SpdyHeaderBlock::const_iterator scheme = headers.end();
  SpdyHeaderBlock::const_iterator version = headers.end();
  SpdyHeaderBlock::const_iterator url = headers.end();

  std::string path_string, host_string, version_string;

  method = headers.find(":method");
  host = headers.find(":host");
  path = headers.find(":path");
  scheme = headers.find(":scheme");
  if (method == headers.end() || host == headers.end() ||
      path == headers.end() || scheme == headers.end()) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: A mandatory header is "
            << "missing. Not creating stream";
    return 0;
  }
  host_string = host->second.as_string();
  path_string = path->second.as_string();
  version_string = "HTTP/1.1";

  if (scheme->second.compare("https") == 0) {
    *is_https_scheme = true;
  }

  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second
            << " " << path_string;
    std::string filename =
        EncodeURL(path_string, host_string, method->second.as_string());
    NewStream(stream_id, priority, filename);
  } else {
    http_data += method->second.as_string() + " " + path_string + " " +
                 version_string + "\r\n";
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second << " "
            << path_string << " " << version_string;
    http_data += "Host: " + (*is_https_scheme ?
                             acceptor_->https_server_ip_ :
                             acceptor_->http_server_ip_) + "\r\n";
    for (SpdyHeaderBlock::const_iterator i = headers.begin();
         i != headers.end(); ++i) {
      if ((i->first.size() > 0 && i->first[0] == ':') ||
          i->first == "host" ||
          i == method ||
          i == host ||
          i == path ||
          i == scheme ||
          i == version ||
          i == url) {
        // Ignore the entry.
      } else {
        http_data +=
            i->first.as_string() + ": " + i->second.as_string() + "\r\n";
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << i->first << ":" << i->second;
      }
    }
    if (forward_ip_header_.length()) {
      // X-Client-Cluster-IP header
      http_data += forward_ip_header_ + ": " +
          connection_->client_ip() + "\r\n";
    }
    http_data += "\r\n";
  }

  VLOG(3) << ACCEPTOR_CLIENT_IDENT << "SpdySM: HTTP Request:\n" << http_data;
  return 1;
}

void SpdySM::OnStreamFrameData(SpdyStreamId stream_id,
                               const char* data,
                               size_t len) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: StreamData(" << stream_id
          << ", [" << len << "])";
  StreamToSmif::iterator it = stream_to_smif_.find(stream_id);
  if (it == stream_to_smif_.end()) {
    VLOG(2) << "Dropping frame from unknown stream " << stream_id;
    if (!valid_spdy_session_)
      close_on_error_ = true;
    return;
  }

  SMInterface* interface = it->second;
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY)
    interface->ProcessWriteInput(data, len);
}

void SpdySM::OnStreamEnd(SpdyStreamId stream_id) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: StreamEnd(" << stream_id << ")";
  StreamToSmif::iterator it = stream_to_smif_.find(stream_id);
  if (it == stream_to_smif_.end()) {
    VLOG(2) << "Dropping frame from unknown stream " << stream_id;
    if (!valid_spdy_session_)
      close_on_error_ = true;
    return;
  }

  SMInterface* interface = it->second;
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY)
    interface->ProcessWriteInput(nullptr, 0);
}

void SpdySM::OnStreamPadding(SpdyStreamId stream_id, size_t len) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: StreamPadding(" << stream_id
          << ", [" << len << "])";
}

void SpdySM::OnSynStream(SpdyStreamId stream_id,
                         SpdyStreamId associated_stream_id,
                         SpdyPriority priority,
                         bool fin,
                         bool unidirectional,
                         const SpdyHeaderBlock& headers) {
  std::string http_data;
  bool is_https_scheme;
  int ret = SpdyHandleNewStream(
      stream_id, priority, headers, http_data, &is_https_scheme);
  if (!ret) {
    LOG(ERROR) << "SpdySM: Could not convert spdy into http.";
    return;
  }
  // We've seen a valid looking SYN_STREAM, consider this to have
  // been a real spdy session.
  valid_spdy_session_ = true;

  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    std::string server_ip;
    std::string server_port;
    if (is_https_scheme) {
      server_ip = acceptor_->https_server_ip_;
      server_port = acceptor_->https_server_port_;
    } else {
      server_ip = acceptor_->http_server_ip_;
      server_port = acceptor_->http_server_port_;
    }
    SMInterface* sm_http_interface =
        FindOrMakeNewSMConnectionInterface(server_ip, server_port);
    stream_to_smif_[stream_id] = sm_http_interface;
    sm_http_interface->SetStreamID(stream_id);
    sm_http_interface->ProcessWriteInput(http_data.c_str(), http_data.size());
  }
}

void SpdySM::OnSynReply(SpdyStreamId stream_id,
                        bool fin,
                        const SpdyHeaderBlock& headers) {
  // TODO(willchan): if there is an error parsing headers, we
  // should send a RST_STREAM.
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSynReply(" << stream_id << ")";
}

void SpdySM::OnHeaders(SpdyStreamId stream_id,
                       bool has_priority,
                       int weight,
                       SpdyStreamId parent_stream_id,
                       bool exclusive,
                       bool fin,
                       const SpdyHeaderBlock& headers) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnHeaders(" << stream_id << ")";
}

void SpdySM::OnRstStream(SpdyStreamId stream_id, SpdyRstStreamStatus status) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnRstStream(" << stream_id
          << ")";
  client_output_ordering_.RemoveStreamId(stream_id);
}

bool SpdySM::OnUnknownFrame(SpdyStreamId stream_id, int frame_type) {
  return false;
}

size_t SpdySM::ProcessReadInput(const char* data, size_t len) {
  DCHECK(buffered_spdy_framer_);
  return buffered_spdy_framer_->ProcessInput(data, len);
}

size_t SpdySM::ProcessWriteInput(const char* data, size_t len) { return 0; }

bool SpdySM::MessageFullyRead() const {
  DCHECK(buffered_spdy_framer_);
  return buffered_spdy_framer_->MessageFullyRead();
}

bool SpdySM::Error() const {
  DCHECK(buffered_spdy_framer_);
  return close_on_error_ || buffered_spdy_framer_->HasError();
}

const char* SpdySM::ErrorAsString() const {
  DCHECK(Error());
  DCHECK(buffered_spdy_framer_);
  return SpdyFramer::ErrorCodeToString(buffered_spdy_framer_->error_code());
}

void SpdySM::ResetForNewInterface(int32_t server_idx) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Reset for new interface: "
          << "server_idx: " << server_idx;
  unused_server_interface_list.push_back(server_idx);
}

void SpdySM::ResetForNewConnection() {
  // seq_num is not cleared, intentionally.
  buffered_spdy_framer_.reset();
  valid_spdy_session_ = false;
  client_output_ordering_.Reset();
  next_outgoing_stream_id_ = 2;
}

// Send a settings frame
int SpdySM::PostAcceptHook() {
  // We should have buffered_spdy_framer_ set after reuse
  DCHECK(buffered_spdy_framer_);
  SettingsMap settings;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, 100);
  SpdySerializedFrame* settings_frame =
      buffered_spdy_framer_->CreateSettings(settings);

  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending Settings Frame";
  EnqueueDataFrame(new SpdyFrameDataFrame(settings_frame));
  return 1;
}

void SpdySM::NewStream(uint32_t stream_id,
                       uint32_t priority,
                       const std::string& filename) {
  MemCacheIter mci;
  mci.stream_id = stream_id;
  mci.priority = priority;
  // TODO(yhirano): The program will crash when
  // acceptor_->flip_handler_type_ != FLIP_HANDLER_SPDY_SERVER.
  // It should be fixed or an assertion should be placed.
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
    if (!memory_cache_->AssignFileData(filename, &mci)) {
      // error creating new stream.
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending ErrorNotFound";
      SendErrorNotFound(stream_id);
    } else {
      AddToOutputOrder(mci);
    }
  } else {
    AddToOutputOrder(mci);
  }
}

void SpdySM::AddToOutputOrder(const MemCacheIter& mci) {
  client_output_ordering_.AddToOutputOrder(mci);
}

void SpdySM::SendEOF(uint32_t stream_id) {
  SendEOFImpl(stream_id);
}

void SpdySM::SendErrorNotFound(uint32_t stream_id) {
  SendErrorNotFoundImpl(stream_id);
}

size_t SpdySM::SendSynStream(uint32_t stream_id, const BalsaHeaders& headers) {
  return SendSynStreamImpl(stream_id, headers);
}

size_t SpdySM::SendSynReply(uint32_t stream_id, const BalsaHeaders& headers) {
  return SendSynReplyImpl(stream_id, headers);
}

void SpdySM::SendDataFrame(uint32_t stream_id,
                           const char* data,
                           int64_t len,
                           uint32_t flags,
                           bool compress) {
  SpdyDataFlags spdy_flags = static_cast<SpdyDataFlags>(flags);
  SendDataFrameImpl(stream_id, data, len, spdy_flags, compress);
}

void SpdySM::SendEOFImpl(uint32_t stream_id) {
  SendDataFrame(stream_id, NULL, 0, DATA_FLAG_FIN, false);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending EOF: " << stream_id;
  KillStream(stream_id);
  stream_to_smif_.erase(stream_id);
}

void SpdySM::SendErrorNotFoundImpl(uint32_t stream_id) {
  BalsaHeaders my_headers;
  my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
  SendSynReplyImpl(stream_id, my_headers);
  SendDataFrame(stream_id, "wtf?", 4, DATA_FLAG_FIN, false);
  client_output_ordering_.RemoveStreamId(stream_id);
}

void SpdySM::KillStream(uint32_t stream_id) {
  client_output_ordering_.RemoveStreamId(stream_id);
}

void SpdySM::CopyHeaders(SpdyHeaderBlock& dest, const BalsaHeaders& headers) {
  for (BalsaHeaders::const_header_lines_iterator hi =
           headers.header_lines_begin();
       hi != headers.header_lines_end();
       ++hi) {
    // It is illegal to send SPDY headers with empty value or header
    // names.
    if (!hi->first.length() || !hi->second.length())
      continue;

    // Key must be all lower case in SPDY headers.
    std::string key = hi->first.as_string();
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    SpdyHeaderBlock::iterator fhi = dest.find(key);
    if (fhi == dest.end()) {
      dest[key] = hi->second.as_string();
    } else {
      dest[key] = (std::string(fhi->second.data(), fhi->second.size()) + "\0" +
                   std::string(hi->second.data(), hi->second.size()));
    }
  }

  // These headers have no value
  dest.erase("X-Associated-Content");  // TODO(mbelshe): case-sensitive
  dest.erase("X-Original-Url");        // TODO(mbelshe): case-sensitive
}

size_t SpdySM::SendSynStreamImpl(uint32_t stream_id,
                                 const BalsaHeaders& headers) {
  SpdyHeaderBlock block;
  CopyHeaders(block, headers);
  block[":method"] = headers.request_method().as_string();
  block[":version"] = headers.request_version().as_string();
  if (headers.HasHeader("X-Original-Url")) {
    std::string original_url = headers.GetHeader("X-Original-Url").as_string();
    block[":path"] = UrlUtilities::GetUrlPath(original_url);
    block[":host"] = UrlUtilities::GetUrlPath(original_url);
  } else {
    block[":path"] = headers.request_uri().as_string();
    if (block.find("host") != block.end()) {
      block[":host"] = headers.GetHeader("Host").as_string();
      block.erase("host");
    }
  }

  DCHECK(buffered_spdy_framer_);
  SpdySerializedFrame* fsrcf = buffered_spdy_framer_->CreateSynStream(
      stream_id, 0, 0, CONTROL_FLAG_NONE, std::move(block));
  size_t df_size = fsrcf->size();
  EnqueueDataFrame(new SpdyFrameDataFrame(fsrcf));

  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynStreamheader "
          << stream_id;
  return df_size;
}

size_t SpdySM::SendSynReplyImpl(uint32_t stream_id,
                                const BalsaHeaders& headers) {
  SpdyHeaderBlock block;
  CopyHeaders(block, headers);
  block[":status"] = headers.response_code().as_string() + " " +
                     headers.response_reason_phrase().as_string();
  block[":version"] = headers.response_version().as_string();

  DCHECK(buffered_spdy_framer_);
  SpdySerializedFrame* fsrcf = buffered_spdy_framer_->CreateSynReply(
      stream_id, CONTROL_FLAG_NONE, std::move(block));
  size_t df_size = fsrcf->size();
  EnqueueDataFrame(new SpdyFrameDataFrame(fsrcf));

  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynReplyheader "
          << stream_id;
  return df_size;
}

void SpdySM::SendDataFrameImpl(uint32_t stream_id,
                               const char* data,
                               int64_t len,
                               SpdyDataFlags flags,
                               bool compress) {
  DCHECK(buffered_spdy_framer_);
  // TODO(mbelshe):  We can't compress here - before going into the
  //                 priority queue.  Compression needs to be done
  //                 with late binding.
  if (len == 0) {
    SpdySerializedFrame* fdf =
        buffered_spdy_framer_->CreateDataFrame(stream_id, data, len, flags);
    EnqueueDataFrame(new SpdyFrameDataFrame(fdf));
    return;
  }

  // Chop data frames into chunks so that one stream can't monopolize the
  // output channel.
  while (len > 0) {
    int64_t size = std::min(len, static_cast<int64_t>(kSpdySegmentSize));
    SpdyDataFlags chunk_flags = flags;

    // If we chunked this block, and the FIN flag was set, there is more
    // data coming.  So, remove the flag.
    if ((size < len) && (flags & DATA_FLAG_FIN))
      chunk_flags = static_cast<SpdyDataFlags>(chunk_flags & ~DATA_FLAG_FIN);

    SpdySerializedFrame* fdf = buffered_spdy_framer_->CreateDataFrame(
        stream_id, data, size, chunk_flags);
    EnqueueDataFrame(new SpdyFrameDataFrame(fdf));

    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending data frame "
            << stream_id << " [" << size << "] shrunk to "
            << (fdf->size() - kSpdyOverhead) << ", flags=" << flags;

    data += size;
    len -= size;
  }
}

void SpdySM::EnqueueDataFrame(DataFrame* df) {
  connection_->EnqueueDataFrame(df);
}

void SpdySM::GetOutput() {
  while (client_output_list_->size() < 2) {
    MemCacheIter* mci = client_output_ordering_.GetIter();
    if (mci == NULL) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT
              << "SpdySM: GetOutput: nothing to output!?";
      return;
    }
    if (!mci->transformed_header) {
      mci->transformed_header = true;
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput transformed "
              << "header stream_id: [" << mci->stream_id << "]";
      if ((mci->stream_id % 2) == 0) {
        // this is a server initiated stream.
        // Ideally, we'd do a 'syn-push' here, instead of a syn-reply.
        BalsaHeaders headers;
        headers.CopyFrom(*(mci->file_data->headers()));
        headers.ReplaceOrAppendHeader("status", "200");
        headers.ReplaceOrAppendHeader("version", "http/1.1");
        headers.SetRequestFirstlineFromStringPieces(
            "PUSH", mci->file_data->filename(), "");
        mci->bytes_sent = SendSynStream(mci->stream_id, headers);
      } else {
        BalsaHeaders headers;
        headers.CopyFrom(*(mci->file_data->headers()));
        mci->bytes_sent = SendSynReply(mci->stream_id, headers);
      }
      return;
    }
    if (mci->body_bytes_consumed >= mci->file_data->body().size()) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput "
              << "remove_stream_id: [" << mci->stream_id << "]";
      SendEOF(mci->stream_id);
      return;
    }
    size_t num_to_write =
        mci->file_data->body().size() - mci->body_bytes_consumed;
    if (num_to_write > mci->max_segment_size)
      num_to_write = mci->max_segment_size;

    bool should_compress = false;
    if (!mci->file_data->headers()->HasHeader("content-encoding")) {
      if (mci->file_data->headers()->HasHeader("content-type")) {
        std::string content_type =
            mci->file_data->headers()->GetHeader("content-type").as_string();
        if (content_type.find("image") == content_type.npos)
          should_compress = true;
      }
    }

    SendDataFrame(mci->stream_id,
                  mci->file_data->body().data() + mci->body_bytes_consumed,
                  num_to_write,
                  0,
                  should_compress);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput SendDataFrame["
            << mci->stream_id << "]: " << num_to_write;
    mci->body_bytes_consumed += num_to_write;
    mci->bytes_sent += num_to_write;
  }
}

void SpdySM::CreateFramer(SpdyMajorVersion spdy_version) {
  DCHECK(!buffered_spdy_framer_);
  buffered_spdy_framer_.reset(new BufferedSpdyFramer(spdy_version));
  buffered_spdy_framer_->set_visitor(this);
}

}  // namespace net
