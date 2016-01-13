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

#include "core/css/CSSSelector.h"
#include "core/dom/SecurityContext.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "platform/JSONValues.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct CSSParserString;
class CSSRule;
class CSSRuleList;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class InspectorFrontend;
class InspectorResourceAgent;
class InstrumentingAgents;
class MediaList;
class Node;
class PlatformFontUsage;
class RenderText;
class StyleResolver;

class InspectorCSSAgent FINAL
    : public InspectorBaseAgent<InspectorCSSAgent>
    , public InspectorDOMAgent::DOMListener
    , public InspectorBackendDispatcher::CSSCommandHandler
    , public InspectorStyleSheetBase::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
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
        ContentSecurityPolicy* m_contentSecurityPolicy;
    };

    static CSSStyleRule* asCSSStyleRule(CSSRule*);

    static PassOwnPtr<InspectorCSSAgent> create(InspectorDOMAgent* domAgent, InspectorPageAgent* pageAgent, InspectorResourceAgent* resourceAgent)
    {
        return adoptPtr(new InspectorCSSAgent(domAgent, pageAgent, resourceAgent));
    }

    static void collectAllDocumentStyleSheets(Document*, Vector<CSSStyleSheet*>&);

    virtual ~InspectorCSSAgent();

    bool forcePseudoState(Element*, CSSSelector::PseudoType);
    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void discardAgent() OVERRIDE;
    virtual void didCommitLoadForMainFrame() OVERRIDE;
    virtual void restore() OVERRIDE;
    virtual void flushPendingFrontendMessages() OVERRIDE;
    virtual void enable(ErrorString*, PassRefPtr<EnableCallback>) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    void reset();
    void mediaQueryResultChanged();
    void willMutateRules();
    void didMutateRules(CSSStyleSheet*);
    void willMutateStyle();
    void didMutateStyle(CSSStyleDeclaration*, bool);

    void activeStyleSheetsUpdated(Document*);
    void documentDetached(Document*);

    virtual void getComputedStyleForNode(ErrorString*, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> >&) OVERRIDE;
    virtual void getPlatformFontsForNode(ErrorString*, int nodeId, String* cssFamilyName, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage> >&) OVERRIDE;
    virtual void getInlineStylesForNode(ErrorString*, int nodeId, RefPtr<TypeBuilder::CSS::CSSStyle>& inlineStyle, RefPtr<TypeBuilder::CSS::CSSStyle>& attributes) OVERRIDE;
    virtual void getMatchedStylesForNode(ErrorString*, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> >& matchedCSSRules, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> >&, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> >& inheritedEntries) OVERRIDE;
    virtual void getStyleSheetText(ErrorString*, const String& styleSheetId, String* result) OVERRIDE;
    virtual void setStyleSheetText(ErrorString*, const String& styleSheetId, const String& text) OVERRIDE;

    virtual void setPropertyText(ErrorString*, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& text, RefPtr<TypeBuilder::CSS::CSSStyle>& result) OVERRIDE;
    virtual void setRuleSelector(ErrorString*, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result) OVERRIDE;
    virtual void createStyleSheet(ErrorString*, const String& frameId, TypeBuilder::CSS::StyleSheetId* outStyleSheetId) OVERRIDE;
    virtual void addRule(ErrorString*, const String& styleSheetId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result) OVERRIDE;
    virtual void forcePseudoState(ErrorString*, int nodeId, const RefPtr<JSONArray>& forcedPseudoClasses) OVERRIDE;
    virtual void getMediaQueries(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >& medias) OVERRIDE;


    bool collectMediaQueriesFromRule(CSSRule*, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray);
    bool collectMediaQueriesFromStyleSheet(CSSStyleSheet*, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray);
    PassRefPtr<TypeBuilder::CSS::CSSMedia> buildMediaObject(const MediaList*, MediaListSource, const String&, CSSStyleSheet*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > buildMediaListChain(CSSRule*);

private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class SetPropertyTextAction;
    class SetRuleSelectorAction;
    class AddRuleAction;
    class InspectorResourceContentLoaderCallback;

    static void collectStyleSheets(CSSStyleSheet*, Vector<CSSStyleSheet*>&);

    InspectorCSSAgent(InspectorDOMAgent*, InspectorPageAgent*, InspectorResourceAgent*);

    typedef HashMap<String, RefPtr<InspectorStyleSheet> > IdToInspectorStyleSheet;
    typedef HashMap<String, RefPtr<InspectorStyleSheetForInlineStyle> > IdToInspectorStyleSheetForInlineStyle;
    typedef HashMap<Node*, RefPtr<InspectorStyleSheetForInlineStyle> > NodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

    void wasEnabled();
    void resetNonPersistentData();
    InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);
    Element* elementForId(ErrorString*, int nodeId);

    void updateActiveStyleSheets(Document*, StyleSheetsUpdateType);
    void setActiveStyleSheets(Document*, const Vector<CSSStyleSheet*>&, StyleSheetsUpdateType);

    void collectPlatformFontsForRenderer(RenderText*, HashCountedSet<String>*);

    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    String unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* viaInspectorStyleSheet(Document*, bool createIfAbsent);
    InspectorStyleSheet* assertInspectorStyleSheetForId(ErrorString*, const String&);
    InspectorStyleSheetBase* assertStyleSheetForId(ErrorString*, const String&);
    TypeBuilder::CSS::StyleSheetOrigin::Enum detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);
    bool styleSheetEditInProgress() const { return m_styleSheetsPendingMutation || m_styleDeclarationPendingMutation || m_isSettingStyleSheetText; }

    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > buildArrayForMatchedRuleList(CSSRuleList*, Element*);
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForAttributesStyle(Element*);

    // InspectorDOMAgent::DOMListener implementation
    virtual void didRemoveDocument(Document*) OVERRIDE;
    virtual void didRemoveDOMNode(Node*) OVERRIDE;
    virtual void didModifyDOMAttr(Element*) OVERRIDE;

    // InspectorStyleSheet::Listener implementation
    virtual void styleSheetChanged(InspectorStyleSheetBase*) OVERRIDE;
    virtual void willReparseStyleSheet() OVERRIDE;
    virtual void didReparseStyleSheet() OVERRIDE;

    void resetPseudoStates();

    InspectorFrontend::CSS* m_frontend;
    InspectorDOMAgent* m_domAgent;
    InspectorPageAgent* m_pageAgent;
    InspectorResourceAgent* m_resourceAgent;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    IdToInspectorStyleSheetForInlineStyle m_idToInspectorStyleSheetForInlineStyle;
    HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet> > m_cssStyleSheetToInspectorStyleSheet;
    typedef HashMap<Document*, OwnPtr<HashSet<CSSStyleSheet*> > > DocumentStyleSheets;
    DocumentStyleSheets m_documentToCSSStyleSheets;
    HashSet<Document*> m_invalidatedDocuments;

    NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
    WillBePersistentHeapHashMap<RefPtrWillBeMember<Document>, RefPtr<InspectorStyleSheet> > m_documentToViaInspectorStyleSheet; // "via inspector" stylesheets
    NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;

    RefPtrWillBePersistent<CSSStyleSheet> m_inspectorUserAgentStyleSheet;

    int m_lastStyleSheetId;
    int m_styleSheetsPendingMutation;
    bool m_styleDeclarationPendingMutation;
    bool m_creatingViaInspectorStyleSheet;
    bool m_isSettingStyleSheetText;

    friend class InspectorResourceContentLoaderCallback;
    friend class StyleSheetBinder;
};


} // namespace WebCore

#endif // !defined(InspectorCSSAgent_h)
