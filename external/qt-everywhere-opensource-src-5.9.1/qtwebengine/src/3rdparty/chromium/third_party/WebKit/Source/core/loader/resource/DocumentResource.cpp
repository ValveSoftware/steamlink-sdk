/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "core/loader/resource/DocumentResource.h"

#include "core/dom/XMLDocument.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/SharedBuffer.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

DocumentResource* DocumentResource::fetchSVGDocument(FetchRequest& request,
                                                     ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  request.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextImage);
  return toDocumentResource(
      fetcher->requestResource(request, SVGDocumentResourceFactory()));
}

DocumentResource::DocumentResource(const ResourceRequest& request,
                                   Type type,
                                   const ResourceLoaderOptions& options)
    : TextResource(request, type, options, "application/xml", String()) {
  // FIXME: We'll support more types to support HTMLImports.
  DCHECK_EQ(type, SVGDocument);
}

DocumentResource::~DocumentResource() {}

DEFINE_TRACE(DocumentResource) {
  visitor->trace(m_document);
  Resource::trace(visitor);
}

void DocumentResource::checkNotify() {
  if (data() && mimeTypeAllowed()) {
    // We don't need to create a new frame because the new document belongs to
    // the parent UseElement.
    m_document = createDocument(response().url());
    m_document->setContent(decodedText());
  }
  Resource::checkNotify();
}

bool DocumentResource::mimeTypeAllowed() const {
  DCHECK_EQ(getType(), SVGDocument);
  AtomicString mimeType = response().mimeType();
  if (response().isHTTP())
    mimeType = httpContentType();
  return mimeType == "image/svg+xml" || mimeType == "text/xml" ||
         mimeType == "application/xml" || mimeType == "application/xhtml+xml";
}

Document* DocumentResource::createDocument(const KURL& url) {
  switch (getType()) {
    case SVGDocument:
      return XMLDocument::createSVG(DocumentInit(url));
    default:
      // FIXME: We'll add more types to support HTMLImports.
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
