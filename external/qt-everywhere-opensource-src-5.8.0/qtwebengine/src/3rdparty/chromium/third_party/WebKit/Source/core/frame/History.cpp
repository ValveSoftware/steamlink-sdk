/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "core/frame/History.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

bool equalIgnoringPathQueryAndFragment(const KURL& a, const KURL& b)
{
    int aLength = a.pathStart();
    int bLength = b.pathStart();

    if (aLength != bLength)
        return false;

    const String& aString = a.getString();
    const String& bString = b.getString();
    for (int i = 0; i < aLength; ++i) {
        if (aString[i] != bString[i])
            return false;
    }
    return true;
}

bool equalIgnoringQueryAndFragment(const KURL& a, const KURL& b)
{
    int aLength = a.pathEnd();
    int bLength = b.pathEnd();

    if (aLength != bLength)
        return false;

    const String& aString = a.getString();
    const String& bString = b.getString();
    for (int i = 0; i < aLength; ++i) {
        if (aString[i] != bString[i])
            return false;
    }
    return true;
}

}  // namespace

History::History(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , m_lastStateObjectRequested(nullptr)
{
}

DEFINE_TRACE(History)
{
    DOMWindowProperty::trace(visitor);
}

unsigned History::length() const
{
    if (!m_frame || !m_frame->loader().client())
        return 0;
    return m_frame->loader().client()->backForwardLength();
}

SerializedScriptValue* History::state()
{
    m_lastStateObjectRequested = stateInternal();
    return m_lastStateObjectRequested.get();
}

SerializedScriptValue* History::stateInternal() const
{
    if (!m_frame)
        return 0;

    if (HistoryItem* historyItem = m_frame->loader().currentItem())
        return historyItem->stateObject();

    return 0;
}

void History::setScrollRestoration(const String& value)
{
    ASSERT(value == "manual"  || value == "auto");
    if (!m_frame || !m_frame->loader().client() || !RuntimeEnabledFeatures::scrollRestorationEnabled())
        return;

    HistoryScrollRestorationType scrollRestoration = value == "manual" ? ScrollRestorationManual : ScrollRestorationAuto;
    if (scrollRestoration == scrollRestorationInternal())
        return;

    if (HistoryItem* historyItem = m_frame->loader().currentItem()) {
        historyItem->setScrollRestorationType(scrollRestoration);
        m_frame->loader().client()->didUpdateCurrentHistoryItem();
    }
}

String History::scrollRestoration()
{
    return scrollRestorationInternal() == ScrollRestorationManual ? "manual" : "auto";
}

HistoryScrollRestorationType History::scrollRestorationInternal() const
{
    if (m_frame && RuntimeEnabledFeatures::scrollRestorationEnabled()) {
        if (HistoryItem* historyItem = m_frame->loader().currentItem())
            return historyItem->scrollRestorationType();
    }

    return ScrollRestorationAuto;
}

bool History::stateChanged() const
{
    return m_lastStateObjectRequested != stateInternal();
}

bool History::isSameAsCurrentState(SerializedScriptValue* state) const
{
    return state == stateInternal();
}

void History::back(ExecutionContext* context)
{
    go(context, -1);
}

void History::forward(ExecutionContext* context)
{
    go(context, 1);
}

void History::go(ExecutionContext* context, int delta)
{
    if (!m_frame || !m_frame->loader().client())
        return;

    ASSERT(isMainThread());
    Document* activeDocument = toDocument(context);
    if (!activeDocument)
        return;

    if (!activeDocument->frame() || !activeDocument->frame()->canNavigate(*m_frame))
        return;
    if (!NavigationDisablerForBeforeUnload::isNavigationAllowed())
        return;

    // We intentionally call reload() for the current frame if delta is zero.
    // Otherwise, navigation happens on the root frame.
    // This behavior is designed in the following spec.
    // https://html.spec.whatwg.org/multipage/browsers.html#dom-history-go
    if (delta)
        m_frame->loader().client()->navigateBackForward(delta);
    else
        m_frame->reload(FrameLoadTypeReload, ClientRedirectPolicy::ClientRedirect);
}

KURL History::urlForState(const String& urlString)
{
    Document* document = m_frame->document();

    if (urlString.isNull())
        return document->url();
    if (urlString.isEmpty())
        return document->baseURL();

    return KURL(document->baseURL(), urlString);
}

bool History::canChangeToUrl(const KURL& url, SecurityOrigin* documentOrigin, const KURL& documentURL)
{
    if (!url.isValid())
        return false;

    if (documentOrigin->isGrantedUniversalAccess())
        return true;

    // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
    // 'pushState'/'replaceState' to modify the URL fragment: see
    // https://crbug.com/528681 for the compatibility concerns.
    if (documentOrigin->isUnique() || documentOrigin->isLocal())
        return equalIgnoringQueryAndFragment(url, documentURL);

    if (!equalIgnoringPathQueryAndFragment(url, documentURL))
        return false;

    RefPtr<SecurityOrigin> requestedOrigin = SecurityOrigin::create(url);
    if (requestedOrigin->isUnique() || !requestedOrigin->isSameSchemeHostPort(documentOrigin))
        return false;

    return true;
}

void History::stateObjectAdded(PassRefPtr<SerializedScriptValue> data, const String& /* title */, const String& urlString, HistoryScrollRestorationType restorationType, FrameLoadType type, ExceptionState& exceptionState)
{
    if (!m_frame || !m_frame->page() || !m_frame->loader().documentLoader())
        return;

    KURL fullURL = urlForState(urlString);
    if (!canChangeToUrl(fullURL, m_frame->document()->getSecurityOrigin(), m_frame->document()->url())) {
        // We can safely expose the URL to JavaScript, as a) no redirection takes place: JavaScript already had this URL, b) JavaScript can only access a same-origin History object.
        exceptionState.throwSecurityError("A history state object with URL '" + fullURL.elidedString() + "' cannot be created in a document with origin '" + m_frame->document()->getSecurityOrigin()->toString() + "' and URL '" + m_frame->document()->url().elidedString() + "'.");
        return;
    }

    m_frame->loader().updateForSameDocumentNavigation(fullURL, SameDocumentNavigationHistoryApi, data, restorationType, type, m_frame->document());
}

} // namespace blink
