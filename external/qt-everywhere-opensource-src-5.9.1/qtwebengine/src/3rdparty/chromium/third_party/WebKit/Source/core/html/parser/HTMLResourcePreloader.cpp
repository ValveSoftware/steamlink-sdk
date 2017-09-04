/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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

#include "core/html/parser/HTMLResourcePreloader.h"

#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/loader/DocumentLoader.h"
#include "platform/Histogram.h"
#include "public/platform/Platform.h"
#include <memory>

namespace blink {

HTMLResourcePreloader::HTMLResourcePreloader(Document& document)
    : m_document(document) {}

HTMLResourcePreloader* HTMLResourcePreloader::create(Document& document) {
  return new HTMLResourcePreloader(document);
}

DEFINE_TRACE(HTMLResourcePreloader) {
  visitor->trace(m_document);
  visitor->trace(m_cssPreloaders);
}

int HTMLResourcePreloader::countPreloads() {
  if (m_document->loader())
    return m_document->loader()->fetcher()->countPreloads();
  return 0;
}

static void preconnectHost(PreloadRequest* request,
                           const NetworkHintsInterface& networkHintsInterface) {
  ASSERT(request);
  ASSERT(request->isPreconnect());
  KURL host(request->baseURL(), request->resourceURL());
  if (!host.isValid() || !host.protocolIsInHTTPFamily())
    return;
  networkHintsInterface.preconnectHost(host, request->crossOrigin());
}

void HTMLResourcePreloader::preload(
    std::unique_ptr<PreloadRequest> preload,
    const NetworkHintsInterface& networkHintsInterface) {
  if (preload->isPreconnect()) {
    preconnectHost(preload.get(), networkHintsInterface);
    return;
  }
  // TODO(yoichio): Should preload if document is imported.
  if (!m_document->loader())
    return;
  FetchRequest request = preload->resourceRequest(m_document);

  // Data URLs are filtered out in the preload scanner.
  DCHECK(!request.url().protocolIsData());

  if (preload->resourceType() == Resource::Script ||
      preload->resourceType() == Resource::CSSStyleSheet ||
      preload->resourceType() == Resource::ImportResource)
    request.setCharset(preload->charset().isEmpty()
                           ? m_document->characterSet().getString()
                           : preload->charset());
  request.setForPreload(true, preload->discoveryTime());
  int duration = static_cast<int>(
      1000 * (monotonicallyIncreasingTime() - preload->discoveryTime()));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, preloadDelayHistogram,
                      ("WebCore.PreloadDelayMs", 0, 2000, 20));
  preloadDelayHistogram.count(duration);

  Resource* resource =
      m_document->loader()->startPreload(preload->resourceType(), request);
  if (resource && !resource->isLoaded() &&
      preload->resourceType() == Resource::CSSStyleSheet) {
    Settings* settings = m_document->settings();
    if (settings && (settings->cssExternalScannerNoPreload() ||
                     settings->cssExternalScannerPreload()))
      m_cssPreloaders.add(new CSSPreloaderResourceClient(resource, this));
  }
}

}  // namespace blink
