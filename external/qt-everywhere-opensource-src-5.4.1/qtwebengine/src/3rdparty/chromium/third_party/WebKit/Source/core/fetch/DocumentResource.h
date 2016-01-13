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
#include "core/fetch/ResourcePtr.h"
#include "core/html/parser/TextResourceDecoder.h"

namespace WebCore {

class Document;

class DocumentResource FINAL : public Resource {
public:
    typedef ResourceClient ClientType;

    DocumentResource(const ResourceRequest&, Type);
    virtual ~DocumentResource();

    Document* document() const { return m_document.get(); }

    virtual void setEncoding(const String&) OVERRIDE;
    virtual String encoding() const OVERRIDE;
    virtual void checkNotify() OVERRIDE;

private:
    PassRefPtrWillBeRawPtr<Document> createDocument(const KURL&);

    RefPtrWillBePersistent<Document> m_document;
    OwnPtr<TextResourceDecoder> m_decoder;
};

DEFINE_TYPE_CASTS(DocumentResource, Resource, resource, resource->type() == Resource::SVGDocument, resource.type() == Resource::SVGDocument); \
inline DocumentResource* toDocumentResource(const ResourcePtr<Resource>& ptr) { return toDocumentResource(ptr.get()); }

class DocumentResourceClient : public ResourceClient {
public:
    virtual ~DocumentResourceClient() { }
    static ResourceClientType expectedType() { return DocumentType; }
    virtual ResourceClientType resourceClientType() const OVERRIDE { return expectedType(); }
};

}

#endif // DocumentResource_h
