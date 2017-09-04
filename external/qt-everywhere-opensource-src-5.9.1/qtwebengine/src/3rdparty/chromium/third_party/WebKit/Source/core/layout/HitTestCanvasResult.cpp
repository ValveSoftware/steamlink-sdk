// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "HitTestCanvasResult.h"

namespace blink {

HitTestCanvasResult::HitTestCanvasResult(String id, Member<Element> control)
    : m_id(id), m_control(control) {}

String HitTestCanvasResult::getId() const {
  return m_id;
}

Element* HitTestCanvasResult::getControl() const {
  return m_control.get();
};

DEFINE_TRACE(HitTestCanvasResult) {
  visitor->trace(m_control);
}

}  // namespace blink
