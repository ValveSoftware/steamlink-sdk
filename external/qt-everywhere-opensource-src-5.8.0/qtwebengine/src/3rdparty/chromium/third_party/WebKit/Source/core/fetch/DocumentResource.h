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

#ifndef DocumentResource_h
#define DocumentResource_h

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceClient.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class Document;
class FetchRequest;
class ResourceFetcher;

class CORE_EXPORT DocumentResource final : public Resource {
public:
    using ClientType = ResourceClient;

    static DocumentResource* fetchSVGDocument(FetchRequest&, ResourceFetcher*);
    ~DocumentResource() override;
    DECLARE_VIRTUAL_TRACE();

    Document* document() const { return m_document.get(); }

    void setEncoding(const String&) override;
    String encoding() const override;
    void checkNotify() override;

private:
    class SVGDocumentResourceFactory : public ResourceFactory {
    public:
        SVGDocumentResourceFactory()
            : ResourceFactory(Resource::SVGDocument) { }

        Resource* create(const ResourceRequest& request, const ResourceLoaderOptions& options, const String& charset) const override
        {
            return new DocumentResource(request, Resource::SVGDocument, options);
        }
    };
    DocumentResource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

    bool mimeTypeAllowed() const;
    Document* createDocument(const KURL&);

    Member<Document> m_document;
    std::unique_ptr<TextResourceDecoder> m_decoder;
};

DEFINE_TYPE_CASTS(DocumentResource, Resource, resource, resource->getType() == Resource::SVGDocument, resource.getType() == Resource::SVGDocument);

class CORE_EXPORT DocumentResourceClient : public GarbageCollectedMixin, public ResourceClient {
public:
    ~DocumentResourceClient() override {}
    static bool isExpectedType(ResourceClient* client) { return client->getResourceClientType() == DocumentType; }
    ResourceClientType getResourceClientType() const final { return DocumentType; }
    DEFINE_INLINE_VIRTUAL_TRACE() {}
};

} // namespace blink

#endif // DocumentResource_h
