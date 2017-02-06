// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/persisted_data.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

const char kPersistedDataPreference[] = "updateclientdata";
const int kDateLastRollCallUnknown = -2;

namespace update_client {

PersistedData::PersistedData(PrefService* pref_service)
    : pref_service_(pref_service) {}

PersistedData::~PersistedData() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

int PersistedData::GetDateLastRollCall(const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_)
    return kDateLastRollCallUnknown;
  int dlrc = kDateLastRollCallUnknown;
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  // We assume ids do not contain '.' characters.
  DCHECK_EQ(std::string::npos, id.find('.'));
  if (!dict->GetInteger(base::StringPrintf("apps.%s.dlrc", id.c_str()), &dlrc))
    return kDateLastRollCallUnknown;
  return dlrc;
}

std::string PersistedData::GetPingFreshness(const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_)
    return std::string();
  std::string freshness;
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  // We assume ids do not contain '.' characters.
  DCHECK_EQ(std::string::npos, id.find('.'));
  if (!dict->GetString(base::StringPrintf("apps.%s.pf", id.c_str()),
                       &freshness))
    return std::string();
  return base::StringPrintf("{%s}", freshness.c_str());
}

void PersistedData::SetDateLastRollCall(const std::vector<std::string>& ids,
                                        int datenum) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_ || datenum < 0)
    return;
  DictionaryPrefUpdate update(pref_service_, kPersistedDataPreference);
  for (auto id : ids) {
    // We assume ids do not contain '.' characters.
    DCHECK_EQ(std::string::npos, id.find('.'));
    update->SetInteger(base::StringPrintf("apps.%s.dlrc", id.c_str()), datenum);
    update->SetString(base::StringPrintf("apps.%s.pf", id.c_str()),
                      base::GenerateGUID());
  }
}

void PersistedData::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPersistedDataPreference);
}

}  // namespace update_client
