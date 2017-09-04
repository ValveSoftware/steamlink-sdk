// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplayEvent_h
#define VRDisplayEvent_h

#include "modules/EventModules.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRDisplayEventInit.h"

namespace blink {

class VRDisplayEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRDisplayEvent* create() { return new VRDisplayEvent; }
  static VRDisplayEvent* create(const AtomicString& type,
                                bool canBubble,
                                bool cancelable,
                                VRDisplay* display,
                                String reason) {
    return new VRDisplayEvent(type, canBubble, cancelable, display, reason);
  }
  static VRDisplayEvent* create(const AtomicString& type,
                                bool canBubble,
                                bool cancelable,
                                VRDisplay*,
                                device::mojom::blink::VRDisplayEventReason);

  static VRDisplayEvent* create(const AtomicString& type,
                                const VRDisplayEventInit& initializer) {
    return new VRDisplayEvent(type, initializer);
  }

  ~VRDisplayEvent() override;

  VRDisplay* display() const { return m_display.get(); }
  const String& reason() const { return m_reason; }

  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  VRDisplayEvent();
  VRDisplayEvent(const AtomicString& type,
                 bool canBubble,
                 bool cancelable,
                 VRDisplay*,
                 String);
  VRDisplayEvent(const AtomicString&, const VRDisplayEventInit&);

  Member<VRDisplay> m_display;
  String m_reason;
};

}  // namespace blink

#endif  // VRDisplayEvent_h
