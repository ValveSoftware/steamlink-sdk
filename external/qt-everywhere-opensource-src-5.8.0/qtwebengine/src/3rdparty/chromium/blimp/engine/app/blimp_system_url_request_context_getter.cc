// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_system_url_request_context_getter.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "blimp/engine/common/blimp_user_agent.h"
#include "content/public/browser/browser_thread.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace blimp {
namespace engine {

BlimpSystemURLRequestContextGetter::BlimpSystemURLRequestContextGetter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BlimpSystemURLRequestContextGetter::~BlimpSystemURLRequestContextGetter() {}

net::URLRequestContext*
BlimpSystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url_request_context_) {
    // Use default values
    net::URLRequestContextBuilder builder;

    // TODO(jessicag): See if proxy_service setup should be harmonized with
    // user request context getter. http://crbug/609981
    builder.set_proxy_service(net::ProxyService::CreateDirect());
    builder.set_user_agent(GetBlimpEngineUserAgent());
    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpSystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO);
}

}  // namespace engine
}  // namespace blimp
