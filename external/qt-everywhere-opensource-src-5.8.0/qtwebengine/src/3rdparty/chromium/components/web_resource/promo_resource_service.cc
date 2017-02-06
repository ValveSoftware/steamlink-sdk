// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/promo_resource_service.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"

namespace web_resource {

static const char kPrefPromoObject[] = "promo";

// static
void PromoResourceService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kNtpPromoResourceCacheUpdate, "0");
  registry->RegisterDictionaryPref(kPrefPromoObject);
}

// static
void PromoResourceService::ClearLocalState(PrefService* local_state) {
  local_state->ClearPref(prefs::kNtpPromoResourceCacheUpdate);
  local_state->ClearPref(kPrefPromoObject);
}

PromoResourceService::PromoResourceService() {}

PromoResourceService::~PromoResourceService() {}

}  // namespace web_resource
