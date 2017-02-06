// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power/origin_power_map_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/power/origin_power_map.h"

namespace power {
// static
OriginPowerMap* OriginPowerMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OriginPowerMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OriginPowerMapFactory* OriginPowerMapFactory::GetInstance() {
  return base::Singleton<OriginPowerMapFactory>::get();
}

OriginPowerMapFactory::OriginPowerMapFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginPowerMap",
          BrowserContextDependencyManager::GetInstance()) {
}

OriginPowerMapFactory::~OriginPowerMapFactory() {
}

KeyedService* OriginPowerMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OriginPowerMap();
}

}  // namespace power
