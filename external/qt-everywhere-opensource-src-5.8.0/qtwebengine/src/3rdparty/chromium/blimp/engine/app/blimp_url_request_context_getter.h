// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_URL_REQUEST_CONTEXT_GETTER_H_
#define BLIMP_ENGINE_APP_BLIMP_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoop;
class SingleThreadTaskRunner;
}

namespace net {
class HostResolver;
class NetworkDelegate;
class NetLog;
class ProxyConfigService;
class ProxyService;
}

namespace blimp {
namespace engine {

// The URLRequestContextGetter for Blimp "user" requests.
// System request context is handled in a separate class.
class BlimpURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // The content of |protocol_handlers| is is swapped into the new instance.
  BlimpURLRequestContextGetter(
      bool ignore_certificate_errors,
      const base::FilePath& base_path,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_loop_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_loop_task_runner,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      net::NetLog* net_log);

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  net::HostResolver* host_resolver();

 private:
  ~BlimpURLRequestContextGetter() override;

  bool ignore_certificate_errors_;
  base::FilePath base_path_;
  scoped_refptr<base::SingleThreadTaskRunner> io_loop_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_loop_task_runner_;
  net::NetLog* net_log_;

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(BlimpURLRequestContextGetter);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_URL_REQUEST_CONTEXT_GETTER_H_
