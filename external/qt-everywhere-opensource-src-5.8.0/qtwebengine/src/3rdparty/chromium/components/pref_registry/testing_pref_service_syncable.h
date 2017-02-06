// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERF_REGISTRY_TESTING_PREF_SERVICE_SYNCABLE_H_
#define COMPONENTS_PERF_REGISTRY_TESTING_PREF_SERVICE_SYNCABLE_H_

#include "base/macros.h"
#include "components/prefs/testing_pref_service.h"

namespace user_prefs {

class PrefRegistrySyncable;

// Test version of PrefServiceSyncable.
class TestingPrefServiceSyncable
    : public TestingPrefServiceBase<PrefService, PrefRegistrySyncable> {
 public:
  TestingPrefServiceSyncable();
  TestingPrefServiceSyncable(TestingPrefStore* managed_prefs,
                             TestingPrefStore* user_prefs,
                             TestingPrefStore* recommended_prefs,
                             PrefRegistrySyncable* pref_registry,
                             PrefNotifierImpl* pref_notifier);
  ~TestingPrefServiceSyncable() override;

  // This is provided as a convenience; on a production PrefService
  // you would do all registrations before constructing it, passing it
  // a PrefRegistry via its constructor (or via e.g. PrefServiceFactory).
  PrefRegistrySyncable* registry();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSyncable);
};

}  // namespace user_prefs

#endif  // COMPONENTS_PERF_REGISTRY_TESTING_PREF_SERVICE_SYNCABLE_H_
