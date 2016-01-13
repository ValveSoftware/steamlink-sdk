/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XMLHttpRequestUpload_h
#define XMLHttpRequestUpload_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventListener.h"
#include "core/xml/XMLHttpRequest.h"
#include "core/xml/XMLHttpRequestEventTarget.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class ExecutionContext;
class XMLHttpRequest;

class XMLHttpRequestUpload FINAL : public NoBaseWillBeRefCountedGarbageCollected<XMLHttpRequestUpload>, public ScriptWrappable, public XMLHttpRequestEventTarget {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(XMLHttpRequestUpload);
public:
    static PassOwnPtrWillBeRawPtr<XMLHttpRequestUpload> create(XMLHttpRequest* xmlHttpRequest)
    {
        return adoptPtrWillBeRefCountedGarbageCollected(new XMLHttpRequestUpload(xmlHttpRequest));
    }

#if !ENABLE(OILPAN)
    void ref() { m_xmlHttpRequest->ref(); }
    void deref() { m_xmlHttpRequest->deref(); }
#endif

    XMLHttpRequest* xmlHttpRequest() const { return m_xmlHttpRequest; }

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    void dispatchEventAndLoadEnd(const AtomicString&, bool, unsigned long long, unsigned long long);
    void dispatchProgressEvent(unsigned long long, unsigned long long);

    void handleRequestError(const AtomicString&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit XMLHttpRequestUpload(XMLHttpRequest*);

#if !ENABLE(OILPAN)
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
#endif

    RawPtrWillBeMember<XMLHttpRequest> m_xmlHttpRequest;
    EventTargetData m_eventTargetData;

    // Last progress event values; used when issuing the
    // required 'progress' event on a request error or abort.
    unsigned long long m_lastBytesSent;
    unsigned long long m_lastTotalBytesToBeSent;
};

} // namespace WebCore

#endif // XMLHttpRequestUpload_h
