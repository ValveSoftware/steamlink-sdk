/*
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoadRequest_h
#define FrameLoadRequest_h

#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/fetch/SubstituteData.h"
#include "core/loader/FrameLoaderTypes.h"

namespace blink {

class HTMLFormElement;
class ResourceRequest;
class SubstituteData;

struct CORE_EXPORT FrameLoadRequest {
    STACK_ALLOCATED();
public:
    explicit FrameLoadRequest(Document* originDocument);
    FrameLoadRequest(Document* originDocument, const ResourceRequest&);
    FrameLoadRequest(Document* originDocument, const ResourceRequest&, const AtomicString& frameName);
    FrameLoadRequest(Document* originDocument, const ResourceRequest&, const SubstituteData&);
    FrameLoadRequest(Document* originDocument, const ResourceRequest&, const AtomicString& frameName, ContentSecurityPolicyDisposition);

    Document* originDocument() const { return m_originDocument.get(); }

    ResourceRequest& resourceRequest() { return m_resourceRequest; }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }

    const AtomicString& frameName() const { return m_frameName; }
    void setFrameName(const AtomicString& frameName) { m_frameName = frameName; }

    const SubstituteData& substituteData() const { return m_substituteData; }

    bool replacesCurrentItem() const { return m_replacesCurrentItem; }
    void setReplacesCurrentItem(bool replacesCurrentItem) { m_replacesCurrentItem = replacesCurrentItem; }

    ClientRedirectPolicy clientRedirect() const { return m_clientRedirect; }
    void setClientRedirect(ClientRedirectPolicy clientRedirect) { m_clientRedirect = clientRedirect; }

    Event* triggeringEvent() const { return m_triggeringEvent.get(); }
    void setTriggeringEvent(Event* triggeringEvent) { m_triggeringEvent = triggeringEvent; }

    HTMLFormElement* form() const { return m_form.get(); }
    void setForm(HTMLFormElement* form) { m_form = form; }

    ShouldSendReferrer getShouldSendReferrer() const { return m_shouldSendReferrer; }
    void setShouldSendReferrer(ShouldSendReferrer shouldSendReferrer) { m_shouldSendReferrer = shouldSendReferrer; }

    ShouldSetOpener getShouldSetOpener() const { return m_shouldSetOpener; }
    void setShouldSetOpener(ShouldSetOpener shouldSetOpener) { m_shouldSetOpener = shouldSetOpener; }

    ContentSecurityPolicyDisposition shouldCheckMainWorldContentSecurityPolicy() const { return m_shouldCheckMainWorldContentSecurityPolicy; }

private:
    FrameLoadRequest(Document* originDocument, const ResourceRequest&, const AtomicString& frameName, const SubstituteData&, ContentSecurityPolicyDisposition);

    Member<Document> m_originDocument;
    ResourceRequest m_resourceRequest;
    AtomicString m_frameName;
    SubstituteData m_substituteData;
    bool m_replacesCurrentItem;
    ClientRedirectPolicy m_clientRedirect;
    Member<Event> m_triggeringEvent;
    Member<HTMLFormElement> m_form;
    ShouldSendReferrer m_shouldSendReferrer;
    ShouldSetOpener m_shouldSetOpener;
    ContentSecurityPolicyDisposition m_shouldCheckMainWorldContentSecurityPolicy;
};

} // namespace blink

#endif // FrameLoadRequest_h
