// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREFS_OBSERVER_STORE_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREFS_OBSERVER_STORE_H_

#include <set>

#include "base/macros.h"
#include "components/prefs/value_map_pref_store.h"
#include "mojo/common/common_custom_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace shell {
class Connector;
}

class PrefObserverStoreTest;

// An implementation of PrefStore which uses prefs::mojom::PreferenceManager as
// the backing of the preferences.
//
// PrefObserverStore caches the values locally to provide synchronous access.
// The cache is empty until the first notification of OnPreferencesChanged is
// received from the prefs::mojom::PreferenceManager. Upon recieving an initial
// OnPreferencesChanged initialization will be considered as completed, and any
// PrefStore::Observer will be notified of OnInitializationCompleted.
//
// Currently this does not support RemoveValue.
class PrefObserverStore : public ValueMapPrefStore,
                          public prefs::mojom::PreferencesObserver {
 public:
  explicit PrefObserverStore(
      prefs::mojom::PreferencesManagerPtr prefs_manager_ptr);

  // Defines the set of |keys| which PrefOvserverStore will handle. Begins
  // listening for changes to these from |prefs_manager_|.
  void Init(const std::set<std::string>& keys);

  // PrefStore:
  bool GetValue(const std::string& key,
                const base::Value** value) const override;

  // ValueMapPrefStore:
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool GetMutableValue(const std::string& key, base::Value** value) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        std::unique_ptr<base::Value> value,
                        uint32_t flags) override;

 protected:
  // base::RefCounted<PrefStore>:
  ~PrefObserverStore() override;

 private:
  friend class PrefObserverStoreTest;

  // Notifies |prefs_manager_| of the change in |key| - |value| pair.
  void SetValueOnPreferenceManager(const std::string& key,
                                   base::Value const & value);

  // prefs::mojom::PreferenceObserver:
  void OnPreferencesChanged(const base::DictionaryValue& preferences) override;

  mojo::Binding<prefs::mojom::PreferencesObserver> prefs_binding_;
  prefs::mojom::PreferencesManagerPtr prefs_manager_ptr_;

  std::set<std::string> keys_;

  // True upon the first OnPreferencesChanged received after Init.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(PrefObserverStore);
};

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREFS_OBSERVER_STORE_H_
