/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "platform/Prerender.h"

#include "platform/PrerenderClient.h"
#include "public/platform/WebPrerender.h"
#include "public/platform/WebPrerenderingSupport.h"

namespace WebCore {

PassRefPtr<Prerender> Prerender::create(PrerenderClient* client, const KURL& url, unsigned relTypes, const String& referrer, ReferrerPolicy policy)
{
    return adoptRef(new Prerender(client, url, relTypes, referrer, policy));
}

Prerender::Prerender(PrerenderClient* client, const KURL& url, const unsigned relTypes, const String& referrer, ReferrerPolicy policy)
    : m_client(client)
    , m_url(url)
    , m_relTypes(relTypes)
    , m_referrer(referrer)
    , m_referrerPolicy(policy)
{
}

Prerender::~Prerender()
{
}

void Prerender::removeClient()
{
    m_client = 0;
}

void Prerender::add()
{
    blink::WebPrerenderingSupport* platform = blink::WebPrerenderingSupport::current();
    if (!platform)
        return;
    platform->add(blink::WebPrerender(this));
}

void Prerender::cancel()
{
    blink::WebPrerenderingSupport* platform = blink::WebPrerenderingSupport::current();
    if (!platform)
        return;
    platform->cancel(blink::WebPrerender(this));
}

void Prerender::abandon()
{
    blink::WebPrerenderingSupport* platform = blink::WebPrerenderingSupport::current();
    if (!platform)
        return;
    platform->abandon(blink::WebPrerender(this));
}

void Prerender::didStartPrerender()
{
    if (m_client)
        m_client->didStartPrerender();
}

void Prerender::didStopPrerender()
{
    if (m_client)
        m_client->didStopPrerender();
}

void Prerender::didSendLoadForPrerender()
{
    if (m_client)
        m_client->didSendLoadForPrerender();
}

void Prerender::didSendDOMContentLoadedForPrerender()
{
    if (m_client)
        m_client->didSendDOMContentLoadedForPrerender();
}

}
