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

#ifndef Blob_h
#define Blob_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/URLRegistry.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class ExecutionContext;

class Blob : public RefCountedWillBeGarbageCollectedFinalized<Blob>, public ScriptWrappable, public URLRegistrable {
public:
    static PassRefPtrWillBeRawPtr<Blob> create()
    {
        return adoptRefWillBeNoop(new Blob(BlobDataHandle::create()));
    }

    static PassRefPtrWillBeRawPtr<Blob> create(PassRefPtr<BlobDataHandle> blobDataHandle)
    {
        return adoptRefWillBeNoop(new Blob(blobDataHandle));
    }

    virtual ~Blob();

    virtual unsigned long long size() const { return m_blobDataHandle->size(); }
    virtual PassRefPtrWillBeRawPtr<Blob> slice(long long start, long long end, const String& contentType, ExceptionState&) const;

    // To allow ExceptionState to be passed in last, manually enumerate the optional argument overloads.
    PassRefPtrWillBeRawPtr<Blob> slice(ExceptionState& exceptionState) const
    {
        return slice(0, std::numeric_limits<long long>::max(), String(), exceptionState);
    }
    PassRefPtrWillBeRawPtr<Blob> slice(long long start, ExceptionState& exceptionState) const
    {
        return slice(start, std::numeric_limits<long long>::max(), String(), exceptionState);
    }
    PassRefPtrWillBeRawPtr<Blob> slice(long long start, long long end, ExceptionState& exceptionState) const
    {
        return slice(start, end, String(), exceptionState);
    }

    virtual void close(ExecutionContext*, ExceptionState&);

    String type() const { return m_blobDataHandle->type(); }
    String uuid() const { return m_blobDataHandle->uuid(); }
    PassRefPtr<BlobDataHandle> blobDataHandle() const { return m_blobDataHandle; }
    // True for all File instances, including the user-built ones.
    virtual bool isFile() const { return false; }
    // Only true for File instances that are backed by platform files.
    virtual bool hasBackingFile() const { return false; }
    bool hasBeenClosed() const { return m_hasBeenClosed; }

    // Used by the JavaScript Blob and File constructors.
    virtual void appendTo(BlobData&) const;

    // URLRegistrable to support PublicURLs.
    virtual URLRegistry& registry() const OVERRIDE FINAL;

    void trace(Visitor*) { }

protected:
    explicit Blob(PassRefPtr<BlobDataHandle>);

    static void clampSliceOffsets(long long size, long long& start, long long& end);
private:
    Blob();

    RefPtr<BlobDataHandle> m_blobDataHandle;
    bool m_hasBeenClosed;
};

} // namespace WebCore

#endif // Blob_h
