// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorState_h
#define SensorState_h

namespace blink {

enum class SensorState {
    Idle,
    Activating,
    Active,
    Errored
};

} // namespace blink

#endif // SensorState_h

