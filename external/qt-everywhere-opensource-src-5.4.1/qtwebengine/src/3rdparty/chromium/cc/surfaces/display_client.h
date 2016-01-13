// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_CLIENT_H_
#define CC_SURFACES_DISPLAY_CLIENT_H_

#include "base/memory/scoped_ptr.h"

namespace cc {

class OutputSurface;

class DisplayClient {
 public:
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() = 0;

 protected:
  virtual ~DisplayClient() {}
};
}

#endif  // CC_SURFACES_DISPLAY_CLIENT_H_
