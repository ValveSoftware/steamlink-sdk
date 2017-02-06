// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousInTransferPacket_h
#define USBIsochronousInTransferPacket_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMDataView.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBIsochronousInTransferPacket final : public GarbageCollectedFinalized<USBIsochronousInTransferPacket>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBIsochronousInTransferPacket* create(const String& status, DOMDataView* data)
    {
        return new USBIsochronousInTransferPacket(status, data);
    }

    ~USBIsochronousInTransferPacket() {}

    String status() const { return m_status; }
    DOMDataView* data() const { return m_data; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_data);
    }

private:
    USBIsochronousInTransferPacket(const String& status, DOMDataView* data)
        : m_status(status)
        , m_data(data)
    {
    }

    const String m_status;
    const Member<DOMDataView> m_data;
};

} // namespace blink

#endif // USBIsochronousInTransferPacket_h
