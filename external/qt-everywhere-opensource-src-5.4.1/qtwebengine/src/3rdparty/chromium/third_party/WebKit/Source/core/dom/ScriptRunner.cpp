/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#include "config.h"
#include "core/dom/ScriptRunner.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/PendingScript.h"
#include "core/dom/ScriptLoader.h"
#include "core/fetch/ScriptResource.h"
#include "platform/heap/Handle.h"

namespace WebCore {

ScriptRunner::ScriptRunner(Document* document)
    : m_document(document)
    , m_timer(this, &ScriptRunner::timerFired)
{
    ASSERT(document);
}

ScriptRunner::~ScriptRunner()
{
}

void ScriptRunner::queueScriptForExecution(ScriptLoader* scriptLoader, ResourcePtr<ScriptResource> resource, ExecutionType executionType)
{
    ASSERT(scriptLoader);
    ASSERT(resource.get());

    Element* element = scriptLoader->element();
    ASSERT(element);
    ASSERT(element->inDocument());

    m_document->incrementLoadEventDelayCount();

    switch (executionType) {
    case ASYNC_EXECUTION:
        m_pendingAsyncScripts.add(scriptLoader, PendingScript(element, resource.get()));
        break;

    case IN_ORDER_EXECUTION:
        m_scriptsToExecuteInOrder.append(PendingScript(element, resource.get()));
        break;
    }
}

void ScriptRunner::suspend()
{
    m_timer.stop();
}

void ScriptRunner::resume()
{
    if (hasPendingScripts())
        m_timer.startOneShot(0, FROM_HERE);
}

void ScriptRunner::notifyScriptReady(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    switch (executionType) {
    case ASYNC_EXECUTION:
        ASSERT(m_pendingAsyncScripts.contains(scriptLoader));
        m_scriptsToExecuteSoon.append(m_pendingAsyncScripts.take(scriptLoader));
        break;

    case IN_ORDER_EXECUTION:
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());
        break;
    }
    m_timer.startOneShot(0, FROM_HERE);
}

void ScriptRunner::notifyScriptLoadError(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    switch (executionType) {
    case ASYNC_EXECUTION:
        ASSERT(m_pendingAsyncScripts.contains(scriptLoader));
        m_pendingAsyncScripts.remove(scriptLoader);
        m_document->decrementLoadEventDelayCount();
        break;

    case IN_ORDER_EXECUTION:
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());
        break;
    }
}

void ScriptRunner::timerFired(Timer<ScriptRunner>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);

    RefPtrWillBeRawPtr<Document> protect(m_document.get());

    Vector<PendingScript> scripts;
    scripts.swap(m_scriptsToExecuteSoon);

    size_t numInOrderScriptsToExecute = 0;
    for (; numInOrderScriptsToExecute < m_scriptsToExecuteInOrder.size() && m_scriptsToExecuteInOrder[numInOrderScriptsToExecute].resource()->isLoaded(); ++numInOrderScriptsToExecute)
        scripts.append(m_scriptsToExecuteInOrder[numInOrderScriptsToExecute]);
    if (numInOrderScriptsToExecute)
        m_scriptsToExecuteInOrder.remove(0, numInOrderScriptsToExecute);

    size_t size = scripts.size();
    for (size_t i = 0; i < size; ++i) {
        ScriptResource* resource = scripts[i].resource();
        RefPtrWillBeRawPtr<Element> element = scripts[i].releaseElementAndClear();
        toScriptLoaderIfPossible(element.get())->execute(resource);
        m_document->decrementLoadEventDelayCount();
    }
}

void ScriptRunner::trace(Visitor* visitor)
{
    visitor->trace(m_document);
    visitor->trace(m_scriptsToExecuteInOrder);
    visitor->trace(m_scriptsToExecuteSoon);
    visitor->trace(m_pendingAsyncScripts);
}

}
