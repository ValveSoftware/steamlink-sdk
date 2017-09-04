// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBAlternateInterface_h
#define USBAlternateInterface_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBEndpoint;
class USBInterface;

class USBAlternateInterface : public GarbageCollected<USBAlternateInterface>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBAlternateInterface* create(const USBInterface*,
                                       size_t alternateIndex);
  static USBAlternateInterface* create(const USBInterface*,
                                       size_t alternateSetting,
                                       ExceptionState&);

  USBAlternateInterface(const USBInterface*, size_t alternateIndex);

  const device::usb::blink::AlternateInterfaceInfo& info() const;

  uint8_t alternateSetting() const { return info().alternate_setting; }
  uint8_t interfaceClass() const { return info().class_code; }
  uint8_t interfaceSubclass() const { return info().subclass_code; }
  uint8_t interfaceProtocol() const { return info().protocol_code; }
  String interfaceName() const { return info().interface_name; }
  HeapVector<Member<USBEndpoint>> endpoints() const;

  DECLARE_TRACE();

 private:
  Member<const USBInterface> m_interface;
  const size_t m_alternateIndex;
};

}  // namespace blink

#endif  // USBAlternateInterface_h
