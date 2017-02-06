// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "content/public/common/content_client.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

class SchemeAndOriginWhitelist {
 public:
  SchemeAndOriginWhitelist() { Reset(); }
  ~SchemeAndOriginWhitelist() {}

  void Reset() {
    GetContentClient()->AddSecureSchemesAndOrigins(&secure_schemes_,
                                                   &secure_origins_);
    GetContentClient()->AddServiceWorkerSchemes(&service_worker_schemes_);
  }

  const std::set<std::string>& secure_schemes() const {
    return secure_schemes_;
  }
  const std::set<GURL>& secure_origins() const { return secure_origins_; }
  const std::set<std::string>& service_worker_schemes() const {
    return service_worker_schemes_;
  }

 private:
  std::set<std::string> secure_schemes_;
  std::set<GURL> secure_origins_;
  std::set<std::string> service_worker_schemes_;
  DISALLOW_COPY_AND_ASSIGN(SchemeAndOriginWhitelist);
};

base::LazyInstance<SchemeAndOriginWhitelist>::Leaky g_trustworthy_whitelist =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeIsCryptographic() || url.SchemeIsFile())
    return true;

  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  std::string hostname = url.HostNoBrackets();
  if (net::IsLocalhost(hostname))
    return true;

  if (ContainsKey(g_trustworthy_whitelist.Get().secure_schemes(), url.scheme()))
    return true;

  if (ContainsKey(g_trustworthy_whitelist.Get().secure_origins(),
                  url.GetOrigin())) {
    return true;
  }

  return false;
}

bool OriginCanAccessServiceWorkers(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() && IsOriginSecure(url))
    return true;

  if (ContainsKey(g_trustworthy_whitelist.Get().service_worker_schemes(),
                  url.scheme())) {
    return true;
  }

  return false;
}

void ResetSchemesAndOriginsWhitelistForTesting() {
  g_trustworthy_whitelist.Get().Reset();
}

}  // namespace content
