// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGAnimatedString.h"

namespace blink {

String SVGAnimatedString::baseVal() {
  return SVGAnimatedProperty<SVGString>::baseVal();
}

void SVGAnimatedString::setBaseVal(const String& value,
                                   ExceptionState& exceptionState) {
  return SVGAnimatedProperty<SVGString>::setBaseVal(value, exceptionState);
}

String SVGAnimatedString::animVal() {
  return SVGAnimatedProperty<SVGString>::animVal();
}

DEFINE_TRACE_WRAPPERS(SVGAnimatedString) {
  visitor->traceWrappers(contextElement());
}

}  // namespace blink
