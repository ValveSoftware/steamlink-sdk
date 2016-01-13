// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_host_state.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_context.h"

const char kKeyName[] = "content_ssl_host_state";

namespace content {

SSLHostState* SSLHostState::GetFor(BrowserContext* context) {
  SSLHostState* rv = static_cast<SSLHostState*>(context->GetUserData(kKeyName));
  if (!rv) {
    rv = new SSLHostState();
    context->SetUserData(kKeyName, rv);
  }
  return rv;
}

SSLHostState::SSLHostState() {
}

SSLHostState::~SSLHostState() {
}

void SSLHostState::HostRanInsecureContent(const std::string& host, int pid) {
  DCHECK(CalledOnValidThread());
  ran_insecure_content_hosts_.insert(BrokenHostEntry(host, pid));
}

bool SSLHostState::DidHostRunInsecureContent(const std::string& host,
                                             int pid) const {
  DCHECK(CalledOnValidThread());
  return !!ran_insecure_content_hosts_.count(BrokenHostEntry(host, pid));
}

void SSLHostState::DenyCertForHost(net::X509Certificate* cert,
                                   const std::string& host,
                                   net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  cert_policy_for_host_[host].Deny(cert, error);
}

void SSLHostState::AllowCertForHost(net::X509Certificate* cert,
                                    const std::string& host,
                                    net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  cert_policy_for_host_[host].Allow(cert, error);
}

void SSLHostState::Clear() {
  DCHECK(CalledOnValidThread());

  cert_policy_for_host_.clear();
}

net::CertPolicy::Judgment SSLHostState::QueryPolicy(net::X509Certificate* cert,
                                                    const std::string& host,
                                                    net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  return cert_policy_for_host_[host].Check(cert, error);
}

}  // namespace content
