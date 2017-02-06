// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"

using content::BrowserContext;

namespace extensions {

// static
ProcessMap* ProcessMapFactory::GetForBrowserContext(BrowserContext* context) {
  return static_cast<ProcessMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ProcessMapFactory* ProcessMapFactory::GetInstance() {
  return base::Singleton<ProcessMapFactory>::get();
}

ProcessMapFactory::ProcessMapFactory()
    : BrowserContextKeyedServiceFactory(
          "ProcessMap",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies on other services.
}

ProcessMapFactory::~ProcessMapFactory() {}

KeyedService* ProcessMapFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ProcessMap;
}

BrowserContext* ProcessMapFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
