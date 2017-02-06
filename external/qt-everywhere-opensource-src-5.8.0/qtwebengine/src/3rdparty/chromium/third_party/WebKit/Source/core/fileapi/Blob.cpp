/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/fileapi/Blob.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMURL.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/BlobPropertyBag.h"
#include "core/frame/UseCounter.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"
#include <memory>

namespace blink {

namespace {

class BlobURLRegistry final : public URLRegistry {
public:
    void registerURL(SecurityOrigin*, const KURL&, URLRegistrable*) override;
    void unregisterURL(const KURL&) override;

    static URLRegistry& registry();
};

void BlobURLRegistry::registerURL(SecurityOrigin* origin, const KURL& publicURL, URLRegistrable* registrableObject)
{
    ASSERT(&registrableObject->registry() == this);
    Blob* blob = static_cast<Blob*>(registrableObject);
    BlobRegistry::registerPublicBlobURL(origin, publicURL, blob->blobDataHandle());
}

void BlobURLRegistry::unregisterURL(const KURL& publicURL)
{
    BlobRegistry::revokePublicBlobURL(publicURL);
}

URLRegistry& BlobURLRegistry::registry()
{
    // This is called on multiple threads.
    // (This code assumes it is safe to register or unregister URLs on
    // BlobURLRegistry (that is implemented by the embedder) on
    // multiple threads.)
    DEFINE_THREAD_SAFE_STATIC_LOCAL(BlobURLRegistry, instance, new BlobURLRegistry());
    return instance;
}

} // namespace

Blob::Blob(PassRefPtr<BlobDataHandle> dataHandle)
    : m_blobDataHandle(dataHandle)
    , m_isClosed(false)
{
}

Blob::~Blob()
{
}

// static
Blob* Blob::create(ExecutionContext* context, const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>& blobParts, const BlobPropertyBag& options, ExceptionState& exceptionState)
{
    ASSERT(options.hasType());
    if (!options.type().containsOnlyASCII()) {
        exceptionState.throwDOMException(SyntaxError, "The 'type' property must consist of ASCII characters.");
        return nullptr;
    }

    ASSERT(options.hasEndings());
    bool normalizeLineEndingsToNative = options.endings() == "native";
    if (normalizeLineEndingsToNative)
        UseCounter::count(context, UseCounter::FileAPINativeLineEndings);

    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->setContentType(options.type().lower());

    populateBlobData(blobData.get(), blobParts, normalizeLineEndingsToNative);

    long long blobSize = blobData->length();
    return new Blob(BlobDataHandle::create(std::move(blobData), blobSize));
}

Blob* Blob::create(const unsigned char* data, size_t bytes, const String& contentType)
{
    ASSERT(data);

    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    blobData->appendBytes(data, bytes);
    long long blobSize = blobData->length();

    return new Blob(BlobDataHandle::create(std::move(blobData), blobSize));
}

// static
void Blob::populateBlobData(BlobData* blobData, const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>& parts, bool normalizeLineEndingsToNative)
{
    for (const auto& item : parts) {
        if (item.isArrayBuffer()) {
            DOMArrayBuffer* arrayBuffer = item.getAsArrayBuffer();
            blobData->appendBytes(arrayBuffer->data(), arrayBuffer->byteLength());
        } else if (item.isArrayBufferView()) {
            DOMArrayBufferView* arrayBufferView = item.getAsArrayBufferView();
            blobData->appendBytes(arrayBufferView->baseAddress(), arrayBufferView->byteLength());
        } else if (item.isBlob()) {
            item.getAsBlob()->appendTo(*blobData);
        } else if (item.isUSVString()) {
            blobData->appendText(item.getAsUSVString(), normalizeLineEndingsToNative);
        } else {
            NOTREACHED();
        }
    }
}

// static
void Blob::clampSliceOffsets(long long size, long long& start, long long& end)
{
    ASSERT(size != -1);

    // Convert the negative value that is used to select from the end.
    if (start < 0)
        start = start + size;
    if (end < 0)
        end = end + size;

    // Clamp the range if it exceeds the size limit.
    if (start < 0)
        start = 0;
    if (end < 0)
        end = 0;
    if (start >= size) {
        start = 0;
        end = 0;
    } else if (end < start) {
        end = start;
    } else if (end > size) {
        end = size;
    }
}

Blob* Blob::slice(long long start, long long end, const String& contentType, ExceptionState& exceptionState) const
{
    if (isClosed()) {
        exceptionState.throwDOMException(InvalidStateError, "Blob has been closed.");
        return nullptr;
    }

    long long size = this->size();
    clampSliceOffsets(size, start, end);

    long long length = end - start;
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    blobData->appendBlob(m_blobDataHandle, start, length);
    return Blob::create(BlobDataHandle::create(std::move(blobData), length));
}

void Blob::close(ExecutionContext* executionContext, ExceptionState& exceptionState)
{
    if (isClosed()) {
        exceptionState.throwDOMException(InvalidStateError, "Blob has been closed.");
        return;
    }

    // Dereferencing a Blob that has been closed should result in
    // a network error. Revoke URLs registered against it through
    // its UUID.
    DOMURL::revokeObjectUUID(executionContext, uuid());

    // A Blob enters a 'readability state' of closed, where it will report its
    // size as zero. Blob and FileReader operations now throws on
    // being passed a Blob in that state. Downstream uses of closed Blobs
    // (e.g., XHR.send()) consider them as empty.
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->setContentType(type());
    m_blobDataHandle = BlobDataHandle::create(std::move(blobData), 0);
    m_isClosed = true;
}

void Blob::appendTo(BlobData& blobData) const
{
    blobData.appendBlob(m_blobDataHandle, 0, m_blobDataHandle->size());
}

URLRegistry& Blob::registry() const
{
    return BlobURLRegistry::registry();
}

} // namespace blink
