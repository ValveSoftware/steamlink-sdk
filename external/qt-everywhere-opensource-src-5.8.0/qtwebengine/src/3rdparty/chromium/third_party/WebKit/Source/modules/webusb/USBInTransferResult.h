// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBInTransferResult_h
#define USBInTransferResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMDataView.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBInTransferResult final
    : public GarbageCollectedFinalized<USBInTransferResult>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBInTransferResult* create(const String& status, const Vector<uint8_t>& data)
    {
        return new USBInTransferResult(status, data);
    }

    USBInTransferResult(const String& status, const Vector<uint8_t>& data)
        : m_status(status)
        , m_data(DOMDataView::create(DOMArrayBuffer::create(data.data(), data.size()), 0, data.size()))
    {
    }

    virtual ~USBInTransferResult() { }

    String status() const { return m_status; }
    DOMDataView* data() const { return m_data; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_data);
    }

private:
    const String m_status;
    const Member<DOMDataView> m_data;
};

} // namespace blink

#endif // USBInTransferResult_h
