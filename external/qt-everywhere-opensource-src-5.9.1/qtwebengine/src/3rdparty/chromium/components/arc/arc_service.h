// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_H_
#define COMPONENTS_ARC_ARC_SERVICE_H_

#include "base/macros.h"

namespace arc {

class ArcBridgeService;

// Abstract class whose lifecycle will be managed by the ArcServiceManager.  It
// is guaranteed that once the ownership of an ArcService has been transferred
// to ArcServiceManager, it will outlive the ArcBridgeService, so it is safe to
// keep a weak reference to it.
class ArcService {
 public:
  virtual ~ArcService();

  ArcBridgeService* arc_bridge_service() const { return arc_bridge_service_; }

 protected:
  explicit ArcService(ArcBridgeService* arc_bridge_service);

 private:
  ArcBridgeService* const arc_bridge_service_;  // owned by ArcServiceManager.

  DISALLOW_COPY_AND_ASSIGN(ArcService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_H_
