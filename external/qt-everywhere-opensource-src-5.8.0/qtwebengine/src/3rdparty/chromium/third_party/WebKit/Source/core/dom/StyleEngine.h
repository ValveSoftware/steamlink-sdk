/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef StyleEngine_h
#define StyleEngine_h

#include "core/CoreExport.h"
#include "core/css/CSSFontSelectorClient.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentOrderedList.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/StyleEngineContext.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/ListHashSet.h"
#include "wtf/TemporaryChange.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class CSSFontSelector;
class CSSStyleSheet;
class Node;
class RuleFeatureSet;
class ShadowTreeStyleSheetCollection;
class StyleRuleFontFace;
class StyleSheet;
class StyleSheetContents;

class CORE_EXPORT StyleEngine final : public GarbageCollectedFinalized<StyleEngine>, public CSSFontSelectorClient  {
    USING_GARBAGE_COLLECTED_MIXIN(StyleEngine);
public:

    class IgnoringPendingStylesheet : public TemporaryChange<bool> {
        DISALLOW_NEW();
    public:
        IgnoringPendingStylesheet(StyleEngine& engine)
            : TemporaryChange<bool>(engine.m_ignorePendingStylesheets, true)
        {
        }
    };

    friend class IgnoringPendingStylesheet;

    static StyleEngine* create(Document& document) { return new StyleEngine(document); }

    ~StyleEngine();

    const HeapVector<Member<StyleSheet>>& styleSheetsForStyleSheetList(TreeScope&);

    const HeapVector<Member<CSSStyleSheet>>& injectedAuthorStyleSheets() const { return m_injectedAuthorStyleSheets; }

    const HeapVector<Member<CSSStyleSheet>> activeStyleSheetsForInspector() const;

    void setNeedsActiveStyleUpdate(StyleSheet*, StyleResolverUpdateMode);
    void addStyleSheetCandidateNode(Node*);
    void removeStyleSheetCandidateNode(Node*);
    void removeStyleSheetCandidateNode(Node*, TreeScope&);
    void modifiedStyleSheetCandidateNode(Node*);
    void watchedSelectorsChanged();

    void injectAuthorSheet(StyleSheetContents* authorSheet);

    void clearMediaQueryRuleSetStyleSheets();
    void updateStyleSheetsInImport(DocumentStyleSheetCollector& parentCollector);
    void updateActiveStyleSheets(StyleResolverUpdateMode);

    String preferredStylesheetSetName() const { return m_preferredStylesheetSetName; }
    String selectedStylesheetSetName() const { return m_selectedStylesheetSetName; }
    void setPreferredStylesheetSetNameIfNotSet(const String&);
    void setSelectedStylesheetSetName(const String&);
    void setHttpDefaultStyle(const String&);

    void addPendingSheet(StyleEngineContext&);
    void removePendingSheet(Node* styleSheetCandidateNode, const StyleEngineContext&);

    bool hasPendingScriptBlockingSheets() const { return m_pendingScriptBlockingStylesheets > 0; }
    bool hasPendingRenderBlockingSheets() const { return m_pendingRenderBlockingStylesheets > 0; }
    bool haveScriptBlockingStylesheetsLoaded() const { return !hasPendingScriptBlockingSheets() || m_ignorePendingStylesheets; }
    bool haveRenderBlockingStylesheetsLoaded() const { return !hasPendingRenderBlockingSheets() || m_ignorePendingStylesheets; }
    bool ignoringPendingStylesheets() const { return m_ignorePendingStylesheets; }

    unsigned maxDirectAdjacentSelectors() const { return m_maxDirectAdjacentSelectors; }
    bool usesSiblingRules() const { return m_usesSiblingRules; }
    bool usesFirstLineRules() const { return m_usesFirstLineRules; }
    bool usesWindowInactiveSelector() const { return m_usesWindowInactiveSelector; }

    bool usesRemUnits() const { return m_usesRemUnits; }
    void setUsesRemUnit(bool b) { m_usesRemUnits = b; }

    void resetCSSFeatureFlags(const RuleFeatureSet&);

    void didRemoveShadowRoot(ShadowRoot*);
    void shadowRootRemovedFromDocument(ShadowRoot*);
    void appendActiveAuthorStyleSheets();

    void setHasUnresolvedKeyframesRule() { m_hasUnresolvedKeyframesRule = true; }
    void keyframesRulesAdded();

    StyleResolver* resolver() const
    {
        return m_resolver.get();
    }

    StyleResolver& ensureResolver()
    {
        if (!m_resolver) {
            createResolver();
        } else if (m_resolver->hasPendingAuthorStyleSheets()) {
            m_resolver->appendPendingAuthorStyleSheets();
        }
        return *m_resolver.get();
    }

    bool hasResolver() const { return m_resolver.get(); }
    void clearResolver();
    void clearMasterResolver();

    StyleInvalidator& styleInvalidator() { return m_styleInvalidator; }

    CSSFontSelector* fontSelector() { return m_fontSelector.get(); }
    void setFontSelector(CSSFontSelector*);

    void removeFontFaceRules(const HeapVector<Member<const StyleRuleFontFace>>&);
    void clearFontCache();
    // updateGenericFontFamilySettings is used from WebSettingsImpl.
    void updateGenericFontFamilySettings();

    void didDetach();
    bool shouldClearResolver() const;
    void resolverChanged(StyleResolverUpdateMode);

    CSSStyleSheet* createSheet(Element*, const String& text, TextPosition startPosition, StyleEngineContext&);
    void removeSheet(StyleSheetContents*);

    void collectScopedStyleFeaturesTo(RuleFeatureSet&) const;
    void ensureFullscreenUAStyle();

    void platformColorsChanged();

    void classChangedForElement(const SpaceSplitString& changedClasses, Element&);
    void classChangedForElement(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element&);
    void attributeChangedForElement(const QualifiedName& attributeName, Element&);
    void idChangedForElement(const AtomicString& oldId, const AtomicString& newId, Element&);
    void pseudoStateChangedForElement(CSSSelector::PseudoType, Element&);
    void scheduleSiblingInvalidationsForElement(Element&, ContainerNode& schedulingParent);
    void scheduleInvalidationsForInsertedSibling(Element* beforeElement, Element& insertedElement);
    void scheduleInvalidationsForRemovedSibling(Element* beforeElement, Element& removedElement, Element& afterElement);
    unsigned styleForElementCount() const { return m_styleForElementCount; }
    void incStyleForElementCount() { m_styleForElementCount++; }

    StyleResolverStats* stats() { return m_styleResolverStats.get(); }
    void setStatsEnabled(bool);

    DECLARE_VIRTUAL_TRACE();

    DECLARE_TRACE_WRAPPERS();

private:
    // CSSFontSelectorClient implementation.
    void fontsNeedUpdate(CSSFontSelector*) override;

private:
    StyleEngine(Document&);

    TreeScopeStyleSheetCollection* ensureStyleSheetCollectionFor(TreeScope&);
    TreeScopeStyleSheetCollection* styleSheetCollectionFor(TreeScope&);
    bool shouldUpdateDocumentStyleSheetCollection(StyleResolverUpdateMode) const;
    bool shouldUpdateShadowTreeStyleSheetCollection(StyleResolverUpdateMode) const;

    void markDocumentDirty();
    void markTreeScopeDirty(TreeScope&);

    bool isMaster() const { return m_isMaster; }
    Document* master();
    Document& document() const { return *m_document; }

    typedef HeapHashSet<Member<TreeScope>> UnorderedTreeScopeSet;

    void clearMediaQueryRuleSetOnTreeScopeStyleSheets(UnorderedTreeScopeSet&);

    void createResolver();

    static CSSStyleSheet* parseSheet(Element*, const String& text, TextPosition startPosition);

    const DocumentStyleSheetCollection* documentStyleSheetCollection() const
    {
        return m_documentStyleSheetCollection.get();
    }

    DocumentStyleSheetCollection* documentStyleSheetCollection()
    {
        return m_documentStyleSheetCollection.get();
    }

    void updateActiveStyleSheetsInShadow(StyleResolverUpdateMode, TreeScope*, UnorderedTreeScopeSet& treeScopesRemoved);

    bool shouldSkipInvalidationFor(const Element&) const;

    Member<Document> m_document;
    bool m_isMaster;

    // Track the number of currently loading top-level stylesheets needed for layout.
    // Sheets loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements and when it is safe to execute scripts.
    int m_pendingScriptBlockingStylesheets = 0;
    int m_pendingRenderBlockingStylesheets = 0;

    HeapVector<Member<CSSStyleSheet>> m_injectedAuthorStyleSheets;

    Member<DocumentStyleSheetCollection> m_documentStyleSheetCollection;

    typedef HeapHashMap<WeakMember<TreeScope>, Member<ShadowTreeStyleSheetCollection>> StyleSheetCollectionMap;
    StyleSheetCollectionMap m_styleSheetCollectionMap;

    bool m_documentScopeDirty = true;
    UnorderedTreeScopeSet m_dirtyTreeScopes;
    UnorderedTreeScopeSet m_activeTreeScopes;

    String m_preferredStylesheetSetName;
    String m_selectedStylesheetSetName;

    bool m_usesSiblingRules = false;
    bool m_usesFirstLineRules = false;
    bool m_usesWindowInactiveSelector = false;
    bool m_usesRemUnits = false;
    unsigned m_maxDirectAdjacentSelectors = 0;

    bool m_ignorePendingStylesheets = false;
    bool m_didCalculateResolver = false;
    bool m_hasUnresolvedKeyframesRule = false;

    Member<StyleResolver> m_resolver;
    StyleInvalidator m_styleInvalidator;

    Member<CSSFontSelector> m_fontSelector;

    HeapHashMap<AtomicString, Member<StyleSheetContents>> m_textToSheetCache;
    HeapHashMap<Member<StyleSheetContents>, AtomicString> m_sheetToTextCache;

    std::unique_ptr<StyleResolverStats> m_styleResolverStats;
    unsigned m_styleForElementCount = 0;

    friend class StyleEngineTest;
};

} // namespace blink

#endif
