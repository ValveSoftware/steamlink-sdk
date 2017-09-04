// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplayCapabilities.h"

namespace blink {

VRDisplayCapabilities::VRDisplayCapabilities()
    : m_hasOriention(false),
      m_hasPosition(false),
      m_hasExternalDisplay(false),
      m_canPresent(false) {}

DEFINE_TRACE(VRDisplayCapabilities) {}

}  // namespace blink
