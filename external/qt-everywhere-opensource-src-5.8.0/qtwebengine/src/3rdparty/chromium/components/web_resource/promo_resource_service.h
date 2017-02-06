// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#define COMPONENTS_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_

#include <stdint.h>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace web_resource {

// This class is being removed <https://crbug.com/576772>. It currently only
// exists to delete its old preferences.
class PromoResourceService {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void ClearLocalState(PrefService* local_state);

 private:
  PromoResourceService();
  ~PromoResourceService();

  DISALLOW_COPY_AND_ASSIGN(PromoResourceService);
};

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
