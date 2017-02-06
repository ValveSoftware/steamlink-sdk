// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/common/chrome_content_client.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace extensions {

// Class verifies certificate by its fingerprint received using different
// channel. It's the only know information about device with self-signed
// certificate.
class PrivetV3ContextGetter::CertVerifier : public net::CertVerifier {
 public:
  CertVerifier() {}

  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::BoundNetLog& net_log) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();

    // Because no trust anchor checking is being performed, don't indicate that
    // it came from an OS-trusted root.
    verify_result->is_issued_by_known_root = false;
    // Because no trust anchor checking is being performed, don't indicate that
    // it came from a supplemental trust anchor.
    verify_result->is_issued_by_additional_trust_anchor = false;
    // Because no name checking is being performed, don't indicate that it the
    // common name was used.
    verify_result->common_name_fallback_used = false;
    // Because the signature is not checked, do not indicate any deprecated
    // signature algorithms were used, even if they might be present.
    verify_result->has_md2 = false;
    verify_result->has_md4 = false;
    verify_result->has_md5 = false;
    verify_result->has_sha1 = false;
    verify_result->has_sha1_leaf = false;
    // Because no chain hashes calculation is being performed, keep hashes
    // container clean.
    verify_result->public_key_hashes.clear();

    verify_result->cert_status =
        CheckFingerprint(params.certificate(), params.hostname())
            ? 0
            : net::CERT_STATUS_AUTHORITY_INVALID;
    return net::IsCertStatusError(verify_result->cert_status)
               ? net::MapCertStatusToNetError(verify_result->cert_status)
               : net::OK;
  }

  void AddPairedHost(const std::string& host,
                     const net::SHA256HashValue& certificate_fingerprint) {
    fingerprints_[host] = certificate_fingerprint;
  }

 private:
  bool CheckFingerprint(const scoped_refptr<net::X509Certificate>& cert,
                        const std::string& hostname) const {
    auto it = fingerprints_.find(hostname);
    if (it == fingerprints_.end())
      return false;

    return it->second == net::X509Certificate::CalculateFingerprint256(
                             cert->os_cert_handle());
  }

  std::map<std::string, net::SHA256HashValue> fingerprints_;

  DISALLOW_COPY_AND_ASSIGN(CertVerifier);
};

PrivetV3ContextGetter::PrivetV3ContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& net_task_runner)
    : net_task_runner_(net_task_runner), weak_ptr_factory_(this) {
}

net::URLRequestContext* PrivetV3ContextGetter::GetURLRequestContext() {
  InitOnNetThread();
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
PrivetV3ContextGetter::GetNetworkTaskRunner() const {
  return net_task_runner_;
}

void PrivetV3ContextGetter::InitOnNetThread() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
  if (!context_) {
    net::URLRequestContextBuilder builder;

    builder.set_proxy_service(net::ProxyService::CreateDirect());
    builder.SetSpdyAndQuicEnabled(false, false);
    builder.DisableHttpCache();
    cert_verifier_ = new CertVerifier();
    builder.SetCertVerifier(base::WrapUnique(cert_verifier_));
    builder.set_user_agent(::GetUserAgent());
    context_ = builder.Build();
  }
}

void PrivetV3ContextGetter::AddPairedHost(
    const std::string& host,
    const net::SHA256HashValue& certificate_fingerprint,
    const base::Closure& callback) {
  net_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&PrivetV3ContextGetter::AddPairedHostOnNetThread,
                 weak_ptr_factory_.GetWeakPtr(), host, certificate_fingerprint),
      callback);
}

void PrivetV3ContextGetter::AddPairedHostOnNetThread(
    const std::string& host,
    const net::SHA256HashValue& certificate_fingerprint) {
  InitOnNetThread();
  cert_verifier_->AddPairedHost(host, certificate_fingerprint);
}

PrivetV3ContextGetter::~PrivetV3ContextGetter() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
}

}  // namespace extensions
