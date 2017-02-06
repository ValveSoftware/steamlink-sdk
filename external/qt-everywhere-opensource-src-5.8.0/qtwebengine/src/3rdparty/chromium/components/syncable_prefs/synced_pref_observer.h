// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNCABLE_PREFS_SYNCED_PREF_OBSERVER_H_
#define COMPONENTS_SYNCABLE_PREFS_SYNCED_PREF_OBSERVER_H_

#include <string>

namespace syncable_prefs {

class SyncedPrefObserver {
 public:
  virtual void OnSyncedPrefChanged(const std::string& path, bool from_sync) = 0;
};

}  // namespace syncable_prefs

#endif  // COMPONENTS_SYNCABLE_PREFS_SYNCED_PREF_OBSERVER_H_
