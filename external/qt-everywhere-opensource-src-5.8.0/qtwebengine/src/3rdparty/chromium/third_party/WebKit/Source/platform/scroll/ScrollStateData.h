// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollStateData_h
#define ScrollStateData_h

#include "cc/input/scroll_state.h"

namespace blink {

// A wrapper around cc's structure to expose it to core.
struct ScrollStateData : public cc::ScrollStateData {
    ScrollStateData()
        : cc::ScrollStateData()
    {
    }
};

} // namespace blink

#endif
