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

#include "core/dom/PendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Element.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/SubresourceIntegrity.h"
#include "platform/SharedBuffer.h"
#include "wtf/CurrentTime.h"

namespace blink {

PendingScript* PendingScript::create(Element* element, ScriptResource* resource)
{
    return new PendingScript(element, resource);
}

PendingScript::PendingScript(Element* element, ScriptResource* resource)
    : m_watchingForLoad(false)
    , m_element(element)
    , m_integrityFailure(false)
    , m_parserBlockingLoadStartTime(0)
    , m_client(nullptr)
{
    setScriptResource(resource);
    ThreadState::current()->registerPreFinalizer(this);
}

PendingScript::~PendingScript()
{
}

void PendingScript::dispose()
{
    if (!m_client)
        return;
    stopWatchingForLoad();
    releaseElementAndClear();
}

void PendingScript::watchForLoad(ScriptResourceClient* client)
{
    DCHECK(!m_watchingForLoad);
    // addClient() will call streamingFinished() if the load is complete. Callers
    // who do not expect to be re-entered from this call should not call
    // watchForLoad for a PendingScript which isReady. We also need to set
    // m_watchingForLoad early, since addClient() can result in calling
    // notifyFinished and further stopWatchingForLoad().
    m_watchingForLoad = true;
    m_client = client;
    if (!m_streamer)
        resource()->addClient(client);
}

void PendingScript::stopWatchingForLoad()
{
    if (!m_watchingForLoad)
        return;
    DCHECK(resource());
    if (!m_streamer)
        resource()->removeClient(m_client);
    m_client = nullptr;
    m_watchingForLoad = false;
}

void PendingScript::streamingFinished()
{
    DCHECK(resource());
    if (m_client)
        m_client->notifyFinished(resource());
}

void PendingScript::setElement(Element* element)
{
    m_element = element;
}

Element* PendingScript::releaseElementAndClear()
{
    setScriptResource(0);
    m_watchingForLoad = false;
    m_startingPosition = TextPosition::belowRangePosition();
    m_integrityFailure = false;
    m_parserBlockingLoadStartTime = 0;
    if (m_streamer)
        m_streamer->cancel();
    m_streamer.release();
    return m_element.release();
}

void PendingScript::setScriptResource(ScriptResource* resource)
{
    setResource(resource);
}

void PendingScript::markParserBlockingLoadStartTime()
{
    DCHECK_EQ(m_parserBlockingLoadStartTime, 0.0);
    m_parserBlockingLoadStartTime = monotonicallyIncreasingTime();
}

void PendingScript::notifyFinished(Resource* resource)
{
    // The following SRI checks need to be here because, unfortunately, fetches
    // are not done purely according to the Fetch spec. In particular,
    // different requests for the same resource do not have different
    // responses; the memory cache can (and will) return the exact same
    // Resource object.
    //
    // For different requests, the same Resource object will be returned and
    // will not be associated with the particular request.  Therefore, when the
    // body of the response comes in, there's no way to validate the integrity
    // of the Resource object against a particular request (since there may be
    // several pending requests all tied to the identical object, and the
    // actual requests are not stored).
    //
    // In order to simulate the correct behavior, Blink explicitly does the SRI
    // checks here, when a PendingScript tied to a particular request is
    // finished (and in the case of a StyleSheet, at the point of execution),
    // while having proper Fetch checks in the fetch module for use in the
    // fetch JavaScript API. In a future world where the ResourceFetcher uses
    // the Fetch algorithm, this should be fixed by having separate Response
    // objects (perhaps attached to identical Resource objects) per request.
    //
    // See https://crbug.com/500701 for more information.
    if (m_element) {
        DCHECK_EQ(resource->getType(), Resource::Script);
        ScriptResource* scriptResource = toScriptResource(resource);
        String integrityAttr = m_element->fastGetAttribute(HTMLNames::integrityAttr);

        // It is possible to get back a script resource with integrity metadata
        // for a request with an empty integrity attribute. In that case, the
        // integrity check should be skipped, so this check ensures that the
        // integrity attribute isn't empty in addition to checking if the
        // resource has empty integrity metadata.
        if (!integrityAttr.isEmpty() && !scriptResource->integrityMetadata().isEmpty()) {
            ScriptIntegrityDisposition disposition = scriptResource->integrityDisposition();
            if (disposition == ScriptIntegrityDisposition::Failed) {
                // TODO(jww): This should probably also generate a console
                // message identical to the one produced by
                // CheckSubresourceIntegrity below. See https://crbug.com/585267.
                m_integrityFailure = true;
            } else if (disposition == ScriptIntegrityDisposition::NotChecked && resource->resourceBuffer()) {
                m_integrityFailure = !SubresourceIntegrity::CheckSubresourceIntegrity(scriptResource->integrityMetadata(), *m_element, resource->resourceBuffer()->data(), resource->resourceBuffer()->size(), resource->url(), *resource);
                scriptResource->setIntegrityDisposition(m_integrityFailure ? ScriptIntegrityDisposition::Failed : ScriptIntegrityDisposition::Passed);
            }
        }
    }

    if (m_streamer)
        m_streamer->notifyFinished(resource);
}

void PendingScript::notifyAppendData(ScriptResource* resource)
{
    if (m_streamer)
        m_streamer->notifyAppendData(resource);
}

DEFINE_TRACE(PendingScript)
{
    visitor->trace(m_element);
    visitor->trace(m_streamer);
    visitor->trace(m_client);
    ResourceOwner<ScriptResource>::trace(visitor);
}

ScriptSourceCode PendingScript::getSource(const KURL& documentURL, bool& errorOccurred) const
{
    if (resource()) {
        errorOccurred = resource()->errorOccurred() || m_integrityFailure;
        DCHECK(resource()->isLoaded());
        if (m_streamer && !m_streamer->streamingSuppressed())
            return ScriptSourceCode(m_streamer, resource());
        return ScriptSourceCode(resource());
    }
    errorOccurred = false;
    return ScriptSourceCode(m_element->textContent(), documentURL, startingPosition());
}

void PendingScript::setStreamer(ScriptStreamer* streamer)
{
    DCHECK(!m_streamer);
    DCHECK(!m_watchingForLoad);
    m_streamer = streamer;
}

bool PendingScript::isReady() const
{
    if (resource() && !resource()->isLoaded())
        return false;
    if (m_streamer && !m_streamer->isFinished())
        return false;
    return true;
}

bool PendingScript::errorOccurred() const
{
    if (resource())
        return resource()->errorOccurred();
    if (m_streamer && m_streamer->resource())
        return m_streamer->resource()->errorOccurred();
    return false;
}

} // namespace blink
