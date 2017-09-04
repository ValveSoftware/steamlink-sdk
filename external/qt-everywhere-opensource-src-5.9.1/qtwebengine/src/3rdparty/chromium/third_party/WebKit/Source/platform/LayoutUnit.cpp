// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LayoutUnit.h"

#include "wtf/text/WTFString.h"

namespace blink {

String LayoutUnit::toString() const {
  if (m_value == LayoutUnit::max().rawValue())
    return "LayoutUnit::max(" + String::number(toDouble()) + ")";
  if (m_value == LayoutUnit::min().rawValue())
    return "LayoutUnit::min(" + String::number(toDouble()) + ")";
  if (m_value == LayoutUnit::nearlyMax().rawValue())
    return "LayoutUnit::nearlyMax(" + String::number(toDouble()) + ")";
  if (m_value == LayoutUnit::nearlyMin().rawValue())
    return "LayoutUnit::nearlyMin(" + String::number(toDouble()) + ")";
  return String::number(toDouble());
}

}  // namespace blink
