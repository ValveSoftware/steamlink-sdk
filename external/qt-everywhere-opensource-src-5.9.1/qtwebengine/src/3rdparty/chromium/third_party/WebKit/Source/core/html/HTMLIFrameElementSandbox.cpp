// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementSandbox.h"

#include "core/html/HTMLIFrameElement.h"

namespace blink {

namespace {

const char* kSupportedTokens[] = {"allow-forms",
                                  "allow-modals",
                                  "allow-pointer-lock",
                                  "allow-popups",
                                  "allow-popups-to-escape-sandbox",
                                  "allow-same-origin",
                                  "allow-scripts",
                                  "allow-top-navigation"};

bool isTokenSupported(const AtomicString& token) {
  for (const char* supportedToken : kSupportedTokens) {
    if (token == supportedToken)
      return true;
  }
  return false;
}

}  // namespace

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(HTMLIFrameElement* element)
    : DOMTokenList(this), m_element(element) {}

HTMLIFrameElementSandbox::~HTMLIFrameElementSandbox() {}

DEFINE_TRACE(HTMLIFrameElementSandbox) {
  visitor->trace(m_element);
  DOMTokenList::trace(visitor);
  DOMTokenListObserver::trace(visitor);
}

bool HTMLIFrameElementSandbox::validateTokenValue(
    const AtomicString& tokenValue,
    ExceptionState&) const {
  return isTokenSupported(tokenValue);
}

void HTMLIFrameElementSandbox::valueWasSet() {
  m_element->sandboxValueWasSet();
}

}  // namespace blink
