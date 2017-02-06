// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/syncable_prefs/pref_service_syncable_factory.h"

#include "base/trace_event/trace_event.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/syncable_prefs/pref_service_syncable.h"

#if defined(SYNCABLE_PREFS_USE_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_service.h"  // nogncheck
#include "components/policy/core/common/policy_types.h"  // nogncheck
#endif

namespace syncable_prefs {

PrefServiceSyncableFactory::PrefServiceSyncableFactory() {
}

PrefServiceSyncableFactory::~PrefServiceSyncableFactory() {
}

void PrefServiceSyncableFactory::SetManagedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if defined(SYNCABLE_PREFS_USE_POLICY)
  set_managed_prefs(new policy::ConfigurationPolicyPrefStore(
      service, connector->GetHandlerList(), policy::POLICY_LEVEL_MANDATORY));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetRecommendedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if defined(SYNCABLE_PREFS_USE_POLICY)
  set_recommended_prefs(new policy::ConfigurationPolicyPrefStore(
      service, connector->GetHandlerList(), policy::POLICY_LEVEL_RECOMMENDED));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetPrefModelAssociatorClient(
    PrefModelAssociatorClient* pref_model_associator_client) {
  pref_model_associator_client_ = pref_model_associator_client;
}

std::unique_ptr<PrefServiceSyncable> PrefServiceSyncableFactory::CreateSyncable(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  TRACE_EVENT0("browser", "PrefServiceSyncableFactory::CreateSyncable");
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  std::unique_ptr<PrefServiceSyncable> pref_service(new PrefServiceSyncable(
      pref_notifier,
      new PrefValueStore(managed_prefs_.get(), supervised_user_prefs_.get(),
                         extension_prefs_.get(), command_line_prefs_.get(),
                         user_prefs_.get(), recommended_prefs_.get(),
                         pref_registry->defaults().get(), pref_notifier),
      user_prefs_.get(), pref_registry, pref_model_associator_client_,
      read_error_callback_, async_));
  return pref_service;
}

}  // namespace syncable_prefs
