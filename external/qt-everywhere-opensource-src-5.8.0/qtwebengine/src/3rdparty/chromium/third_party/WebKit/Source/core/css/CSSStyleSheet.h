/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSStyleSheet_h
#define CSSStyleSheet_h

#include "core/CoreExport.h"
#include "core/css/CSSRule.h"
#include "core/css/StyleSheet.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/TextPosition.h"

namespace blink {

class CSSImportRule;
class CSSRule;
class CSSRuleList;
class CSSStyleSheet;
class Document;
class ExceptionState;
class MediaQuerySet;
class SecurityOrigin;
class StyleSheetContents;

enum StyleSheetUpdateType {
    PartialRuleUpdate,
    EntireStyleSheetUpdate
};

class CORE_EXPORT CSSStyleSheet final : public StyleSheet {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(CSSStyleSheet);
public:
    static CSSStyleSheet* create(StyleSheetContents*, CSSImportRule* ownerRule = 0);
    static CSSStyleSheet* create(StyleSheetContents*, Node* ownerNode);
    static CSSStyleSheet* createInline(Node*, const KURL&, const TextPosition& startPosition = TextPosition::minimumPosition(), const String& encoding = String());
    static CSSStyleSheet* createInline(StyleSheetContents*, Node* ownerNode, const TextPosition& startPosition = TextPosition::minimumPosition());

    ~CSSStyleSheet() override;

    CSSStyleSheet* parentStyleSheet() const override;
    Node* ownerNode() const override { return m_ownerNode; }
    MediaList* media() const override;
    String href() const override;
    String title() const override { return m_title; }
    bool disabled() const override { return m_isDisabled; }
    void setDisabled(bool) override;

    CSSRuleList* cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionState&);
    unsigned insertRule(const String& rule, ExceptionState&); // Deprecated.
    void deleteRule(unsigned index, ExceptionState&);

    // IE Extensions
    CSSRuleList* rules();
    int addRule(const String& selector, const String& style, int index, ExceptionState&);
    int addRule(const String& selector, const String& style, ExceptionState&);
    void removeRule(unsigned index, ExceptionState& exceptionState) { deleteRule(index, exceptionState); }

    // For CSSRuleList.
    unsigned length() const;
    CSSRule* item(unsigned index);

    void clearOwnerNode() override;

    CSSRule* ownerRule() const override { return m_ownerRule; }
    KURL baseURL() const override;
    bool isLoading() const override;

    void clearOwnerRule() { m_ownerRule = nullptr; }
    Document* ownerDocument() const;
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(MediaQuerySet*);
    void setTitle(const String& title) { m_title = title; }
    // Set by LinkStyle iff CORS-enabled fetch of stylesheet succeeded from this origin.
    void setAllowRuleAccessFromOrigin(PassRefPtr<SecurityOrigin> allowedOrigin);

    class RuleMutationScope {
        WTF_MAKE_NONCOPYABLE(RuleMutationScope);
        STACK_ALLOCATED();
    public:
        explicit RuleMutationScope(CSSStyleSheet*);
        explicit RuleMutationScope(CSSRule*);
        ~RuleMutationScope();

    private:
        Member<CSSStyleSheet> m_styleSheet;
    };

    void willMutateRules();
    void didMutateRules();
    void didMutate(StyleSheetUpdateType = PartialRuleUpdate);

    StyleSheetContents* contents() const { return m_contents.get(); }

    bool isInline() const { return m_isInlineStylesheet; }
    TextPosition startPositionInSource() const { return m_startPosition; }

    bool sheetLoaded();
    bool loadCompleted() const { return m_loadCompleted; }
    void startLoadingDynamicSheet();
    void setText(const String&);

    DECLARE_VIRTUAL_TRACE();

private:
    CSSStyleSheet(StyleSheetContents*, CSSImportRule* ownerRule);
    CSSStyleSheet(StyleSheetContents*, Node* ownerNode, bool isInlineStylesheet, const TextPosition& startPosition);

    bool isCSSStyleSheet() const override { return true; }
    String type() const override { return "text/css"; }

    void reattachChildRuleCSSOMWrappers();

    bool canAccessRules() const;

    void setLoadCompleted(bool);

    Member<StyleSheetContents> m_contents;
    bool m_isInlineStylesheet;
    bool m_isDisabled;
    String m_title;
    Member<MediaQuerySet> m_mediaQueries;

    RefPtr<SecurityOrigin> m_allowRuleAccessFromOrigin;

    Member<Node> m_ownerNode;
    Member<CSSRule> m_ownerRule;

    TextPosition m_startPosition;
    bool m_loadCompleted;
    mutable Member<MediaList> m_mediaCSSOMWrapper;
    mutable HeapVector<Member<CSSRule>> m_childRuleCSSOMWrappers;
    mutable Member<CSSRuleList> m_ruleListCSSOMWrapper;
};

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSStyleSheet* sheet)
    : m_styleSheet(sheet)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSRule* rule)
    : m_styleSheet(rule ? rule->parentStyleSheet() : 0)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::~RuleMutationScope()
{
    if (m_styleSheet)
        m_styleSheet->didMutateRules();
}

DEFINE_TYPE_CASTS(CSSStyleSheet, StyleSheet, sheet, sheet->isCSSStyleSheet(), sheet.isCSSStyleSheet());

} // namespace blink

#endif
