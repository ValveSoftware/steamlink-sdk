// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMDataView.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "platform/CheckedInt.h"
#include "wtf/typed_arrays/ArrayBufferView.h"

namespace blink {

namespace {

class DataView final : public ArrayBufferView {
public:
    static PassRefPtr<DataView> create(ArrayBuffer* buffer, unsigned byteOffset, unsigned byteLength)
    {
        RELEASE_ASSERT(byteOffset <= buffer->byteLength());
        CheckedInt<uint32_t> checkedOffset(byteOffset);
        CheckedInt<uint32_t> checkedLength(byteLength);
        CheckedInt<uint32_t> checkedMax = checkedOffset + checkedLength;
        RELEASE_ASSERT(checkedMax.isValid());
        RELEASE_ASSERT(checkedMax.value() <= buffer->byteLength());
        return adoptRef(new DataView(buffer, byteOffset, byteLength));
    }

    unsigned byteLength() const override { return m_byteLength; }
    ViewType type() const override { return TypeDataView; }

protected:
    void neuter() override
    {
        ArrayBufferView::neuter();
        m_byteLength = 0;
    }

private:
    DataView(ArrayBuffer* buffer, unsigned byteOffset, unsigned byteLength)
        : ArrayBufferView(buffer, byteOffset)
        , m_byteLength(byteLength) { }

    unsigned m_byteLength;
};

} // anonymous namespace

DOMDataView* DOMDataView::create(DOMArrayBufferBase* buffer, unsigned byteOffset, unsigned byteLength)
{
    RefPtr<DataView> dataView = DataView::create(buffer->buffer(), byteOffset, byteLength);
    return new DOMDataView(dataView, buffer);
}

v8::Local<v8::Object> DOMDataView::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    DCHECK(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Local<v8::Value> v8Buffer = toV8(buffer(), creationContext, isolate);
    if (v8Buffer.IsEmpty())
        return v8::Local<v8::Object>();
    DCHECK(v8Buffer->IsArrayBuffer());

    v8::Local<v8::Object> wrapper = v8::DataView::New(v8Buffer.As<v8::ArrayBuffer>(), byteOffset(), byteLength());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

} // namespace blink
