// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_PREF_HASH_FILTER_H_
#define COMPONENTS_USER_PREFS_TRACKED_PREF_HASH_FILTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/interceptable_pref_filter.h"
#include "components/user_prefs/tracked/tracked_preference.h"

class PrefHashStore;
class PrefService;
class PrefStore;
class TrackedPreferenceValidationDelegate;

namespace base {
class DictionaryValue;
class Time;
class Value;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter : public InterceptablePrefFilter {
 public:
  enum EnforcementLevel { NO_ENFORCEMENT, ENFORCE_ON_LOAD };

  enum PrefTrackingStrategy {
    // Atomic preferences are tracked as a whole.
    TRACKING_STRATEGY_ATOMIC,
    // Split preferences are dictionaries for which each top-level entry is
    // tracked independently. Note: preferences using this strategy must be kept
    // in sync with TrackedSplitPreferences in histograms.xml.
    TRACKING_STRATEGY_SPLIT,
  };

  enum ValueType {
    VALUE_IMPERSONAL,
    // The preference value may contain personal information.
    VALUE_PERSONAL,
  };

  struct TrackedPreferenceMetadata {
    size_t reporting_id;
    const char* name;
    EnforcementLevel enforcement_level;
    PrefTrackingStrategy strategy;
    ValueType value_type;
  };

  // Constructs a PrefHashFilter tracking the specified |tracked_preferences|
  // using |pref_hash_store| to check/store hashes. An optional |delegate| is
  // notified of the status of each preference as it is checked.
  // If |on_reset_on_load| is provided, it will be invoked if a reset occurs in
  // FilterOnLoad.
  // |reporting_ids_count| is the count of all possible IDs (possibly greater
  // than |tracked_preferences.size()|). If |report_super_mac_validity| is true,
  // the state of the super MAC will be reported via UMA during
  // FinalizeFilterOnLoad.
  PrefHashFilter(
      std::unique_ptr<PrefHashStore> pref_hash_store,
      const std::vector<TrackedPreferenceMetadata>& tracked_preferences,
      const base::Closure& on_reset_on_load,
      TrackedPreferenceValidationDelegate* delegate,
      size_t reporting_ids_count,
      bool report_super_mac_validity);

  ~PrefHashFilter() override;

  // Registers required user preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Retrieves the time of the last reset event, if any, for the provided user
  // preferences. If no reset has occurred, Returns a null |Time|.
  static base::Time GetResetTime(PrefService* user_prefs);

  // Clears the time of the last reset event, if any, for the provided user
  // preferences.
  static void ClearResetTime(PrefService* user_prefs);

  // Initializes the PrefHashStore with hashes of the tracked preferences in
  // |pref_store_contents|. |pref_store_contents| will be the |storage| passed
  // to PrefHashStore::BeginTransaction().
  void Initialize(base::DictionaryValue* pref_store_contents);

  // PrefFilter remaining implementation.
  void FilterUpdate(const std::string& path) override;
  void FilterSerializeData(base::DictionaryValue* pref_store_contents) override;

 private:
  // InterceptablePrefFilter implementation.
  void FinalizeFilterOnLoad(
      const PostFilterOnLoadCallback& post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) override;

  // Callback to be invoked only once (and subsequently reset) on the next
  // FilterOnLoad event. It will be allowed to modify the |prefs| handed to
  // FilterOnLoad before handing them back to this PrefHashFilter.
  FilterOnLoadInterceptor filter_on_load_interceptor_;

  // A map of paths to TrackedPreferences; this map owns this individual
  // TrackedPreference objects.
  typedef base::ScopedPtrHashMap<std::string,
                                 std::unique_ptr<TrackedPreference>>
      TrackedPreferencesMap;
  // A map from changed paths to their corresponding TrackedPreferences (which
  // aren't owned by this map).
  typedef std::map<std::string, const TrackedPreference*> ChangedPathsMap;

  std::unique_ptr<PrefHashStore> pref_hash_store_;

  // Invoked if a reset occurs in a call to FilterOnLoad.
  const base::Closure on_reset_on_load_;

  TrackedPreferencesMap tracked_paths_;

  // The set of all paths whose value has changed since the last call to
  // FilterSerializeData.
  ChangedPathsMap changed_paths_;

  // Whether to report the validity of the super MAC at load time (via UMA).
  bool report_super_mac_validity_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilter);
};

#endif  // COMPONENTS_PREFS_TRACKED_PREF_HASH_FILTER_H_
