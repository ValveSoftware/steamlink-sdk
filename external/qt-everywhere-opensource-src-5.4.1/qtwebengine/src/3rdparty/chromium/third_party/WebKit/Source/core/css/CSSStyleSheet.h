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

#include "core/css/CSSRule.h"
#include "core/css/StyleSheet.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

class CSSCharsetRule;
class CSSImportRule;
class BisonCSSParser;
class CSSRule;
class CSSRuleList;
class CSSStyleSheet;
class CSSStyleSheetResource;
class Document;
class ExceptionState;
class MediaQuerySet;
class SecurityOrigin;
class StyleSheetContents;

enum StyleSheetUpdateType {
    PartialRuleUpdate,
    EntireStyleSheetUpdate
};

class CSSStyleSheet FINAL : public StyleSheet {
public:
    static PassRefPtrWillBeRawPtr<CSSStyleSheet> create(PassRefPtrWillBeRawPtr<StyleSheetContents>, CSSImportRule* ownerRule = 0);
    static PassRefPtrWillBeRawPtr<CSSStyleSheet> create(PassRefPtrWillBeRawPtr<StyleSheetContents>, Node* ownerNode);
    static PassRefPtrWillBeRawPtr<CSSStyleSheet> createInline(Node*, const KURL&, const TextPosition& startPosition = TextPosition::minimumPosition(), const String& encoding = String());
    static PassRefPtrWillBeRawPtr<CSSStyleSheet> createInline(PassRefPtrWillBeRawPtr<StyleSheetContents>, Node* ownerNode, const TextPosition& startPosition = TextPosition::minimumPosition());

    virtual ~CSSStyleSheet();

    virtual CSSStyleSheet* parentStyleSheet() const OVERRIDE;
    virtual Node* ownerNode() const OVERRIDE { return m_ownerNode; }
    virtual MediaList* media() const OVERRIDE;
    virtual String href() const OVERRIDE;
    virtual String title() const OVERRIDE { return m_title; }
    virtual bool disabled() const OVERRIDE { return m_isDisabled; }
    virtual void setDisabled(bool) OVERRIDE;

    PassRefPtrWillBeRawPtr<CSSRuleList> cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionState&);
    unsigned insertRule(const String& rule, ExceptionState&); // Deprecated.
    void deleteRule(unsigned index, ExceptionState&);

    // IE Extensions
    PassRefPtrWillBeRawPtr<CSSRuleList> rules();
    int addRule(const String& selector, const String& style, int index, ExceptionState&);
    int addRule(const String& selector, const String& style, ExceptionState&);
    void removeRule(unsigned index, ExceptionState& exceptionState) { deleteRule(index, exceptionState); }

    // For CSSRuleList.
    unsigned length() const;
    CSSRule* item(unsigned index);

    virtual void clearOwnerNode() OVERRIDE;

    virtual CSSRule* ownerRule() const OVERRIDE { return m_ownerRule; }
    virtual KURL baseURL() const OVERRIDE;
    virtual bool isLoading() const OVERRIDE;

    void clearOwnerRule() { m_ownerRule = nullptr; }
    Document* ownerDocument() const;
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtrWillBeRawPtr<MediaQuerySet>);
    void setTitle(const String& title) { m_title = title; }

    class RuleMutationScope {
        WTF_MAKE_NONCOPYABLE(RuleMutationScope);
        STACK_ALLOCATED();
    public:
        explicit RuleMutationScope(CSSStyleSheet*);
        explicit RuleMutationScope(CSSRule*);
        ~RuleMutationScope();

    private:
        RawPtrWillBeMember<CSSStyleSheet> m_styleSheet;
    };

    void willMutateRules();
    void didMutateRules();
    void didMutate(StyleSheetUpdateType = PartialRuleUpdate);

    void clearChildRuleCSSOMWrappers();

    StyleSheetContents* contents() const { return m_contents.get(); }

    bool isInline() const { return m_isInlineStylesheet; }
    TextPosition startPositionInSource() const { return m_startPosition; }

    bool sheetLoaded();
    bool loadCompleted() const { return m_loadCompleted; }
    void startLoadingDynamicSheet();

    virtual void trace(Visitor*) OVERRIDE;

private:
    CSSStyleSheet(PassRefPtrWillBeRawPtr<StyleSheetContents>, CSSImportRule* ownerRule);
    CSSStyleSheet(PassRefPtrWillBeRawPtr<StyleSheetContents>, Node* ownerNode, bool isInlineStylesheet, const TextPosition& startPosition);

    virtual bool isCSSStyleSheet() const OVERRIDE { return true; }
    virtual String type() const OVERRIDE { return "text/css"; }

    void reattachChildRuleCSSOMWrappers();

    bool canAccessRules() const;

    void setLoadCompleted(bool);

    RefPtrWillBeMember<StyleSheetContents> m_contents;
    bool m_isInlineStylesheet;
    bool m_isDisabled;
    String m_title;
    RefPtrWillBeMember<MediaQuerySet> m_mediaQueries;

    RawPtrWillBeMember<Node> m_ownerNode;
    RawPtrWillBeMember<CSSRule> m_ownerRule;

    TextPosition m_startPosition;
    bool m_loadCompleted;
    mutable RefPtrWillBeMember<MediaList> m_mediaCSSOMWrapper;
    mutable WillBeHeapVector<RefPtrWillBeMember<CSSRule> > m_childRuleCSSOMWrappers;
    mutable OwnPtrWillBeMember<CSSRuleList> m_ruleListCSSOMWrapper;
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

} // namespace

#endif
