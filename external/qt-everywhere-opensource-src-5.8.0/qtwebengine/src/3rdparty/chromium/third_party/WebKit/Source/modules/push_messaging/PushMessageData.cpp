// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushMessageData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/ArrayBufferOrArrayBufferViewOrUSVString.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/fileapi/Blob.h"
#include "platform/blob/BlobData.h"
#include "wtf/Assertions.h"
#include "wtf/text/TextEncoding.h"
#include <memory>
#include <v8.h>

namespace blink {

PushMessageData* PushMessageData::create(const String& messageString)
{
    // The standard supports both an empty but valid message and a null message.
    // In case the message is explicitly null, return a null pointer which will
    // be set in the PushEvent.
    if (messageString.isNull())
        return nullptr;
    return PushMessageData::create(ArrayBufferOrArrayBufferViewOrUSVString::fromUSVString(messageString));
}

PushMessageData* PushMessageData::create(const ArrayBufferOrArrayBufferViewOrUSVString& messageData)
{
    if (messageData.isArrayBuffer() || messageData.isArrayBufferView()) {
        DOMArrayBuffer* buffer = messageData.isArrayBufferView()
            ? messageData.getAsArrayBufferView()->buffer()
            : messageData.getAsArrayBuffer();

        return new PushMessageData(static_cast<const char*>(buffer->data()), buffer->byteLength());
    }

    if (messageData.isUSVString()) {
        CString encodedString = UTF8Encoding().encode(messageData.getAsUSVString(), WTF::EntitiesForUnencodables);
        return new PushMessageData(encodedString.data(), encodedString.length());
    }

    DCHECK(messageData.isNull());
    return nullptr;
}

PushMessageData::PushMessageData(const char* data, unsigned bytesSize)
{
    m_data.append(data, bytesSize);
}

PushMessageData::~PushMessageData()
{
}

DOMArrayBuffer* PushMessageData::arrayBuffer() const
{
    return DOMArrayBuffer::create(m_data.data(), m_data.size());
}

Blob* PushMessageData::blob() const
{
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->appendBytes(m_data.data(), m_data.size());

    // Note that the content type of the Blob object is deliberately not being
    // provided, following the specification.

    const long long byteLength = blobData->length();
    return Blob::create(BlobDataHandle::create(std::move(blobData), byteLength));
}

ScriptValue PushMessageData::json(ScriptState* scriptState, ExceptionState& exceptionState) const
{
    v8::Isolate* isolate = scriptState->isolate();

    ScriptState::Scope scope(scriptState);
    v8::Local<v8::String> dataString = v8String(isolate, text());

    v8::TryCatch block(isolate);
    v8::Local<v8::Value> parsed;
    if (!v8Call(v8::JSON::Parse(isolate, dataString), parsed, block)) {
        exceptionState.rethrowV8Exception(block.Exception());
        return ScriptValue();
    }

    return ScriptValue(scriptState, parsed);
}

String PushMessageData::text() const
{
    return UTF8Encoding().decode(m_data.data(), m_data.size());
}

DEFINE_TRACE(PushMessageData)
{
}

} // namespace blink
