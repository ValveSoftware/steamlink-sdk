// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSURIValue.h"

#include "core/css/CSSMarkup.h"
#include "core/dom/Document.h"
#include "core/svg/SVGElementProxy.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

CSSURIValue::CSSURIValue(const String& urlString)
    : CSSValue(URIClass), m_url(urlString) {}

CSSURIValue::~CSSURIValue() {}

SVGElementProxy& CSSURIValue::ensureElementProxy(Document& document) const {
  if (m_proxy)
    return *m_proxy;
  SVGURLReferenceResolver resolver(m_url, document);
  AtomicString fragmentId = resolver.fragmentIdentifier();
  if (resolver.isLocal()) {
    m_proxy = SVGElementProxy::create(fragmentId);
  } else {
    m_proxy =
        SVGElementProxy::create(resolver.absoluteUrl().getString(), fragmentId);
  }
  return *m_proxy;
}

String CSSURIValue::customCSSText() const {
  return serializeURI(m_url);
}

bool CSSURIValue::equals(const CSSURIValue& other) const {
  return m_url == other.m_url;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSURIValue) {
  visitor->trace(m_proxy);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
