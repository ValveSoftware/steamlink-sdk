// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class CertVerifier;
class URLRequestContext;
struct SHA256HashValue;
}

namespace extensions {

class PrivetV3ContextGetter : public net::URLRequestContextGetter {
 public:
  PrivetV3ContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& net_task_runner);

  net::URLRequestContext* GetURLRequestContext() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  void AddPairedHost(const std::string& host,
                     const net::SHA256HashValue& certificate_fingerprint,
                     const base::Closure& callback);

 protected:
  ~PrivetV3ContextGetter() override;

 private:
  class CertVerifier;

  void InitOnNetThread();
  void AddPairedHostOnNetThread(
      const std::string& host,
      const net::SHA256HashValue& certificate_fingerprint);

  // Owned by context_
  CertVerifier* cert_verifier_ = nullptr;
  std::unique_ptr<net::URLRequestContext> context_;

  scoped_refptr<base::SingleThreadTaskRunner> net_task_runner_;

  base::WeakPtrFactory<PrivetV3ContextGetter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetV3ContextGetter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_
