/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/web/WebBlob.h"

#include "V8Blob.h"
#include "bindings/v8/V8Binding.h"
#include "core/fileapi/Blob.h"
#include "platform/blob/BlobData.h"
#include "wtf/PassOwnPtr.h"

using namespace WebCore;

namespace blink {

WebBlob WebBlob::createFromUUID(const WebString& uuid, const WebString& type, long long size)
{
    RefPtrWillBeRawPtr<Blob> blob = Blob::create(BlobDataHandle::create(uuid, type, size));
    return WebBlob(blob);
}

WebBlob WebBlob::createFromFile(const WebString& path, long long size)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->appendFile(path);
    RefPtrWillBeRawPtr<Blob> blob = Blob::create(BlobDataHandle::create(blobData.release(), size));
    return WebBlob(blob);
}

WebBlob WebBlob::fromV8Value(v8::Handle<v8::Value> value)
{
    if (V8Blob::hasInstance(value, v8::Isolate::GetCurrent())) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        Blob* blob = V8Blob::toNative(object);
        ASSERT(blob);
        return WebBlob(blob);
    }
    return WebBlob();
}

void WebBlob::reset()
{
    m_private.reset();
}

void WebBlob::assign(const WebBlob& other)
{
    m_private = other.m_private;
}

WebString WebBlob::uuid()
{
    if (!m_private.get())
        return WebString();
    return m_private->uuid();
}

v8::Handle<v8::Value> WebBlob::toV8Value(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!m_private.get())
        return v8::Handle<v8::Value>();
    return toV8(m_private.get(), creationContext, isolate);
}

WebBlob::WebBlob(const PassRefPtrWillBeRawPtr<WebCore::Blob>& blob)
    : m_private(blob)
{
}

WebBlob& WebBlob::operator=(const PassRefPtrWillBeRawPtr<WebCore::Blob>& blob)
{
    m_private = blob;
    return *this;
}

} // namespace blink
