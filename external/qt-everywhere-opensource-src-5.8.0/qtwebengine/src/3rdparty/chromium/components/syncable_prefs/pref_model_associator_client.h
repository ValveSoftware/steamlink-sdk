// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNCABLE_PREFS_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define COMPONENTS_SYNCABLE_PREFS_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "base/macros.h"

namespace syncable_prefs {

// This class allows the embedder to configure the PrefModelAssociator to
// have a different behaviour when receiving preference synchronisations
// events from the server.
class PrefModelAssociatorClient {
 public:
  // Returns true if the preference named |pref_name| is a list preference
  // whose server value is merged with local value during synchronisation.
  virtual bool IsMergeableListPreference(
      const std::string& pref_name) const = 0;

  // Returns true if the preference named |pref_name| is a dictionary preference
  // whose server value is merged with local value during synchronisation.
  virtual bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const = 0;

 protected:
  PrefModelAssociatorClient() {}
  virtual ~PrefModelAssociatorClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefModelAssociatorClient);
};

}  // namespace syncable_prefs

#endif  // COMPONENTS_SYNCABLE_PREFS_PREF_MODEL_ASSOCIATOR_CLIENT_H_
