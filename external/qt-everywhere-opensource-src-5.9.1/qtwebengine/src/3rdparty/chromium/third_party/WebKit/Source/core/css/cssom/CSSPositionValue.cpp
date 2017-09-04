// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSPositionValue.h"

#include "core/css/CSSValuePair.h"
#include "core/css/cssom/CSSLengthValue.h"

namespace blink {

CSSValue* CSSPositionValue::toCSSValue() const {
  return CSSValuePair::create(m_x->toCSSValue(), m_y->toCSSValue(),
                              CSSValuePair::KeepIdenticalValues);
}

}  // namespace blink
