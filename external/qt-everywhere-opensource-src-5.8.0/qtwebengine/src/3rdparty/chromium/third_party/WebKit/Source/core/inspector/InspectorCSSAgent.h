/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorCSSAgent_h
#define InspectorCSSAgent_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "core/dom/SecurityContext.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/inspector/protocol/CSS.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSKeyframeRule;
class CSSRule;
class CSSRuleList;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class InspectedFrames;
class InspectorNetworkAgent;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class MediaList;
class Node;
class LayoutObject;

class CORE_EXPORT InspectorCSSAgent final
    : public InspectorBaseAgent<protocol::CSS::Metainfo>
    , public InspectorDOMAgent::DOMListener
    , public InspectorStyleSheetBase::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
    USING_GARBAGE_COLLECTED_MIXIN(InspectorCSSAgent);
public:
    enum MediaListSource {
        MediaListSourceLinkedSheet,
        MediaListSourceInlineSheet,
        MediaListSourceMediaRule,
        MediaListSourceImportRule
    };

    enum StyleSheetsUpdateType {
        InitialFrontendLoad = 0,
        ExistingFrontendRefresh,
    };

    class InlineStyleOverrideScope {
        STACK_ALLOCATED();
    public:
        InlineStyleOverrideScope(SecurityContext* context)
            : m_contentSecurityPolicy(context->contentSecurityPolicy())
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(true);
        }

        ~InlineStyleOverrideScope()
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(false);
        }

    private:
        Member<ContentSecurityPolicy> m_contentSecurityPolicy;
    };

    static CSSStyleRule* asCSSStyleRule(CSSRule*);
    static CSSMediaRule* asCSSMediaRule(CSSRule*);

    static InspectorCSSAgent* create(InspectorDOMAgent* domAgent, InspectedFrames* inspectedFrames, InspectorNetworkAgent* networkAgent, InspectorResourceContentLoader* resourceContentLoader, InspectorResourceContainer* resourceContainer)
    {
        return new InspectorCSSAgent(domAgent, inspectedFrames, networkAgent, resourceContentLoader, resourceContainer);
    }

    static void collectAllDocumentStyleSheets(Document*, HeapVector<Member<CSSStyleSheet>>&);

    ~InspectorCSSAgent() override;
    DECLARE_VIRTUAL_TRACE();

    bool forcePseudoState(Element*, CSSSelector::PseudoType);
    void didCommitLoadForLocalFrame(LocalFrame*) override;
    void restore() override;
    void flushPendingProtocolNotifications() override;
    void reset();
    void mediaQueryResultChanged();

    void activeStyleSheetsUpdated(Document*);
    void documentDetached(Document*);

    void enable(ErrorString*, std::unique_ptr<EnableCallback>) override;
    void disable(ErrorString*) override;
    void getMatchedStylesForNode(ErrorString*, int nodeId, Maybe<protocol::CSS::CSSStyle>* inlineStyle, Maybe<protocol::CSS::CSSStyle>* attributesStyle, Maybe<protocol::Array<protocol::CSS::RuleMatch>>* matchedCSSRules, Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>>*, Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>>*, Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>>*) override;
    void getInlineStylesForNode(ErrorString*, int nodeId, Maybe<protocol::CSS::CSSStyle>* inlineStyle, Maybe<protocol::CSS::CSSStyle>* attributesStyle) override;
    void getComputedStyleForNode(ErrorString*, int nodeId, std::unique_ptr<protocol::Array<protocol::CSS::CSSComputedStyleProperty>>*) override;
    void getPlatformFontsForNode(ErrorString*, int nodeId, std::unique_ptr<protocol::Array<protocol::CSS::PlatformFontUsage>>* fonts) override;
    void getStyleSheetText(ErrorString*, const String& styleSheetId, String* text) override;
    void setStyleSheetText(ErrorString*, const String& styleSheetId, const String& text, Maybe<String>* sourceMapURL) override;
    void setRuleSelector(ErrorString*, const String& styleSheetId, std::unique_ptr<protocol::CSS::SourceRange>, const String& selector, std::unique_ptr<protocol::CSS::SelectorList>*) override;
    void setKeyframeKey(ErrorString*, const String& styleSheetId, std::unique_ptr<protocol::CSS::SourceRange>, const String& keyText, std::unique_ptr<protocol::CSS::Value>* outKeyText) override;
    void setStyleTexts(ErrorString*, std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>> edits, std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* styles) override;
    void setMediaText(ErrorString*, const String& styleSheetId, std::unique_ptr<protocol::CSS::SourceRange>, const String& text, std::unique_ptr<protocol::CSS::CSSMedia>*) override;
    void createStyleSheet(ErrorString*, const String& frameId, String* styleSheetId) override;
    void addRule(ErrorString*, const String& styleSheetId, const String& ruleText, std::unique_ptr<protocol::CSS::SourceRange>, std::unique_ptr<protocol::CSS::CSSRule>*) override;
    void forcePseudoState(ErrorString*, int nodeId, std::unique_ptr<protocol::Array<String>> forcedPseudoClasses) override;
    void getMediaQueries(ErrorString*, std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>*) override;
    void setEffectivePropertyValueForNode(ErrorString*, int nodeId, const String& propertyName, const String& value) override;
    void getBackgroundColors(ErrorString*, int nodeId, Maybe<protocol::Array<String>>* backgroundColors) override;

    void collectMediaQueriesFromRule(CSSRule*, protocol::Array<protocol::CSS::CSSMedia>*);
    void collectMediaQueriesFromStyleSheet(CSSStyleSheet*, protocol::Array<protocol::CSS::CSSMedia>*);
    std::unique_ptr<protocol::CSS::CSSMedia> buildMediaObject(const MediaList*, MediaListSource, const String&, CSSStyleSheet*);
    std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>> buildMediaListChain(CSSRule*);

    CSSStyleDeclaration* findEffectiveDeclaration(CSSPropertyID, const HeapVector<Member<CSSStyleDeclaration>>& styles);
    void setLayoutEditorValue(ErrorString*, Element*, CSSStyleDeclaration*, CSSPropertyID, const String& value, bool forceImportant = false);
    void layoutEditorItemSelected(Element*, CSSStyleDeclaration*);

    HeapVector<Member<CSSStyleDeclaration>> matchingStyles(Element*);
    String styleSheetId(CSSStyleSheet*);
private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class ModifyRuleAction;
    class SetElementStyleAction;
    class AddRuleAction;

    static void collectStyleSheets(CSSStyleSheet*, HeapVector<Member<CSSStyleSheet>>&);

    InspectorCSSAgent(InspectorDOMAgent*, InspectedFrames*, InspectorNetworkAgent*, InspectorResourceContentLoader*, InspectorResourceContainer*);

    typedef HeapHashMap<String, Member<InspectorStyleSheet>> IdToInspectorStyleSheet;
    typedef HeapHashMap<String, Member<InspectorStyleSheetForInlineStyle>> IdToInspectorStyleSheetForInlineStyle;
    typedef HeapHashMap<Member<Node>, Member<InspectorStyleSheetForInlineStyle>> NodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

    void resourceContentLoaded(std::unique_ptr<EnableCallback>);
    void wasEnabled();
    void resetNonPersistentData();
    InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);
    Element* elementForId(ErrorString*, int nodeId);

    void updateActiveStyleSheets(Document*, StyleSheetsUpdateType);
    void setActiveStyleSheets(Document*, const HeapVector<Member<CSSStyleSheet>>&, StyleSheetsUpdateType);
    CSSStyleDeclaration* setStyleText(ErrorString*, InspectorStyleSheetBase*, const SourceRange&, const String&);
    bool multipleStyleTextsActions(ErrorString*, std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>, HeapVector<Member<StyleSheetAction>>* actions);

    std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>> animationsForNode(Element*);

    void collectPlatformFontsForLayoutObject(LayoutObject*, HashCountedSet<std::pair<int, String>>*);

    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    String unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* inspectorStyleSheetForRule(CSSStyleRule*);

    InspectorStyleSheet* viaInspectorStyleSheet(Document*, bool createIfAbsent);
    InspectorStyleSheet* assertInspectorStyleSheetForId(ErrorString*, const String&);
    InspectorStyleSheetBase* assertStyleSheetForId(ErrorString*, const String&);
    String detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    std::unique_ptr<protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>> buildArrayForMatchedRuleList(CSSRuleList*, Element*, PseudoId);
    std::unique_ptr<protocol::CSS::CSSStyle> buildObjectForAttributesStyle(Element*);

    // InspectorDOMAgent::DOMListener implementation
    void didRemoveDocument(Document*) override;
    void didRemoveDOMNode(Node*) override;
    void didModifyDOMAttr(Element*) override;

    // InspectorStyleSheet::Listener implementation
    void styleSheetChanged(InspectorStyleSheetBase*) override;
    void willReparseStyleSheet() override;
    void didReparseStyleSheet() override;

    void resetPseudoStates();

    Member<InspectorDOMAgent> m_domAgent;
    Member<InspectedFrames> m_inspectedFrames;
    Member<InspectorNetworkAgent> m_networkAgent;
    Member<InspectorResourceContentLoader> m_resourceContentLoader;
    Member<InspectorResourceContainer> m_resourceContainer;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    IdToInspectorStyleSheetForInlineStyle m_idToInspectorStyleSheetForInlineStyle;
    HeapHashMap<Member<CSSStyleSheet>, Member<InspectorStyleSheet>> m_cssStyleSheetToInspectorStyleSheet;
    typedef HeapHashMap<Member<Document>, Member<HeapHashSet<Member<CSSStyleSheet>>>> DocumentStyleSheets;
    DocumentStyleSheets m_documentToCSSStyleSheets;
    HeapHashSet<Member<Document>> m_invalidatedDocuments;

    NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
    HeapHashMap<Member<Document>, Member<InspectorStyleSheet>> m_documentToViaInspectorStyleSheet; // "via inspector" stylesheets
    NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;

    Member<CSSStyleSheet> m_inspectorUserAgentStyleSheet;

    bool m_creatingViaInspectorStyleSheet;
    bool m_isSettingStyleSheetText;
    int m_resourceContentLoaderClientId;

    friend class InspectorResourceContentLoaderCallback;
    friend class StyleSheetBinder;
};


} // namespace blink

#endif // !defined(InspectorCSSAgent_h)
