// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBuffer_h
#define DOMArrayBuffer_h

#include "core/CoreExport.h"
#include "core/dom/DOMArrayBufferBase.h"
#include "wtf/typed_arrays/ArrayBuffer.h"

namespace blink {

class CORE_EXPORT DOMArrayBuffer final : public DOMArrayBufferBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    static DOMArrayBuffer* create(PassRefPtr<WTF::ArrayBuffer> buffer)
    {
        return new DOMArrayBuffer(buffer);
    }
    static DOMArrayBuffer* create(unsigned numElements, unsigned elementByteSize)
    {
        return create(WTF::ArrayBuffer::create(numElements, elementByteSize));
    }
    static DOMArrayBuffer* create(const void* source, unsigned byteLength)
    {
        return create(WTF::ArrayBuffer::create(source, byteLength));
    }
    static DOMArrayBuffer* create(WTF::ArrayBufferContents& contents)
    {
        return create(WTF::ArrayBuffer::create(contents));
    }

    // Only for use by XMLHttpRequest::responseArrayBuffer and
    // Internals::serializeObject.
    static DOMArrayBuffer* createUninitialized(unsigned numElements, unsigned elementByteSize)
    {
        return create(WTF::ArrayBuffer::createUninitialized(numElements, elementByteSize));
    }

    DOMArrayBuffer* slice(int begin, int end) const
    {
        return create(buffer()->slice(begin, end));
    }
    DOMArrayBuffer* slice(int begin) const
    {
        return create(buffer()->slice(begin));
    }

    v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext) override;

private:
    explicit DOMArrayBuffer(PassRefPtr<WTF::ArrayBuffer> buffer)
        : DOMArrayBufferBase(buffer)
    {
    }
};

} // namespace blink

#endif // DOMArrayBuffer_h
