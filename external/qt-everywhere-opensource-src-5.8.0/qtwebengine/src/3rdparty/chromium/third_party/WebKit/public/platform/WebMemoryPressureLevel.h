// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryPressureLevel_h
#define WebMemoryPressureLevel_h

namespace blink {

// These number must correspond to
// base::MemoryPressureListener::MemoryPressureLevel.
enum WebMemoryPressureLevel {
    WebMemoryPressureLevelNone,
    WebMemoryPressureLevelModerate,
    WebMemoryPressureLevelCritical,
};

} // namespace blink

#endif
