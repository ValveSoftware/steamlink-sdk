// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPathValue.h"

#include "core/style/StylePath.h"
#include "core/svg/SVGPathUtilities.h"
#include <memory>

namespace blink {

CSSPathValue* CSSPathValue::create(PassRefPtr<StylePath> stylePath) {
  return new CSSPathValue(std::move(stylePath));
}

CSSPathValue* CSSPathValue::create(
    std::unique_ptr<SVGPathByteStream> pathByteStream) {
  return CSSPathValue::create(StylePath::create(std::move(pathByteStream)));
}

CSSPathValue::CSSPathValue(PassRefPtr<StylePath> stylePath)
    : CSSValue(PathClass), m_stylePath(stylePath) {
  ASSERT(m_stylePath);
}

namespace {

CSSPathValue* createPathValue() {
  std::unique_ptr<SVGPathByteStream> pathByteStream =
      SVGPathByteStream::create();
  // Need to be registered as LSan ignored, as it will be reachable and
  // separately referred to by emptyPathValue() callers.
  LEAK_SANITIZER_IGNORE_OBJECT(pathByteStream.get());
  return CSSPathValue::create(std::move(pathByteStream));
}

}  // namespace

CSSPathValue& CSSPathValue::emptyPathValue() {
  DEFINE_STATIC_LOCAL(CSSPathValue, empty, (createPathValue()));
  return empty;
}

String CSSPathValue::customCSSText() const {
  return "path('" + buildStringFromByteStream(byteStream()) + "')";
}

bool CSSPathValue::equals(const CSSPathValue& other) const {
  return byteStream() == other.byteStream();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPathValue) {
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
