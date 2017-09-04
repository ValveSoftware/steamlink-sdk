// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRuleNamespace_h
#define StyleRuleNamespace_h

#include "core/css/StyleRule.h"

namespace blink {

// This class is never actually stored anywhere currently, but only used for
// the parser to pass to a stylesheet
class StyleRuleNamespace final : public StyleRuleBase {
 public:
  static StyleRuleNamespace* create(AtomicString prefix, AtomicString uri) {
    return new StyleRuleNamespace(prefix, uri);
  }

  StyleRuleNamespace* copy() const {
    return new StyleRuleNamespace(m_prefix, m_uri);
  }

  AtomicString prefix() const { return m_prefix; }
  AtomicString uri() const { return m_uri; }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleBase::traceAfterDispatch(visitor);
  }

 private:
  StyleRuleNamespace(AtomicString prefix, AtomicString uri)
      : StyleRuleBase(Namespace), m_prefix(prefix), m_uri(uri) {}

  AtomicString m_prefix;
  AtomicString m_uri;
};

DEFINE_STYLE_RULE_TYPE_CASTS(Namespace);

}  // namespace blink

#endif  // StyleRuleNamespace_h
