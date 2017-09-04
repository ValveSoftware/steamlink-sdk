// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkInstrumentation_h
#define NetworkInstrumentation_h

#include "platform/PlatformExport.h"
#include "platform/network/ResourceLoadPriority.h"

namespace blink {
class ResourceRequest;
}  // namespace blink

namespace network_instrumentation {

enum RequestOutcome { Success, Fail };

class PLATFORM_EXPORT ScopedResourceLoadTracker {
 public:
  ScopedResourceLoadTracker(unsigned long resourceID,
                            const blink::ResourceRequest&);
  ~ScopedResourceLoadTracker();
  void resourceLoadContinuesBeyondScope();

 private:
  // If this variable is false, close resource load slice at end of scope.
  bool m_resourceLoadContinuesBeyondScope;

  const unsigned long m_resourceID;

  DISALLOW_COPY_AND_ASSIGN(ScopedResourceLoadTracker);
};

void PLATFORM_EXPORT resourcePrioritySet(unsigned long resourceID,
                                         blink::ResourceLoadPriority);

void PLATFORM_EXPORT endResourceLoad(unsigned long resourceID, RequestOutcome);

}  // namespace network_instrumentation

#endif  // NetworkInstrumentation_h
