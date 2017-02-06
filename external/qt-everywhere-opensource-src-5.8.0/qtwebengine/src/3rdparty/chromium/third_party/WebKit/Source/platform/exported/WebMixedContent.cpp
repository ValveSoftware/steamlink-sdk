/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
 */

#include "public/platform/WebMixedContent.h"

namespace blink {

// static
WebMixedContent::ContextType WebMixedContent::contextTypeFromRequestContext(WebURLRequest::RequestContext context, bool strictMixedContentCheckingForPlugin)
{
    switch (context) {
    // "Optionally-blockable" mixed content
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextFavicon:
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextVideo:
        return ContextType::OptionallyBlockable;

    // Plugins! Oh how dearly we love plugin-loaded content!
    case WebURLRequest::RequestContextPlugin: {
        return strictMixedContentCheckingForPlugin ? ContextType::Blockable : ContextType::OptionallyBlockable;
    }

    // "Blockable" mixed content
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextFont:
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextIframe:
    case WebURLRequest::RequestContextImageSet:
    case WebURLRequest::RequestContextImport:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextObject:
    case WebURLRequest::RequestContextPing:
    case WebURLRequest::RequestContextScript:
    case WebURLRequest::RequestContextServiceWorker:
    case WebURLRequest::RequestContextSharedWorker:
    case WebURLRequest::RequestContextStyle:
    case WebURLRequest::RequestContextSubresource:
    case WebURLRequest::RequestContextTrack:
    case WebURLRequest::RequestContextWorker:
    case WebURLRequest::RequestContextXMLHttpRequest:
    case WebURLRequest::RequestContextXSLT:
        return ContextType::Blockable;

    // FIXME: Contexts that we should block, but don't currently. https://crbug.com/388650
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextPrefetch:
        return ContextType::ShouldBeBlockable;

    case WebURLRequest::RequestContextUnspecified:
        NOTREACHED();
    }
    NOTREACHED();
    return ContextType::Blockable;
}

// static
const char* WebMixedContent::requestContextName(WebURLRequest::RequestContext context)
{
    switch (context) {
    case WebURLRequest::RequestContextAudio:
        return "audio file";
    case WebURLRequest::RequestContextBeacon:
        return "Beacon endpoint";
    case WebURLRequest::RequestContextCSPReport:
        return "Content Security Policy reporting endpoint";
    case WebURLRequest::RequestContextDownload:
        return "download";
    case WebURLRequest::RequestContextEmbed:
        return "plugin resource";
    case WebURLRequest::RequestContextEventSource:
        return "EventSource endpoint";
    case WebURLRequest::RequestContextFavicon:
        return "favicon";
    case WebURLRequest::RequestContextFetch:
        return "resource";
    case WebURLRequest::RequestContextFont:
        return "font";
    case WebURLRequest::RequestContextForm:
        return "form action";
    case WebURLRequest::RequestContextFrame:
        return "frame";
    case WebURLRequest::RequestContextHyperlink:
        return "resource";
    case WebURLRequest::RequestContextIframe:
        return "frame";
    case WebURLRequest::RequestContextImage:
        return "image";
    case WebURLRequest::RequestContextImageSet:
        return "image";
    case WebURLRequest::RequestContextImport:
        return "HTML Import";
    case WebURLRequest::RequestContextInternal:
        return "resource";
    case WebURLRequest::RequestContextLocation:
        return "resource";
    case WebURLRequest::RequestContextManifest:
        return "manifest";
    case WebURLRequest::RequestContextObject:
        return "plugin resource";
    case WebURLRequest::RequestContextPing:
        return "hyperlink auditing endpoint";
    case WebURLRequest::RequestContextPlugin:
        return "plugin data";
    case WebURLRequest::RequestContextPrefetch:
        return "prefetch resource";
    case WebURLRequest::RequestContextScript:
        return "script";
    case WebURLRequest::RequestContextServiceWorker:
        return "Service Worker script";
    case WebURLRequest::RequestContextSharedWorker:
        return "Shared Worker script";
    case WebURLRequest::RequestContextStyle:
        return "stylesheet";
    case WebURLRequest::RequestContextSubresource:
        return "resource";
    case WebURLRequest::RequestContextTrack:
        return "Text Track";
    case WebURLRequest::RequestContextUnspecified:
        return "resource";
    case WebURLRequest::RequestContextVideo:
        return "video";
    case WebURLRequest::RequestContextWorker:
        return "Worker script";
    case WebURLRequest::RequestContextXMLHttpRequest:
        return "XMLHttpRequest endpoint";
    case WebURLRequest::RequestContextXSLT:
        return "XSLT";
    }
    NOTREACHED();
    return "resource";
}

} // namespace blink
