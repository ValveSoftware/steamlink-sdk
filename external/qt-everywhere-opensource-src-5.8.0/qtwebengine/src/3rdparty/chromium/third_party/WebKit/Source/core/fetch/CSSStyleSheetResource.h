/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CSSStyleSheetResource_h
#define CSSStyleSheetResource_h

#include "core/CoreExport.h"
#include "core/fetch/StyleSheetResource.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSParserContext;
class FetchRequest;
class ResourceClient;
class ResourceFetcher;
class StyleSheetContents;

class CORE_EXPORT CSSStyleSheetResource final : public StyleSheetResource {
public:
    enum class MIMETypeCheck { Strict, Lax };

    static CSSStyleSheetResource* fetch(FetchRequest&, ResourceFetcher*);
    static CSSStyleSheetResource* createForTest(const ResourceRequest&, const String& charset);

    ~CSSStyleSheetResource() override;
    DECLARE_VIRTUAL_TRACE();

    const String sheetText(MIMETypeCheck = MIMETypeCheck::Strict) const;

    void didAddClient(ResourceClient*) override;

    StyleSheetContents* restoreParsedStyleSheet(const CSSParserContext&);
    void saveParsedStyleSheet(StyleSheetContents*);

protected:
    bool isSafeToUnlock() const override;
    void destroyDecodedDataIfPossible() override;
    void destroyDecodedDataForFailedRevalidation() override { destroyDecodedDataIfPossible(); }

private:
    class CSSStyleSheetResourceFactory : public ResourceFactory {
    public:
        CSSStyleSheetResourceFactory()
            : ResourceFactory(Resource::CSSStyleSheet) { }

        Resource* create(const ResourceRequest& request, const ResourceLoaderOptions& options, const String& charset) const override
        {
            return new CSSStyleSheetResource(request, options, charset);
        }
    };
    CSSStyleSheetResource(const ResourceRequest&, const ResourceLoaderOptions&, const String& charset);

    bool canUseSheet(MIMETypeCheck) const;
    void checkNotify() override;

    void setParsedStyleSheetCache(StyleSheetContents*);

    String m_decodedSheetText;

    Member<StyleSheetContents> m_parsedStyleSheetCache;
};

DEFINE_RESOURCE_TYPE_CASTS(CSSStyleSheet);

} // namespace blink

#endif
