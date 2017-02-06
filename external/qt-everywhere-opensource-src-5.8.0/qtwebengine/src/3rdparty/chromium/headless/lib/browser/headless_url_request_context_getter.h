// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "headless/public/headless_browser.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
class MessageLoop;
}

namespace net {
class HostResolver;
class MappedHostResolver;
class NetworkDelegate;
class NetLog;
class ProxyConfigService;
class ProxyService;
class URLRequestContextStorage;
}

namespace headless {

class HeadlessURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  HeadlessURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      content::ProtocolHandlerMap* protocol_handlers,
      ProtocolHandlerMap context_protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      HeadlessBrowser::Options* options);

  // net::URLRequestContextGetter implementation:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  net::HostResolver* host_resolver() const;

 protected:
  ~HeadlessURLRequestContextGetter() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // The |options| object given to the constructor is not guaranteed to outlive
  // this class, so we make copies of the parts we need to access on the IO
  // thread.
  std::string user_agent_;
  std::string host_resolver_rules_;
  net::HostPortPair proxy_server_;

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessURLRequestContextGetter);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_URL_REQUEST_CONTEXT_GETTER_H_
