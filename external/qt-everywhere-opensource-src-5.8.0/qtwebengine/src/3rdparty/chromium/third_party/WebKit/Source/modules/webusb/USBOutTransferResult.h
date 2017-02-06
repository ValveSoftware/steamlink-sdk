// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBOutTransferResult_h
#define USBOutTransferResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBOutTransferResult final
    : public GarbageCollectedFinalized<USBOutTransferResult>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBOutTransferResult* create(const String& status, unsigned bytesWritten)
    {
        return new USBOutTransferResult(status, bytesWritten);
    }

    USBOutTransferResult(const String& status, unsigned bytesWritten)
        : m_status(status)
        , m_bytesWritten(bytesWritten)
    {
    }

    virtual ~USBOutTransferResult() { }

    String status() const { return m_status; }
    unsigned bytesWritten() const { return m_bytesWritten; }

    DEFINE_INLINE_TRACE() { }

private:
    const String m_status;
    const unsigned m_bytesWritten;
};

} // namespace blink

#endif // USBOutTransferResult_h
