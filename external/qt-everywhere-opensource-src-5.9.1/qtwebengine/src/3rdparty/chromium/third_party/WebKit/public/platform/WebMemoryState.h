// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryState_h
#define WebMemoryState_h

namespace blink {

// These numbers must correspond to base::MemoryState.
enum class MemoryState {
  UNKNOWN = -1,
  NORMAL = 0,
  THROTTLED = 1,
  SUSPENDED = 2,
};

}  // namespace blink

#endif
