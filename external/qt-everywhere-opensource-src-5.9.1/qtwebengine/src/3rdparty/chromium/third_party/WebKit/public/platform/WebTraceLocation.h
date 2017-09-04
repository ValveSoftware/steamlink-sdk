// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTraceLocation_h
#define WebTraceLocation_h

#include "base/location.h"

namespace blink {

using WebTraceLocation = tracked_objects::Location;
#define BLINK_FROM_HERE FROM_HERE
}

#endif  // WebTraceLocation_h
