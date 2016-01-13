/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorDOMStorageAgent.h"

#include "bindings/v8/ExceptionState.h"
#include "core/InspectorFrontend.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/storage/Storage.h"
#include "core/storage/StorageNamespace.h"
#include "platform/JSONValues.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

namespace DOMStorageAgentState {
static const char domStorageAgentEnabled[] = "domStorageAgentEnabled";
};

static bool hadException(ExceptionState& exceptionState, ErrorString* errorString)
{
    if (!exceptionState.hadException())
        return false;

    switch (exceptionState.code()) {
    case SecurityError:
        *errorString = "Security error";
        return true;
    default:
        *errorString = "Unknown DOM storage error";
        return true;
    }
}

InspectorDOMStorageAgent::InspectorDOMStorageAgent(InspectorPageAgent* pageAgent)
    : InspectorBaseAgent<InspectorDOMStorageAgent>("DOMStorage")
    , m_pageAgent(pageAgent)
    , m_frontend(0)
{
}

InspectorDOMStorageAgent::~InspectorDOMStorageAgent()
{
}

void InspectorDOMStorageAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend;
}

void InspectorDOMStorageAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorDOMStorageAgent::restore()
{
    if (isEnabled())
        enable(0);
}

bool InspectorDOMStorageAgent::isEnabled() const
{
    return m_state->getBoolean(DOMStorageAgentState::domStorageAgentEnabled);
}

void InspectorDOMStorageAgent::enable(ErrorString*)
{
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, true);
    m_instrumentingAgents->setInspectorDOMStorageAgent(this);
}

void InspectorDOMStorageAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorDOMStorageAgent(0);
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, false);
}

void InspectorDOMStorageAgent::getDOMStorageItems(ErrorString* errorString, const RefPtr<JSONObject>& storageId, RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > >& items)
{
    LocalFrame* frame;
    OwnPtrWillBeRawPtr<StorageArea> storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return;

    RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > > storageItems = TypeBuilder::Array<TypeBuilder::Array<String> >::create();

    TrackExceptionState exceptionState;
    for (unsigned i = 0; i < storageArea->length(exceptionState, frame); ++i) {
        String name(storageArea->key(i, exceptionState, frame));
        if (hadException(exceptionState, errorString))
            return;
        String value(storageArea->getItem(name, exceptionState, frame));
        if (hadException(exceptionState, errorString))
            return;
        RefPtr<TypeBuilder::Array<String> > entry = TypeBuilder::Array<String>::create();
        entry->addItem(name);
        entry->addItem(value);
        storageItems->addItem(entry);
    }
    items = storageItems.release();
}

static String toErrorString(ExceptionState& exceptionState)
{
    if (exceptionState.hadException())
        return DOMException::getErrorName(exceptionState.code());
    return "";
}

void InspectorDOMStorageAgent::setDOMStorageItem(ErrorString* errorString, const RefPtr<JSONObject>& storageId, const String& key, const String& value)
{
    LocalFrame* frame;
    OwnPtrWillBeRawPtr<StorageArea> storageArea = findStorageArea(0, storageId, frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    TrackExceptionState exceptionState;
    storageArea->setItem(key, value, exceptionState, frame);
    *errorString = toErrorString(exceptionState);
}

void InspectorDOMStorageAgent::removeDOMStorageItem(ErrorString* errorString, const RefPtr<JSONObject>& storageId, const String& key)
{
    LocalFrame* frame;
    OwnPtrWillBeRawPtr<StorageArea> storageArea = findStorageArea(0, storageId, frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    TrackExceptionState exceptionState;
    storageArea->removeItem(key, exceptionState, frame);
    *errorString = toErrorString(exceptionState);
}

PassRefPtr<TypeBuilder::DOMStorage::StorageId> InspectorDOMStorageAgent::storageId(SecurityOrigin* securityOrigin, bool isLocalStorage)
{
    return TypeBuilder::DOMStorage::StorageId::create()
        .setSecurityOrigin(securityOrigin->toRawString())
        .setIsLocalStorage(isLocalStorage).release();
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin)
{
    if (!m_frontend || !isEnabled())
        return;

    RefPtr<TypeBuilder::DOMStorage::StorageId> id = storageId(securityOrigin, storageType == LocalStorage);

    if (key.isNull())
        m_frontend->domstorage()->domStorageItemsCleared(id);
    else if (newValue.isNull())
        m_frontend->domstorage()->domStorageItemRemoved(id, key);
    else if (oldValue.isNull())
        m_frontend->domstorage()->domStorageItemAdded(id, key, newValue);
    else
        m_frontend->domstorage()->domStorageItemUpdated(id, key, oldValue, newValue);
}

PassOwnPtrWillBeRawPtr<StorageArea> InspectorDOMStorageAgent::findStorageArea(ErrorString* errorString, const RefPtr<JSONObject>& storageId, LocalFrame*& targetFrame)
{
    String securityOrigin;
    bool isLocalStorage = false;
    bool success = storageId->getString("securityOrigin", &securityOrigin);
    if (success)
        success = storageId->getBoolean("isLocalStorage", &isLocalStorage);
    if (!success) {
        if (errorString)
            *errorString = "Invalid storageId format";
        return nullptr;
    }

    LocalFrame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    if (!frame) {
        if (errorString)
            *errorString = "LocalFrame not found for the given security origin";
        return nullptr;
    }
    targetFrame = frame;

    if (isLocalStorage)
        return StorageNamespace::localStorageArea(frame->document()->securityOrigin());
    return m_pageAgent->page()->sessionStorage()->storageArea(frame->document()->securityOrigin());
}

} // namespace WebCore

