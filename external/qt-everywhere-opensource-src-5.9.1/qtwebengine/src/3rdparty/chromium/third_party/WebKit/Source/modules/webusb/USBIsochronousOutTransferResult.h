// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousOutTransferResult_h
#define USBIsochronousOutTransferResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMDataView.h"
#include "modules/webusb/USBIsochronousOutTransferPacket.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBIsochronousOutTransferResult final
    : public GarbageCollectedFinalized<USBIsochronousOutTransferResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBIsochronousOutTransferResult* create(
      const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets) {
    return new USBIsochronousOutTransferResult(packets);
  }

  USBIsochronousOutTransferResult(
      const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets)
      : m_packets(packets) {}

  virtual ~USBIsochronousOutTransferResult() {}

  const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets() const {
    return m_packets;
  }

  DEFINE_INLINE_TRACE() { visitor->trace(m_packets); }

 private:
  const HeapVector<Member<USBIsochronousOutTransferPacket>> m_packets;
};

}  // namespace blink

#endif  // USBIsochronousOutTransferResult_h
