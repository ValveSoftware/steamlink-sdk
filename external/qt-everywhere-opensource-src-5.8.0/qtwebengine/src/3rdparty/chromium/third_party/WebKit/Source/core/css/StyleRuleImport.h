/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef StyleRuleImport_h
#define StyleRuleImport_h

#include "core/css/StyleRule.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSStyleSheetResource;
class MediaQuerySet;
class StyleSheetContents;

class StyleRuleImport : public StyleRuleBase {
    USING_PRE_FINALIZER(StyleRuleImport, dispose);
public:
    static StyleRuleImport* create(const String& href, MediaQuerySet*);

    ~StyleRuleImport();

    StyleSheetContents* parentStyleSheet() const { return m_parentStyleSheet; }
    void setParentStyleSheet(StyleSheetContents* sheet) { ASSERT(sheet); m_parentStyleSheet = sheet; }
    void clearParentStyleSheet() { m_parentStyleSheet = nullptr; }

    String href() const { return m_strHref; }
    StyleSheetContents* styleSheet() const { return m_styleSheet.get(); }

    bool isLoading() const;
    MediaQuerySet* mediaQueries() { return m_mediaQueries.get(); }

    void requestStyleSheet();

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    // FIXME: inherit from StyleSheetResourceClient directly to eliminate back pointer, as there are no space savings in this.
    // NOTE: We put the StyleSheetResourceClient in a member instead of inheriting from it
    // to avoid adding a vptr to StyleRuleImport.
    class ImportedStyleSheetClient final : public GarbageCollectedFinalized<ImportedStyleSheetClient>, public StyleSheetResourceClient {
        USING_GARBAGE_COLLECTED_MIXIN(ImportedStyleSheetClient);
    public:
        ImportedStyleSheetClient(StyleRuleImport* ownerRule) : m_ownerRule(ownerRule) { }
        ~ImportedStyleSheetClient() override { }
        void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource* sheet) override
        {
            m_ownerRule->setCSSStyleSheet(href, baseURL, charset, sheet);
        }
        String debugName() const override { return "ImportedStyleSheetClient"; }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_ownerRule);
            StyleSheetResourceClient::trace(visitor);
        }

    private:
        Member<StyleRuleImport> m_ownerRule;
    };

    void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*);

    StyleRuleImport(const String& href, MediaQuerySet*);

    void dispose();

    Member<StyleSheetContents> m_parentStyleSheet;

    Member<ImportedStyleSheetClient> m_styleSheetClient;
    String m_strHref;
    Member<MediaQuerySet> m_mediaQueries;
    Member<StyleSheetContents> m_styleSheet;
    Member<CSSStyleSheetResource> m_resource;
    bool m_loading;
};

DEFINE_STYLE_RULE_TYPE_CASTS(Import);

} // namespace blink

#endif
