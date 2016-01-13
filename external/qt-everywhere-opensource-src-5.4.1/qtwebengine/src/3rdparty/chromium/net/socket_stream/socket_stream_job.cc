// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_job.h"

#include "base/memory/singleton.h"
#include "net/http/transport_security_state.h"
#include "net/socket_stream/socket_stream_job_manager.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"

namespace net {

// static
SocketStreamJob::ProtocolFactory* SocketStreamJob::RegisterProtocolFactory(
    const std::string& scheme, ProtocolFactory* factory) {
  return SocketStreamJobManager::GetInstance()->RegisterProtocolFactory(
      scheme, factory);
}

// static
SocketStreamJob* SocketStreamJob::CreateSocketStreamJob(
    const GURL& url,
    SocketStream::Delegate* delegate,
    TransportSecurityState* sts,
    SSLConfigService* ssl,
    URLRequestContext* context,
    CookieStore* cookie_store) {
  GURL socket_url(url);
  if (url.scheme() == "ws" && sts &&
      sts->ShouldUpgradeToSSL(url.host(),
                              SSLConfigService::IsSNIAvailable(ssl))) {
    url::Replacements<char> replacements;
    static const char kNewScheme[] = "wss";
    replacements.SetScheme(kNewScheme, url::Component(0, strlen(kNewScheme)));
    socket_url = url.ReplaceComponents(replacements);
  }
  return SocketStreamJobManager::GetInstance()->CreateJob(
      socket_url, delegate, context, cookie_store);
}

SocketStreamJob::SocketStreamJob() {}

SocketStream::UserData* SocketStreamJob::GetUserData(const void* key) const {
  return socket_->GetUserData(key);
}

void SocketStreamJob::SetUserData(const void* key,
                                  SocketStream::UserData* data) {
  socket_->SetUserData(key, data);
}

void SocketStreamJob::Connect() {
  socket_->Connect();
}

bool SocketStreamJob::SendData(const char* data, int len) {
  return socket_->SendData(data, len);
}

void SocketStreamJob::Close() {
  socket_->Close();
}

void SocketStreamJob::RestartWithAuth(const AuthCredentials& credentials) {
  socket_->RestartWithAuth(credentials);
}

void SocketStreamJob::CancelWithError(int error) {
  socket_->CancelWithError(error);
}

void SocketStreamJob::CancelWithSSLError(const net::SSLInfo& ssl_info) {
  socket_->CancelWithSSLError(ssl_info);
}

void SocketStreamJob::ContinueDespiteError() {
  socket_->ContinueDespiteError();
}

void SocketStreamJob::DetachDelegate() {
  socket_->DetachDelegate();
}

void SocketStreamJob::DetachContext() {
  if (socket_.get())
    socket_->DetachContext();
}

SocketStreamJob::~SocketStreamJob() {}

}  // namespace net
