// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousInTransferResult_h
#define USBIsochronousInTransferResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMDataView.h"
#include "modules/webusb/USBIsochronousInTransferPacket.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBIsochronousInTransferResult final : public GarbageCollectedFinalized<USBIsochronousInTransferResult>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBIsochronousInTransferResult* create(DOMArrayBuffer* data, const HeapVector<Member<USBIsochronousInTransferPacket>>& packets)
    {
        return new USBIsochronousInTransferResult(data, packets);
    }

    USBIsochronousInTransferResult(DOMArrayBuffer* data, const HeapVector<Member<USBIsochronousInTransferPacket>>& packets)
        : m_packets(packets)
    {
        unsigned byteLength = data->byteLength();
        m_data = DOMDataView::create(data, 0, byteLength);
    }

    virtual ~USBIsochronousInTransferResult() {}

    DOMDataView* data() const { return m_data; }
    const HeapVector<Member<USBIsochronousInTransferPacket>>& packets() const
    {
        return m_packets;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_data);
        visitor->trace(m_packets);
    }

private:
    Member<DOMDataView> m_data;
    const HeapVector<Member<USBIsochronousInTransferPacket>> m_packets;
};

} // namespace blink

#endif // USBIsochronousInTransferResult_h
