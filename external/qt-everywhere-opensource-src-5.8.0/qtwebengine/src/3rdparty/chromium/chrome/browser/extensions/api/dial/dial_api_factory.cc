// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_api_factory.h"

#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
scoped_refptr<DialAPI> DialAPIFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DialAPI*>(
      GetInstance()->GetServiceForBrowserContext(context, true).get());
}

// static
DialAPIFactory* DialAPIFactory::GetInstance() {
  return base::Singleton<DialAPIFactory>::get();
}

DialAPIFactory::DialAPIFactory() : RefcountedBrowserContextKeyedServiceFactory(
    "DialAPI", BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

DialAPIFactory::~DialAPIFactory() {
}

scoped_refptr<RefcountedKeyedService> DialAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return scoped_refptr<DialAPI>(new DialAPI(static_cast<Profile*>(profile)));
}

bool DialAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DialAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
