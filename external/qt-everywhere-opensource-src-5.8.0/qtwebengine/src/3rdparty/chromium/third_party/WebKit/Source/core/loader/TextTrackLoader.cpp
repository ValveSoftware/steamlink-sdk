/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "core/loader/TextTrackLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Logging.h"
#include "platform/SharedBuffer.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

TextTrackLoader::TextTrackLoader(TextTrackLoaderClient& client, Document& document)
    : m_client(client)
    , m_document(document)
    , m_cueLoadTimer(this, &TextTrackLoader::cueLoadTimerFired)
    , m_state(Idle)
    , m_newCuesAvailable(false)
{
}

TextTrackLoader::~TextTrackLoader()
{
}

void TextTrackLoader::cueLoadTimerFired(Timer<TextTrackLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_cueLoadTimer);

    if (m_newCuesAvailable) {
        m_newCuesAvailable = false;
        m_client->newCuesAvailable(this);
    }

    if (m_state >= Finished)
        m_client->cueLoadingCompleted(this, m_state == Failed);
}

void TextTrackLoader::cancelLoad()
{
    clearResource();
}

void TextTrackLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse&)
{
    DCHECK_EQ(this->resource(), resource);
    if (resource->options().corsEnabled == IsCORSEnabled || document().getSecurityOrigin()->canRequestNoSuborigin(request.url()))
        return;

    corsPolicyPreventedLoad(document().getSecurityOrigin(), request.url());
    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0, BLINK_FROM_HERE);
    clearResource();
}

void TextTrackLoader::dataReceived(Resource* resource, const char* data, size_t length)
{
    ASSERT(this->resource() == resource);

    if (m_state == Failed)
        return;

    if (!m_cueParser)
        m_cueParser = VTTParser::create(this, document());

    m_cueParser->parseBytes(data, length);
}

void TextTrackLoader::corsPolicyPreventedLoad(SecurityOrigin* securityOrigin, const KURL& url)
{
    String consoleMessage("Text track from origin '" + SecurityOrigin::create(url)->toString() + "' has been blocked from loading: Not at same origin as the document, and parent of track element does not have a 'crossorigin' attribute. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.");
    document().addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, consoleMessage));
    m_state = Failed;
}

void TextTrackLoader::notifyFinished(Resource* resource)
{
    ASSERT(this->resource() == resource);
    if (m_state != Failed)
        m_state = resource->errorOccurred() ? Failed : Finished;

    if (m_state == Finished && m_cueParser)
        m_cueParser->flush();

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0, BLINK_FROM_HERE);

    cancelLoad();
}

bool TextTrackLoader::load(const KURL& url, CrossOriginAttributeValue crossOrigin)
{
    cancelLoad();

    FetchRequest cueRequest(ResourceRequest(document().completeURL(url)), FetchInitiatorTypeNames::texttrack);

    if (crossOrigin != CrossOriginAttributeNotSet) {
        cueRequest.setCrossOriginAccessControl(document().getSecurityOrigin(), crossOrigin);
    } else if (!document().getSecurityOrigin()->canRequestNoSuborigin(url)) {
        // Text track elements without 'crossorigin' set on the parent are "No CORS"; report error if not same-origin.
        corsPolicyPreventedLoad(document().getSecurityOrigin(), url);
        return false;
    }

    ResourceFetcher* fetcher = document().fetcher();
    setResource(RawResource::fetchTextTrack(cueRequest, fetcher));
    return resource();
}

void TextTrackLoader::newCuesParsed()
{
    if (m_cueLoadTimer.isActive())
        return;

    m_newCuesAvailable = true;
    m_cueLoadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void TextTrackLoader::newRegionsParsed()
{
    m_client->newRegionsAvailable(this);
}

void TextTrackLoader::fileFailedToParse()
{
    m_state = Failed;

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0, BLINK_FROM_HERE);

    cancelLoad();
}

void TextTrackLoader::getNewCues(HeapVector<Member<TextTrackCue>>& outputCues)
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        m_cueParser->getNewCues(outputCues);
}

void TextTrackLoader::getNewRegions(HeapVector<Member<VTTRegion>>& outputRegions)
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        m_cueParser->getNewRegions(outputRegions);
}

DEFINE_TRACE(TextTrackLoader)
{
    visitor->trace(m_client);
    visitor->trace(m_cueParser);
    visitor->trace(m_document);
    ResourceOwner<RawResource>::trace(visitor);
    VTTParserClient::trace(visitor);
}

} // namespace blink
