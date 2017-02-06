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

#include "modules/storage/InspectorDOMStorageAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/page/Page.h"
#include "modules/storage/Storage.h"
#include "modules/storage/StorageNamespace.h"
#include "modules/storage/StorageNamespaceController.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

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

InspectorDOMStorageAgent::InspectorDOMStorageAgent(Page* page)
    : m_page(page)
    , m_isEnabled(false)
{
}

InspectorDOMStorageAgent::~InspectorDOMStorageAgent()
{
}

DEFINE_TRACE(InspectorDOMStorageAgent)
{
    visitor->trace(m_page);
    InspectorBaseAgent::trace(visitor);
}

void InspectorDOMStorageAgent::restore()
{
    if (m_state->booleanProperty(DOMStorageAgentState::domStorageAgentEnabled, false))
        enable(0);
}

void InspectorDOMStorageAgent::enable(ErrorString*)
{
    if (m_isEnabled)
        return;
    m_isEnabled = true;
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, true);
    if (StorageNamespaceController* controller = StorageNamespaceController::from(m_page))
        controller->setInspectorAgent(this);
}

void InspectorDOMStorageAgent::disable(ErrorString*)
{
    if (!m_isEnabled)
        return;
    m_isEnabled = false;
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, false);
    if (StorageNamespaceController* controller = StorageNamespaceController::from(m_page))
        controller->setInspectorAgent(nullptr);
}

void InspectorDOMStorageAgent::getDOMStorageItems(ErrorString* errorString, std::unique_ptr<protocol::DOMStorage::StorageId> storageId, std::unique_ptr<protocol::Array<protocol::Array<String>>>* items)
{
    LocalFrame* frame;
    StorageArea* storageArea = findStorageArea(errorString, std::move(storageId), frame);
    if (!storageArea)
        return;

    std::unique_ptr<protocol::Array<protocol::Array<String>>> storageItems = protocol::Array<protocol::Array<String>>::create();

    TrackExceptionState exceptionState;
    for (unsigned i = 0; i < storageArea->length(exceptionState, frame); ++i) {
        String name(storageArea->key(i, exceptionState, frame));
        if (hadException(exceptionState, errorString))
            return;
        String value(storageArea->getItem(name, exceptionState, frame));
        if (hadException(exceptionState, errorString))
            return;
        std::unique_ptr<protocol::Array<String>> entry = protocol::Array<String>::create();
        entry->addItem(name);
        entry->addItem(value);
        storageItems->addItem(std::move(entry));
    }
    *items = std::move(storageItems);
}

static String toErrorString(ExceptionState& exceptionState)
{
    if (exceptionState.hadException())
        return DOMException::getErrorName(exceptionState.code());
    return "";
}

void InspectorDOMStorageAgent::setDOMStorageItem(ErrorString* errorString, std::unique_ptr<protocol::DOMStorage::StorageId> storageId, const String& key, const String& value)
{
    LocalFrame* frame;
    StorageArea* storageArea = findStorageArea(0, std::move(storageId), frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    TrackExceptionState exceptionState;
    storageArea->setItem(key, value, exceptionState, frame);
    *errorString = toErrorString(exceptionState);
}

void InspectorDOMStorageAgent::removeDOMStorageItem(ErrorString* errorString, std::unique_ptr<protocol::DOMStorage::StorageId> storageId, const String& key)
{
    LocalFrame* frame;
    StorageArea* storageArea = findStorageArea(0, std::move(storageId), frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    TrackExceptionState exceptionState;
    storageArea->removeItem(key, exceptionState, frame);
    *errorString = toErrorString(exceptionState);
}

std::unique_ptr<protocol::DOMStorage::StorageId> InspectorDOMStorageAgent::storageId(SecurityOrigin* securityOrigin, bool isLocalStorage)
{
    return protocol::DOMStorage::StorageId::create()
        .setSecurityOrigin(securityOrigin->toRawString())
        .setIsLocalStorage(isLocalStorage).build();
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin)
{
    if (!frontend())
        return;

    std::unique_ptr<protocol::DOMStorage::StorageId> id = storageId(securityOrigin, storageType == LocalStorage);

    if (key.isNull())
        frontend()->domStorageItemsCleared(std::move(id));
    else if (newValue.isNull())
        frontend()->domStorageItemRemoved(std::move(id), key);
    else if (oldValue.isNull())
        frontend()->domStorageItemAdded(std::move(id), key, newValue);
    else
        frontend()->domStorageItemUpdated(std::move(id), key, oldValue, newValue);
}

StorageArea* InspectorDOMStorageAgent::findStorageArea(ErrorString* errorString, std::unique_ptr<protocol::DOMStorage::StorageId> storageId, LocalFrame*& targetFrame)
{
    String securityOrigin = storageId->getSecurityOrigin();
    bool isLocalStorage = storageId->getIsLocalStorage();

    if (!m_page->mainFrame()->isLocalFrame())
        return nullptr;

    InspectedFrames* inspectedFrames = InspectedFrames::create(m_page->deprecatedLocalMainFrame());
    LocalFrame* frame = inspectedFrames->frameWithSecurityOrigin(securityOrigin);
    if (!frame) {
        if (errorString)
            *errorString = "LocalFrame not found for the given security origin";
        return nullptr;
    }
    targetFrame = frame;

    if (isLocalStorage)
        return StorageNamespace::localStorageArea(frame->document()->getSecurityOrigin());
    StorageNamespace* sessionStorage = StorageNamespaceController::from(m_page)->sessionStorage();
    if (!sessionStorage) {
        if (errorString)
            *errorString = "SessionStorage is not supported";
        return nullptr;
    }
    return sessionStorage->storageArea(frame->document()->getSecurityOrigin());
}

} // namespace blink
