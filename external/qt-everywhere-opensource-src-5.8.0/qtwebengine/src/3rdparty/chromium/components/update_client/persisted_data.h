// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
#define COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace update_client {

// A PersistedData is a wrapper layer around a PrefService, designed to maintain
// update data that outlives the browser process and isn't exposed outside of
// update_client.
//
// The public methods of this class should be called only on the thread that
// initializes it - which also has to match the thread the PrefService has been
// initialized on.
class PersistedData {
 public:
  // Constructs a provider that uses the specified |pref_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| must outlive the entire update_client.
  explicit PersistedData(PrefService* pref_service);

  ~PersistedData();

  // Returns the DateLastRollCall (the server-localized calendar date number the
  // |id| was last checked by this client on) for the specified |id|.
  // -2 indicates that there is no recorded date number for the |id|.
  int GetDateLastRollCall(const std::string& id) const;

  // Returns the PingFreshness (a random token that is written into the profile
  // data whenever the DateLastRollCall it is modified) for the specified |id|.
  // "" indicates that there is no recorded freshness value for the |id|.
  std::string GetPingFreshness(const std::string& id) const;

  // Records the DateLastRollCall for the specified |ids|. |datenum| must be a
  // non-negative integer: calls with a negative |datenum| are simply ignored.
  // Calls to SetDateLastRollCall that occur prior to the persisted data store
  // has been fully initialized are ignored. Also sets the PingFreshness.
  void SetDateLastRollCall(const std::vector<std::string>& ids, int datenum);

  // This is called only via update_client's RegisterUpdateClientPreferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  base::ThreadChecker thread_checker_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(PersistedData);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
