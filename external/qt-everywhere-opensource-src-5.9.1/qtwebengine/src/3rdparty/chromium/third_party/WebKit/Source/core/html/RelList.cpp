// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/RelList.h"

#include "core/dom/Document.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/HashMap.h"

namespace blink {

using namespace HTMLNames;

RelList::RelList(Element* element)
    : DOMTokenList(nullptr), m_element(element) {}

unsigned RelList::length() const {
  return !m_element->fastGetAttribute(relAttr).isEmpty() ? m_relValues.size()
                                                         : 0;
}

const AtomicString RelList::item(unsigned index) const {
  if (index >= length())
    return AtomicString();
  return m_relValues[index];
}

bool RelList::containsInternal(const AtomicString& token) const {
  return !m_element->fastGetAttribute(relAttr).isEmpty() &&
         m_relValues.contains(token);
}

void RelList::setRelValues(const AtomicString& value) {
  m_relValues.set(value, SpaceSplitString::ShouldNotFoldCase);
}

static HashSet<AtomicString>& supportedTokens() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens, ());

  if (tokens.isEmpty()) {
    tokens = {
        "preload",
        "preconnect",
        "dns-prefetch",
        "stylesheet",
        "import",
        "icon",
        "alternate",
        "prefetch",
        "prerender",
        "next",
        "manifest",
        "apple-touch-icon",
        "apple-touch-icon-precomposed",
    };
  }

  return tokens;
}

bool RelList::validateTokenValue(const AtomicString& tokenValue,
                                 ExceptionState&) const {
  if (supportedTokens().contains(tokenValue))
    return true;
  return OriginTrials::linkServiceWorkerEnabled(
             m_element->getExecutionContext()) &&
         tokenValue == "serviceworker";
}

DEFINE_TRACE(RelList) {
  visitor->trace(m_element);
  DOMTokenList::trace(visitor);
}

}  // namespace blink
