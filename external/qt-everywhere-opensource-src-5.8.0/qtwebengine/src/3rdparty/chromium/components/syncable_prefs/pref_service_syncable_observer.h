// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_
#define COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_

namespace syncable_prefs {

class PrefServiceSyncableObserver {
 public:
  // Invoked when PrefService::IsSyncing() changes.
  virtual void OnIsSyncingChanged() = 0;

 protected:
  virtual ~PrefServiceSyncableObserver() {}
};

}  // namespace syncable_prefs

#endif  // COMPONENTS_SYNCABLE_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_
