// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

#include "base/logging.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web/public/browser_state.h"

void RefcountedBrowserStateKeyedServiceFactory::SetTestingFactory(
    web::BrowserState* context,
    TestingFactoryFunction testing_factory) {
  RefcountedKeyedServiceFactory::SetTestingFactory(
      context,
      reinterpret_cast<RefcountedKeyedServiceFactory::TestingFactoryFunction>(
          testing_factory));
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserStateKeyedServiceFactory::SetTestingFactoryAndUse(
    web::BrowserState* context,
    TestingFactoryFunction testing_factory) {
  return RefcountedKeyedServiceFactory::SetTestingFactoryAndUse(
      context,
      reinterpret_cast<RefcountedKeyedServiceFactory::TestingFactoryFunction>(
          testing_factory));
}

RefcountedBrowserStateKeyedServiceFactory::
    RefcountedBrowserStateKeyedServiceFactory(
        const char* name,
        BrowserStateDependencyManager* manager)
    : RefcountedKeyedServiceFactory(name, manager) {
}

RefcountedBrowserStateKeyedServiceFactory::
    ~RefcountedBrowserStateKeyedServiceFactory() {
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserStateKeyedServiceFactory::GetServiceForBrowserState(
    web::BrowserState* context,
    bool create) {
  return RefcountedKeyedServiceFactory::GetServiceForContext(context, create);
}

web::BrowserState*
RefcountedBrowserStateKeyedServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  DCHECK(CalledOnValidThread());

#ifndef NDEBUG
  AssertContextWasntDestroyed(context);
#endif

  // Safe default for Incognito mode: no service.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

bool RefcountedBrowserStateKeyedServiceFactory::
    ServiceIsCreatedWithBrowserState() const {
  return KeyedServiceBaseFactory::ServiceIsCreatedWithContext();
}

bool RefcountedBrowserStateKeyedServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return KeyedServiceBaseFactory::ServiceIsNULLWhileTesting();
}

void RefcountedBrowserStateKeyedServiceFactory::BrowserStateShutdown(
    web::BrowserState* context) {
  RefcountedKeyedServiceFactory::ContextShutdown(context);
}

void RefcountedBrowserStateKeyedServiceFactory::BrowserStateDestroyed(
    web::BrowserState* context) {
  RefcountedKeyedServiceFactory::ContextDestroyed(context);
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserStateKeyedServiceFactory::BuildServiceInstanceFor(
    base::SupportsUserData* context) const {
  return BuildServiceInstanceFor(static_cast<web::BrowserState*>(context));
}

bool RefcountedBrowserStateKeyedServiceFactory::IsOffTheRecord(
    base::SupportsUserData* context) const {
  return static_cast<web::BrowserState*>(context)->IsOffTheRecord();
}

base::SupportsUserData*
RefcountedBrowserStateKeyedServiceFactory::GetContextToUse(
    base::SupportsUserData* context) const {
  return GetBrowserStateToUse(static_cast<web::BrowserState*>(context));
}

bool RefcountedBrowserStateKeyedServiceFactory::ServiceIsCreatedWithContext()
    const {
  return ServiceIsCreatedWithBrowserState();
}

void RefcountedBrowserStateKeyedServiceFactory::ContextShutdown(
    base::SupportsUserData* context) {
  BrowserStateShutdown(static_cast<web::BrowserState*>(context));
}

void RefcountedBrowserStateKeyedServiceFactory::ContextDestroyed(
    base::SupportsUserData* context) {
  BrowserStateDestroyed(static_cast<web::BrowserState*>(context));
}

void RefcountedBrowserStateKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterBrowserStatePrefs(registry);
}
