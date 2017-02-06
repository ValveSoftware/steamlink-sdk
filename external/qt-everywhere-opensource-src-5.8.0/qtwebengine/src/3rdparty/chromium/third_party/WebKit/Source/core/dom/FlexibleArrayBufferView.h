// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FlexibleArrayBufferView_h
#define FlexibleArrayBufferView_h

#include "core/CoreExport.h"
#include "core/dom/DOMArrayBufferView.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT FlexibleArrayBufferView {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(FlexibleArrayBufferView);
public:
    FlexibleArrayBufferView()
        : m_smallData(nullptr)
        , m_smallLength(0)
    {
    }

    void setFull(DOMArrayBufferView* full) { m_full = full; }
    void setSmall(void* data, size_t length)
    {
        m_smallData = data;
        m_smallLength = length;
    }

    void clear()
    {
        m_full = nullptr;
        m_smallData = nullptr;
        m_smallLength = 0;
    }

    bool isEmpty() const { return !m_full && !m_smallData; }
    bool isFull() const { return m_full; }

    DOMArrayBufferView* full() const
    {
        DCHECK(isFull());
        return m_full;
    }

    // WARNING: The pointer returned by baseAddressMaybeOnStack() may point to
    // temporary storage that is only valid during the life-time of the
    // FlexibleArrayBufferView object.
    void* baseAddressMaybeOnStack() const
    {
        DCHECK(!isEmpty());
        return isFull() ? m_full->baseAddress() : m_smallData;
    }

    unsigned byteOffset() const
    {
        DCHECK(!isEmpty());
        return isFull() ? m_full->byteOffset() : 0;
    }

    unsigned byteLength() const
    {
        DCHECK(!isEmpty());
        return isFull() ? m_full->byteLength() : m_smallLength;
    }

    operator bool() const { return !isEmpty(); }

private:
    Member<DOMArrayBufferView> m_full;

    void* m_smallData;
    size_t m_smallLength;
};

} // namespace blink

#endif // FlexibleArrayBufferView_h
