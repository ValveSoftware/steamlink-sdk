// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_VALUE_MAP_PREF_STORE_H_
#define BASE_PREFS_VALUE_MAP_PREF_STORE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/prefs/base_prefs_export.h"
#include "base/prefs/pref_value_map.h"
#include "base/prefs/writeable_pref_store.h"

// A basic PrefStore implementation that uses a simple name-value map for
// storing the preference values.
class BASE_PREFS_EXPORT ValueMapPrefStore : public WriteablePrefStore {
 public:
  ValueMapPrefStore();

  // PrefStore overrides:
  virtual bool GetValue(const std::string& key,
                        const base::Value** value) const OVERRIDE;
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual bool HasObservers() const OVERRIDE;

  // WriteablePrefStore overrides:
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** value) OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;

 protected:
  virtual ~ValueMapPrefStore();

  // Notify observers about the initialization completed event.
  void NotifyInitializationCompleted();

 private:
  PrefValueMap prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ValueMapPrefStore);
};

#endif  // BASE_PREFS_VALUE_MAP_PREF_STORE_H_
