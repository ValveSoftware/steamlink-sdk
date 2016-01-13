/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *           (C) 2008 Apple Inc.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/storage/StorageArea.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Page.h"
#include "core/page/StorageClient.h"
#include "core/storage/Storage.h"
#include "core/storage/StorageEvent.h"
#include "core/storage/StorageNamespace.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebStorageArea.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace WebCore {

StorageArea::StorageArea(PassOwnPtr<blink::WebStorageArea> storageArea, StorageType storageType)
    : m_storageArea(storageArea)
    , m_storageType(storageType)
    , m_canAccessStorageCachedResult(false)
    , m_canAccessStorageCachedFrame(0)
{
}

StorageArea::~StorageArea()
{
}

unsigned StorageArea::length(ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return 0;
    }
    return m_storageArea->length();
}

String StorageArea::key(unsigned index, ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return String();
    }
    return m_storageArea->key(index);
}

String StorageArea::getItem(const String& key, ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return String();
    }
    return m_storageArea->getItem(key);
}

void StorageArea::setItem(const String& key, const String& value, ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return;
    }
    blink::WebStorageArea::Result result = blink::WebStorageArea::ResultOK;
    m_storageArea->setItem(key, value, frame->document()->url(), result);
    if (result != blink::WebStorageArea::ResultOK)
        exceptionState.throwDOMException(QuotaExceededError, "Setting the value of '" + key + "' exceeded the quota.");
}

void StorageArea::removeItem(const String& key, ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return;
    }
    m_storageArea->removeItem(key, frame->document()->url());
}

void StorageArea::clear(ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return;
    }
    m_storageArea->clear(frame->document()->url());
}

bool StorageArea::contains(const String& key, ExceptionState& exceptionState, LocalFrame* frame)
{
    if (!canAccessStorage(frame)) {
        exceptionState.throwSecurityError("access is denied for this document.");
        return false;
    }
    return !getItem(key, exceptionState, frame).isNull();
}

bool StorageArea::canAccessStorage(LocalFrame* frame)
{
    if (!frame || !frame->page())
        return false;
    if (m_canAccessStorageCachedFrame == frame)
        return m_canAccessStorageCachedResult;
    bool result = frame->page()->storageClient().canAccessStorage(frame, m_storageType);
    m_canAccessStorageCachedFrame = frame;
    m_canAccessStorageCachedResult = result;
    return result;
}

size_t StorageArea::memoryBytesUsedByCache()
{
    return m_storageArea->memoryBytesUsedByCache();
}

void StorageArea::dispatchLocalStorageEvent(const String& key, const String& oldValue, const String& newValue, SecurityOrigin* securityOrigin, const KURL& pageURL, blink::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    // FIXME: This looks suspicious. Why doesn't this use allPages instead?
    const HashSet<Page*>& pages = Page::ordinaryPages();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            // FIXME: We do not yet have a way to dispatch events to out-of-process frames.
            if (!frame->isLocalFrame())
                continue;
            Storage* storage = frame->domWindow()->optionalLocalStorage();
            if (storage && toLocalFrame(frame)->document()->securityOrigin()->canAccess(securityOrigin) && !isEventSource(storage, sourceAreaInstance))
                frame->domWindow()->enqueueWindowEvent(StorageEvent::create(EventTypeNames::storage, key, oldValue, newValue, pageURL, storage));
        }
        InspectorInstrumentation::didDispatchDOMStorageEvent(*it, key, oldValue, newValue, LocalStorage, securityOrigin);
    }
}

static Page* findPageWithSessionStorageNamespace(const blink::WebStorageNamespace& sessionNamespace)
{
    // FIXME: This looks suspicious. Why doesn't this use allPages instead?
    const HashSet<Page*>& pages = Page::ordinaryPages();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        const bool dontCreateIfMissing = false;
        StorageNamespace* storageNamespace = (*it)->sessionStorage(dontCreateIfMissing);
        if (storageNamespace && storageNamespace->isSameNamespace(sessionNamespace))
            return *it;
    }
    return 0;
}

void StorageArea::dispatchSessionStorageEvent(const String& key, const String& oldValue, const String& newValue, SecurityOrigin* securityOrigin, const KURL& pageURL, const blink::WebStorageNamespace& sessionNamespace, blink::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    Page* page = findPageWithSessionStorageNamespace(sessionNamespace);
    if (!page)
        return;

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        // FIXME: We do not yet have a way to dispatch events to out-of-process frames.
        if (!frame->isLocalFrame())
            continue;
        Storage* storage = frame->domWindow()->optionalSessionStorage();
        if (storage && toLocalFrame(frame)->document()->securityOrigin()->canAccess(securityOrigin) && !isEventSource(storage, sourceAreaInstance))
            frame->domWindow()->enqueueWindowEvent(StorageEvent::create(EventTypeNames::storage, key, oldValue, newValue, pageURL, storage));
    }
    InspectorInstrumentation::didDispatchDOMStorageEvent(page, key, oldValue, newValue, SessionStorage, securityOrigin);
}

bool StorageArea::isEventSource(Storage* storage, blink::WebStorageArea* sourceAreaInstance)
{
    ASSERT(storage);
    StorageArea* area = storage->area();
    return area->m_storageArea == sourceAreaInstance;
}

} // namespace WebCore
