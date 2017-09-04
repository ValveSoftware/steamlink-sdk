// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/HTMLIFrameElementPayments.h"

#include "core/dom/QualifiedName.h"
#include "core/html/HTMLIFrameElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

HTMLIFrameElementPayments::HTMLIFrameElementPayments() {}

// static
const char* HTMLIFrameElementPayments::supplementName() {
  return "HTMLIFrameElementPayments";
}

// static
bool HTMLIFrameElementPayments::fastHasAttribute(
    const QualifiedName& name,
    const HTMLIFrameElement& element) {
  DCHECK(name == HTMLNames::allowpaymentrequestAttr);
  return element.fastHasAttribute(name);
}

// static
void HTMLIFrameElementPayments::setBooleanAttribute(const QualifiedName& name,
                                                    HTMLIFrameElement& element,
                                                    bool value) {
  DCHECK(name == HTMLNames::allowpaymentrequestAttr);
  element.setBooleanAttribute(name, value);
}

// static
HTMLIFrameElementPayments& HTMLIFrameElementPayments::from(
    HTMLIFrameElement& iframe) {
  HTMLIFrameElementPayments* supplement =
      static_cast<HTMLIFrameElementPayments*>(
          Supplement<HTMLIFrameElement>::from(iframe, supplementName()));
  if (!supplement) {
    supplement = new HTMLIFrameElementPayments();
    provideTo(iframe, supplementName(), supplement);
  }
  return *supplement;
}

// static
bool HTMLIFrameElementPayments::allowPaymentRequest(
    HTMLIFrameElement& element) {
  return RuntimeEnabledFeatures::paymentRequestIFrameEnabled() &&
         element.fastHasAttribute(HTMLNames::allowpaymentrequestAttr);
}

DEFINE_TRACE(HTMLIFrameElementPayments) {
  Supplement<HTMLIFrameElement>::trace(visitor);
}

}  // namespace blink
