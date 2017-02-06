// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <utility>

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"

namespace content {

static NavigationURLLoaderFactory* g_factory = nullptr;

std::unique_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    BrowserContext* browser_context,
    std::unique_ptr<NavigationRequestInfo> request_info,
    ServiceWorkerContextWrapper* service_worker_context_wrapper,
    NavigationURLLoaderDelegate* delegate) {
  if (g_factory) {
    return g_factory->CreateLoader(browser_context, std::move(request_info),
                                   service_worker_context_wrapper, delegate);
  }
  return std::unique_ptr<NavigationURLLoader>(
      new NavigationURLLoaderImpl(browser_context, std::move(request_info),
                                  service_worker_context_wrapper, delegate));
}

void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_factory == nullptr || factory == nullptr);
  g_factory = factory;
}

}  // namespace content
