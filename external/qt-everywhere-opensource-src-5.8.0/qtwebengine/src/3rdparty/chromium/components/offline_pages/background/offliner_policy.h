// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_

namespace offline_pages {

// Policy for the Background Offlining system.  Some policy will belong to the
// RequestCoordinator, some to the RequestQueue, and some to the Offliner.
class OfflinerPolicy {
 public:
  OfflinerPolicy(){};

  // TODO(petewil): Implement and add a .cc file.
  // Eventually this should get data from a finch experiment.
};
}

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
