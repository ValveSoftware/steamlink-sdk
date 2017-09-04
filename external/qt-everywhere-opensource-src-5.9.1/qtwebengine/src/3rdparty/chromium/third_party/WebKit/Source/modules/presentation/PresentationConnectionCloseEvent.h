// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionCloseEvent_h
#define PresentationConnectionCloseEvent_h

#include "modules/EventModules.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/heap/Handle.h"

namespace blink {

class PresentationConnectionCloseEventInit;

// Presentation API event to be fired when the state of a PresentationConnection
// has changed to 'closed'.
class PresentationConnectionCloseEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PresentationConnectionCloseEvent() override = default;

  static PresentationConnectionCloseEvent* create(const AtomicString& eventType,
                                                  const String& reason,
                                                  const String& message) {
    return new PresentationConnectionCloseEvent(eventType, reason, message);
  }

  static PresentationConnectionCloseEvent* create(
      const AtomicString& eventType,
      const PresentationConnectionCloseEventInit& initializer) {
    return new PresentationConnectionCloseEvent(eventType, initializer);
  }

  const String& reason() const { return m_reason; }
  const String& message() const { return m_message; }

  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  PresentationConnectionCloseEvent(const AtomicString& eventType,
                                   const String& reason,
                                   const String& message);
  PresentationConnectionCloseEvent(
      const AtomicString& eventType,
      const PresentationConnectionCloseEventInit& initializer);

  String m_reason;
  String m_message;
};

}  // namespace blink

#endif  // PresentationConnectionAvailableEvent_h
