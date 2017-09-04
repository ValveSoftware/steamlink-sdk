// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service.h"

#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcService::ArcService(ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  DCHECK(arc_bridge_service());
}

ArcService::~ArcService() {}

}  // namespace arc
