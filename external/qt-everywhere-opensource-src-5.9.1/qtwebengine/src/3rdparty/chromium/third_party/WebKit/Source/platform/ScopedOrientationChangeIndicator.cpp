// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScopedOrientationChangeIndicator.h"

#include "wtf/Assertions.h"

namespace blink {

ScopedOrientationChangeIndicator::State
    ScopedOrientationChangeIndicator::s_state =
        ScopedOrientationChangeIndicator::State::NotProcessing;

ScopedOrientationChangeIndicator::ScopedOrientationChangeIndicator() {
  DCHECK(isMainThread());

  m_previousState = s_state;
  s_state = State::Processing;
}

ScopedOrientationChangeIndicator::~ScopedOrientationChangeIndicator() {
  DCHECK(isMainThread());
  s_state = m_previousState;
}

// static
bool ScopedOrientationChangeIndicator::processingOrientationChange() {
  DCHECK(isMainThread());
  return s_state == State::Processing;
}

}  // namespace blink
