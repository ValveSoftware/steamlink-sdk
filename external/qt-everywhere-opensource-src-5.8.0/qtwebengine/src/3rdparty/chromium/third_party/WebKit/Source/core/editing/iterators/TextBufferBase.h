// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextBufferBase_h
#define TextBufferBase_h

#include "core/CoreExport.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT TextBufferBase {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(TextBufferBase);
public:
    void clear() { m_size = 0; }
    size_t size() const { return m_size; }
    bool isEmpty() const { return m_size == 0; }
    size_t capacity() const { return m_buffer.capacity(); }
    const UChar& operator[](size_t index) const { DCHECK_LT(index, m_size); return data()[index]; }
    virtual const UChar* data() const = 0;

    void pushCharacters(UChar, size_t length);

    template<typename T>
    void pushRange(const T* other, size_t length)
    {
        if (length == 0)
            return;
        std::copy(other, other + length, ensureDestination(length));
    }

    void shrink(size_t delta) { DCHECK_LE(delta, m_size); m_size -= delta; }

protected:
    TextBufferBase();
    UChar* ensureDestination(size_t length);
    void grow(size_t demand);

    virtual UChar* calcDestination(size_t length) = 0;
    virtual void shiftData(size_t oldCapacity);

    const UChar* bufferBegin() const { return m_buffer.begin(); }
    const UChar* bufferEnd() const { return m_buffer.end(); }
    UChar* bufferBegin() { return m_buffer.begin(); }
    UChar* bufferEnd() { return m_buffer.end(); }

private:
    size_t m_size = 0;
    Vector<UChar, 1024> m_buffer;
};

} // namespace blink

#endif // TextBufferBase_h
