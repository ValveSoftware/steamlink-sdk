// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousOutTransferPacket_h
#define USBIsochronousOutTransferPacket_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBIsochronousOutTransferPacket final : public GarbageCollectedFinalized<USBIsochronousOutTransferPacket>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBIsochronousOutTransferPacket* create(const String& status, unsigned bytesWritten)
    {
        return new USBIsochronousOutTransferPacket(status, bytesWritten);
    }

    USBIsochronousOutTransferPacket(const String& status, unsigned bytesWritten)
        : m_status(status)
        , m_bytesWritten(bytesWritten)
    {
    }

    virtual ~USBIsochronousOutTransferPacket() {}

    String status() const { return m_status; }
    unsigned bytesWritten() const { return m_bytesWritten; }

    DEFINE_INLINE_TRACE() {}

private:
    const String m_status;
    const unsigned m_bytesWritten;
};

} // namespace blink

#endif // USBIsochronousOutTransferPacket_h
