// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_CLIENT_H_
#define CC_SURFACES_DISPLAY_CLIENT_H_

namespace cc {
struct ManagedMemoryPolicy;

class DisplayClient {
 public:
  virtual ~DisplayClient() {}
  virtual void DisplayOutputSurfaceLost() = 0;
  virtual void DisplaySetMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_CLIENT_H_
