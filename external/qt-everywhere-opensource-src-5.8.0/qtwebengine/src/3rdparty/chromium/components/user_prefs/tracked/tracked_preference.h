// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_H_
#define COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_H_

class PrefHashStoreTransaction;

namespace base {
class DictionaryValue;
class Value;
}

// A TrackedPreference tracks changes to an individual preference, reporting and
// reacting to them according to preference-specific and browser-wide policies.
class TrackedPreference {
 public:
  virtual ~TrackedPreference() {}

  // Notifies the underlying TrackedPreference about its new |value| which
  // can update hashes in the corresponding hash store via |transaction|.
  virtual void OnNewValue(const base::Value* value,
                          PrefHashStoreTransaction* transaction) const = 0;

  // Verifies that the value of this TrackedPreference in |pref_store_contents|
  // is valid. Responds to verification failures according to
  // preference-specific and browser-wide policy and reports results to via UMA.
  // May use |transaction| to check/modify hashes in the corresponding hash
  // store.
  virtual bool EnforceAndReport(
      base::DictionaryValue* pref_store_contents,
      PrefHashStoreTransaction* transaction) const = 0;
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_H_
