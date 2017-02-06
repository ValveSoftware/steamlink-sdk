// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pref_registry/testing_pref_service_syncable.h"

#include "base/bind.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"

template <>
TestingPrefServiceBase<PrefService, user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(TestingPrefStore* managed_prefs,
                           TestingPrefStore* user_prefs,
                           TestingPrefStore* recommended_prefs,
                           user_prefs::PrefRegistrySyncable* pref_registry,
                           PrefNotifierImpl* pref_notifier)
    : PrefService(pref_notifier,
                  new PrefValueStore(managed_prefs,
                                     NULL,  // supervised_user_prefs
                                     NULL,  // extension_prefs
                                     NULL,  // command_line_prefs
                                     user_prefs,
                                     recommended_prefs,
                                     pref_registry->defaults().get(),
                                     pref_notifier),
          user_prefs,
          pref_registry,
          base::Bind(&TestingPrefServiceBase<
                       PrefService,
                       user_prefs::PrefRegistrySyncable>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

namespace user_prefs {

TestingPrefServiceSyncable::TestingPrefServiceSyncable()
    : TestingPrefServiceBase<PrefService, PrefRegistrySyncable>(
          new TestingPrefStore(),
          new TestingPrefStore(),
          new TestingPrefStore(),
          new PrefRegistrySyncable(),
          new PrefNotifierImpl()) {
}

TestingPrefServiceSyncable::TestingPrefServiceSyncable(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistrySyncable* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : TestingPrefServiceBase<PrefService, PrefRegistrySyncable>(
          managed_prefs,
          user_prefs,
          recommended_prefs,
          pref_registry,
          pref_notifier) {
}

TestingPrefServiceSyncable::~TestingPrefServiceSyncable() {
}

PrefRegistrySyncable* TestingPrefServiceSyncable::registry() {
  return static_cast<PrefRegistrySyncable*>(DeprecatedGetPrefRegistry());
}

}  // namespace user_prefs
