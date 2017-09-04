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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
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
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

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
class StyleRuleUsageTracker;

class CORE_EXPORT InspectorCSSAgent final
    : public InspectorBaseAgent<protocol::CSS::Metainfo>,
      public InspectorDOMAgent::DOMListener,
      public InspectorStyleSheetBase::Listener {
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
        : m_contentSecurityPolicy(context->contentSecurityPolicy()) {
      m_contentSecurityPolicy->setOverrideAllowInlineStyle(true);
    }

    ~InlineStyleOverrideScope() {
      m_contentSecurityPolicy->setOverrideAllowInlineStyle(false);
    }

   private:
    Member<ContentSecurityPolicy> m_contentSecurityPolicy;
  };

  static CSSStyleRule* asCSSStyleRule(CSSRule*);
  static CSSMediaRule* asCSSMediaRule(CSSRule*);

  static InspectorCSSAgent* create(
      InspectorDOMAgent* domAgent,
      InspectedFrames* inspectedFrames,
      InspectorNetworkAgent* networkAgent,
      InspectorResourceContentLoader* resourceContentLoader,
      InspectorResourceContainer* resourceContainer) {
    return new InspectorCSSAgent(domAgent, inspectedFrames, networkAgent,
                                 resourceContentLoader, resourceContainer);
  }

  static void collectAllDocumentStyleSheets(Document*,
                                            HeapVector<Member<CSSStyleSheet>>&);

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
  void fontsUpdated();
  void getUnusedRules(
      std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>*);
  void setUsageTrackerStatus(bool enabled);

  void enable(std::unique_ptr<EnableCallback>) override;
  Response disable() override;
  Response getMatchedStylesForNode(
      int nodeId,
      Maybe<protocol::CSS::CSSStyle>* inlineStyle,
      Maybe<protocol::CSS::CSSStyle>* attributesStyle,
      Maybe<protocol::Array<protocol::CSS::RuleMatch>>* matchedCSSRules,
      Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>>*,
      Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>>*,
      Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>>*) override;
  Response getInlineStylesForNode(
      int nodeId,
      Maybe<protocol::CSS::CSSStyle>* inlineStyle,
      Maybe<protocol::CSS::CSSStyle>* attributesStyle) override;
  Response getComputedStyleForNode(
      int nodeId,
      std::unique_ptr<
          protocol::Array<protocol::CSS::CSSComputedStyleProperty>>*) override;
  Response getPlatformFontsForNode(
      int nodeId,
      std::unique_ptr<protocol::Array<protocol::CSS::PlatformFontUsage>>* fonts)
      override;
  Response collectClassNames(
      const String& styleSheetId,
      std::unique_ptr<protocol::Array<String>>* classNames) override;
  Response getStyleSheetText(const String& styleSheetId, String* text) override;
  Response setStyleSheetText(const String& styleSheetId,
                             const String& text,
                             Maybe<String>* sourceMapURL) override;
  Response setRuleSelector(
      const String& styleSheetId,
      std::unique_ptr<protocol::CSS::SourceRange>,
      const String& selector,
      std::unique_ptr<protocol::CSS::SelectorList>*) override;
  Response setKeyframeKey(
      const String& styleSheetId,
      std::unique_ptr<protocol::CSS::SourceRange>,
      const String& keyText,
      std::unique_ptr<protocol::CSS::Value>* outKeyText) override;
  Response setStyleTexts(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>
          edits,
      std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* styles)
      override;
  Response setMediaText(const String& styleSheetId,
                        std::unique_ptr<protocol::CSS::SourceRange>,
                        const String& text,
                        std::unique_ptr<protocol::CSS::CSSMedia>*) override;
  Response createStyleSheet(const String& frameId,
                            String* styleSheetId) override;
  Response addRule(const String& styleSheetId,
                   const String& ruleText,
                   std::unique_ptr<protocol::CSS::SourceRange>,
                   std::unique_ptr<protocol::CSS::CSSRule>*) override;
  Response forcePseudoState(
      int nodeId,
      std::unique_ptr<protocol::Array<String>> forcedPseudoClasses) override;
  Response getMediaQueries(
      std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>*) override;
  Response setEffectivePropertyValueForNode(int nodeId,
                                            const String& propertyName,
                                            const String& value) override;
  Response getBackgroundColors(
      int nodeId,
      Maybe<protocol::Array<String>>* backgroundColors) override;

  Response startRuleUsageTracking() override;

  Response stopRuleUsageTracking(
      std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result)
      override;

  void collectMediaQueriesFromRule(CSSRule*,
                                   protocol::Array<protocol::CSS::CSSMedia>*);
  void collectMediaQueriesFromStyleSheet(
      CSSStyleSheet*,
      protocol::Array<protocol::CSS::CSSMedia>*);
  std::unique_ptr<protocol::CSS::CSSMedia> buildMediaObject(const MediaList*,
                                                            MediaListSource,
                                                            const String&,
                                                            CSSStyleSheet*);
  std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>> buildMediaListChain(
      CSSRule*);

  CSSStyleDeclaration* findEffectiveDeclaration(
      CSSPropertyID,
      const HeapVector<Member<CSSStyleDeclaration>>& styles);
  Response setLayoutEditorValue(Element*,
                                CSSStyleDeclaration*,
                                CSSPropertyID,
                                const String& value,
                                bool forceImportant = false);
  void layoutEditorItemSelected(Element*, CSSStyleDeclaration*);
  Response getLayoutTreeAndStyles(
      std::unique_ptr<protocol::Array<String>> styleWhitelist,
      std::unique_ptr<protocol::Array<protocol::CSS::LayoutTreeNode>>*
          layoutTreeNodes,
      std::unique_ptr<protocol::Array<protocol::CSS::ComputedStyle>>*
          computedStyles) override;

  HeapVector<Member<CSSStyleDeclaration>> matchingStyles(Element*);
  String styleSheetId(CSSStyleSheet*);

 private:
  class StyleSheetAction;
  class SetStyleSheetTextAction;
  class ModifyRuleAction;
  class SetElementStyleAction;
  class AddRuleAction;

  static void collectStyleSheets(CSSStyleSheet*,
                                 HeapVector<Member<CSSStyleSheet>>&);

  InspectorCSSAgent(InspectorDOMAgent*,
                    InspectedFrames*,
                    InspectorNetworkAgent*,
                    InspectorResourceContentLoader*,
                    InspectorResourceContainer*);

  typedef HeapHashMap<String, Member<InspectorStyleSheet>>
      IdToInspectorStyleSheet;
  typedef HeapHashMap<String, Member<InspectorStyleSheetForInlineStyle>>
      IdToInspectorStyleSheetForInlineStyle;
  typedef HeapHashMap<Member<Node>, Member<InspectorStyleSheetForInlineStyle>>
      NodeToInspectorStyleSheet;  // bogus "stylesheets" with elements' inline
                                  // styles
  typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

  void resourceContentLoaded(std::unique_ptr<EnableCallback>);
  void wasEnabled();
  void resetNonPersistentData();
  InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);

  void updateActiveStyleSheets(Document*, StyleSheetsUpdateType);
  void setActiveStyleSheets(Document*,
                            const HeapVector<Member<CSSStyleSheet>>&,
                            StyleSheetsUpdateType);
  Response setStyleText(InspectorStyleSheetBase*,
                        const SourceRange&,
                        const String&,
                        CSSStyleDeclaration*&);
  Response multipleStyleTextsActions(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>,
      HeapVector<Member<StyleSheetAction>>* actions);

  std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>>
  animationsForNode(Element*);

  void collectPlatformFontsForLayoutObject(
      LayoutObject*,
      HashCountedSet<std::pair<int, String>>*);

  InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
  String unbindStyleSheet(InspectorStyleSheet*);
  InspectorStyleSheet* inspectorStyleSheetForRule(CSSStyleRule*);

  InspectorStyleSheet* viaInspectorStyleSheet(Document*);
  Response assertInspectorStyleSheetForId(const String&, InspectorStyleSheet*&);
  Response assertStyleSheetForId(const String&, InspectorStyleSheetBase*&);
  String detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

  std::unique_ptr<protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
  std::unique_ptr<protocol::CSS::RuleUsage> buildObjectForRuleUsage(
      CSSStyleRule*,
      bool);
  std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>>
  buildArrayForMatchedRuleList(CSSRuleList*, Element*, PseudoId);
  std::unique_ptr<protocol::CSS::CSSStyle> buildObjectForAttributesStyle(
      Element*);

  // InspectorDOMAgent::DOMListener implementation
  void didAddDocument(Document*) override;
  void didRemoveDocument(Document*) override;
  void didRemoveDOMNode(Node*) override;
  void didModifyDOMAttr(Element*) override;

  // InspectorStyleSheet::Listener implementation
  void styleSheetChanged(InspectorStyleSheetBase*) override;

  void resetPseudoStates();

  struct VectorStringHashTraits;
  using ComputedStylesMap = WTF::HashMap<Vector<String>,
                                         int,
                                         VectorStringHashTraits,
                                         VectorStringHashTraits>;

  void visitLayoutTreeNodes(
      Node*,
      protocol::Array<protocol::CSS::LayoutTreeNode>& layoutTreeNodes,
      const Vector<std::pair<String, CSSPropertyID>>& cssPropertyWhitelist,
      ComputedStylesMap& styleToIndexMap,
      protocol::Array<protocol::CSS::ComputedStyle>& computedStyles);

  // A non-zero index corresponds to a style in |computedStyles|, -1 means an
  // empty style.
  int getStyleIndexForNode(
      Node*,
      const Vector<std::pair<String, CSSPropertyID>>& cssPropertyWhitelist,
      ComputedStylesMap& styleToIndexMap,
      protocol::Array<protocol::CSS::ComputedStyle>& computedStyles);

  Member<InspectorDOMAgent> m_domAgent;
  Member<InspectedFrames> m_inspectedFrames;
  Member<InspectorNetworkAgent> m_networkAgent;
  Member<InspectorResourceContentLoader> m_resourceContentLoader;
  Member<InspectorResourceContainer> m_resourceContainer;

  IdToInspectorStyleSheet m_idToInspectorStyleSheet;
  IdToInspectorStyleSheetForInlineStyle m_idToInspectorStyleSheetForInlineStyle;
  HeapHashMap<Member<CSSStyleSheet>, Member<InspectorStyleSheet>>
      m_cssStyleSheetToInspectorStyleSheet;
  typedef HeapHashMap<Member<Document>,
                      Member<HeapHashSet<Member<CSSStyleSheet>>>>
      DocumentStyleSheets;
  DocumentStyleSheets m_documentToCSSStyleSheets;
  HeapHashSet<Member<Document>> m_invalidatedDocuments;

  NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
  NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;

  Member<StyleRuleUsageTracker> m_tracker;

  Member<CSSStyleSheet> m_inspectorUserAgentStyleSheet;

  int m_resourceContentLoaderClientId;

  friend class InspectorResourceContentLoaderCallback;
  friend class StyleSheetBinder;
};

}  // namespace blink

#endif  // !defined(InspectorCSSAgent_h)
