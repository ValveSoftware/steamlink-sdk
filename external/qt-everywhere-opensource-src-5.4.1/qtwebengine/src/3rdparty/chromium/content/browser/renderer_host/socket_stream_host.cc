// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/socket_stream_host.h"

#include "base/logging.h"
#include "content/common/socket_stream.h"
#include "content/public/browser/content_browser_client.h"
#include "net/socket_stream/socket_stream_job.h"
#include "net/url_request/url_request_context.h"

namespace content {
namespace {

const char* kSocketIdKey = "socketId";

class SocketStreamId : public net::SocketStream::UserData {
 public:
  explicit SocketStreamId(int socket_id) : socket_id_(socket_id) {}
  virtual ~SocketStreamId() {}
  int socket_id() const { return socket_id_; }

 private:
  int socket_id_;
};

}  // namespace

SocketStreamHost::SocketStreamHost(
    net::SocketStream::Delegate* delegate,
    int child_id,
    int render_frame_id,
    int socket_id)
    : delegate_(delegate),
      child_id_(child_id),
      render_frame_id_(render_frame_id),
      socket_id_(socket_id) {
  DCHECK_NE(socket_id_, kNoSocketId);
  VLOG(1) << "SocketStreamHost: render_frame_id=" << render_frame_id
          << " socket_id=" << socket_id_;
}

/* static */
int SocketStreamHost::SocketIdFromSocketStream(
    const net::SocketStream* socket) {
  net::SocketStream::UserData* d = socket->GetUserData(kSocketIdKey);
  if (d) {
    SocketStreamId* socket_stream_id = static_cast<SocketStreamId*>(d);
    return socket_stream_id->socket_id();
  }
  return kNoSocketId;
}

SocketStreamHost::~SocketStreamHost() {
  VLOG(1) << "SocketStreamHost destructed socket_id=" << socket_id_;
  job_->DetachContext();
  job_->DetachDelegate();
}

void SocketStreamHost::Connect(const GURL& url,
                               net::URLRequestContext* request_context) {
  VLOG(1) << "SocketStreamHost::Connect url=" << url;
  job_ = net::SocketStreamJob::CreateSocketStreamJob(
      url, delegate_, request_context->transport_security_state(),
      request_context->ssl_config_service(),
      request_context,
      GetContentClient()->browser()->OverrideCookieStoreForRenderProcess(
          child_id_));
  job_->SetUserData(kSocketIdKey, new SocketStreamId(socket_id_));
  job_->Connect();
}

bool SocketStreamHost::SendData(const std::vector<char>& data) {
  VLOG(1) << "SocketStreamHost::SendData";
  return job_.get() && job_->SendData(&data[0], data.size());
}

void SocketStreamHost::Close() {
  VLOG(1) << "SocketStreamHost::Close";
  if (!job_.get())
    return;
  job_->Close();
}

void SocketStreamHost::CancelWithError(int error) {
  VLOG(1) << "SocketStreamHost::CancelWithError: error=" << error;
  if (!job_.get())
    return;
  job_->CancelWithError(error);
}

void SocketStreamHost::CancelWithSSLError(const net::SSLInfo& ssl_info) {
  VLOG(1) << "SocketStreamHost::CancelWithSSLError";
  if (!job_.get())
    return;
  job_->CancelWithSSLError(ssl_info);
}

void SocketStreamHost::ContinueDespiteError() {
  VLOG(1) << "SocketStreamHost::ContinueDespiteError";
  if (!job_.get())
    return;
  job_->ContinueDespiteError();
}

}  // namespace content
