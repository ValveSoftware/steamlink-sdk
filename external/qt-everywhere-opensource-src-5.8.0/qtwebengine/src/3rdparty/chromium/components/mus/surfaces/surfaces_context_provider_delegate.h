// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_DELEGATE_H_
#define COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_DELEGATE_H_

#include <stdint.h>

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace mus {

class SurfacesContextProviderDelegate {
 public:
  virtual void OnVSyncParametersUpdated(const base::TimeTicks& timebase,
                                        const base::TimeDelta& interval) = 0;

 protected:
  virtual ~SurfacesContextProviderDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_DELEGATE_H_
