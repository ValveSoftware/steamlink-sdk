// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementPayments_h
#define HTMLIFrameElementPayments_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLIFrameElement;
class QualifiedName;

class HTMLIFrameElementPayments final
    : public GarbageCollected<HTMLIFrameElementPayments>,
      public Supplement<HTMLIFrameElement> {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElementPayments);

 public:
  static bool fastHasAttribute(const QualifiedName&, const HTMLIFrameElement&);
  static void setBooleanAttribute(const QualifiedName&,
                                  HTMLIFrameElement&,
                                  bool);
  static HTMLIFrameElementPayments& from(HTMLIFrameElement&);
  static bool allowPaymentRequest(HTMLIFrameElement&);

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLIFrameElementPayments();

  static const char* supplementName();
};

}  // namespace blink

#endif  // HTMLIFrameElementPayments_h
