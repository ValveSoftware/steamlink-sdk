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

#include "core/inspector/InspectorCSSAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSVariableData.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaValues.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleRuleUsageTracker.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Node.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorResourceContainer.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutObjectInlines.h"
#include "core/layout/LayoutText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/style/StyleGeneratedImage.h"
#include "core/style/StyleImage.h"
#include "core/svg/SVGElement.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/text/TextRun.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringConcatenate.h"

namespace {

int s_frontendOperationCounter = 0;

class FrontendOperationScope {
 public:
  FrontendOperationScope() { ++s_frontendOperationCounter; }
  ~FrontendOperationScope() { --s_frontendOperationCounter; }
};

using namespace blink;

String createShorthandValue(Document* document,
                            const String& shorthand,
                            const String& oldText,
                            const String& longhand,
                            const String& newValue) {
  StyleSheetContents* styleSheetContents =
      StyleSheetContents::create(strictCSSParserContext());
  String text = " div { " + shorthand + ": " + oldText + "; }";
  CSSParser::parseSheet(CSSParserContext(*document, nullptr),
                        styleSheetContents, text);

  CSSStyleSheet* styleSheet = CSSStyleSheet::create(styleSheetContents);
  CSSStyleRule* rule = toCSSStyleRule(styleSheet->item(0));
  CSSStyleDeclaration* style = rule->style();
  TrackExceptionState exceptionState;
  style->setProperty(longhand, newValue, style->getPropertyPriority(longhand),
                     exceptionState);
  return style->getPropertyValue(shorthand);
}

HeapVector<Member<CSSStyleRule>> filterDuplicateRules(CSSRuleList* ruleList) {
  HeapVector<Member<CSSStyleRule>> uniqRules;
  HeapHashSet<Member<CSSRule>> uniqRulesSet;
  for (unsigned i = ruleList ? ruleList->length() : 0; i > 0; --i) {
    CSSRule* rule = ruleList->item(i - 1);
    if (!rule || rule->type() != CSSRule::kStyleRule ||
        uniqRulesSet.contains(rule))
      continue;
    uniqRulesSet.add(rule);
    uniqRules.append(toCSSStyleRule(rule));
  }
  uniqRules.reverse();
  return uniqRules;
}

// Get the elements which overlap the given rectangle.
HeapVector<Member<Element>> elementsFromRect(LayoutRect rect,
                                             Document& document) {
  HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active |
                         HitTestRequest::ListBased |
                         HitTestRequest::PenetratingList |
                         HitTestRequest::IgnoreClipping);

  LayoutPoint center = rect.center();
  unsigned leftPadding, rightPadding, topPadding, bottomPadding;
  leftPadding = rightPadding = (rect.width() / 2).toUnsigned();
  topPadding = bottomPadding = (rect.height() / 2).toUnsigned();
  HitTestResult result(request, center, topPadding, rightPadding, bottomPadding,
                       leftPadding);
  document.frame()->contentLayoutItem().hitTest(result);
  return document.elementsFromHitTestResult(result);
}

// Blends the colors from the given gradient with the existing colors.
void blendWithColorsFromGradient(CSSGradientValue* gradient,
                                 Vector<Color>& colors,
                                 bool& foundNonTransparentColor,
                                 bool& foundOpaqueColor,
                                 const LayoutObject& layoutObject) {
  Vector<Color> stopColors;
  gradient->getStopColors(stopColors, layoutObject);

  if (colors.isEmpty()) {
    colors.appendRange(stopColors.begin(), stopColors.end());
  } else {
    if (colors.size() > 1) {
      // Gradient on gradient is too complicated, bail out
      colors.clear();
      return;
    }

    Color existingColor = colors.first();
    colors.clear();
    for (auto stopColor : stopColors) {
      foundNonTransparentColor =
          foundNonTransparentColor || (stopColor.alpha() != 0);
      colors.append(existingColor.blend(stopColor));
    }
  }
  foundOpaqueColor =
      foundOpaqueColor || gradient->knownToBeOpaque(layoutObject);
}

// Gets the colors from an image style, if one exists and it is a gradient.
void addColorsFromImageStyle(const ComputedStyle& style,
                             Vector<Color>& colors,
                             bool& foundOpaqueColor,
                             bool& foundNonTransparentColor,
                             const LayoutObject& layoutObject) {
  const FillLayer& backgroundLayers = style.backgroundLayers();
  if (!backgroundLayers.hasImage())
    return;

  StyleImage* styleImage = backgroundLayers.image();
  // hasImage() does not always indicate that this is non-null
  if (!styleImage)
    return;

  if (!styleImage->isGeneratedImage()) {
    // Make no assertions about the colors in non-generated images
    colors.clear();
    foundOpaqueColor = false;
    return;
  }

  StyleGeneratedImage* genImage = toStyleGeneratedImage(styleImage);
  CSSValue* imageCSS = genImage->cssValue();
  if (imageCSS->isGradientValue()) {
    CSSGradientValue* gradient = toCSSGradientValue(imageCSS);
    blendWithColorsFromGradient(gradient, colors, foundNonTransparentColor,
                                foundOpaqueColor, layoutObject);
  }
  return;
}

// Get the background colors behind the given rect in the given document, by
// walking up all the elements returned by a hit test (but not going beyond
// |topElement|) covering the area of the rect, and blending their background
// colors.
bool getColorsFromRect(LayoutRect rect,
                       Document& document,
                       Element* topElement,
                       Vector<Color>& colors) {
  HeapVector<Member<Element>> elementsUnderRect =
      elementsFromRect(rect, document);

  bool foundOpaqueColor = false;
  bool foundTopElement = false;

  for (auto e = elementsUnderRect.rbegin();
       !foundTopElement && e != elementsUnderRect.rend(); ++e) {
    const Element* element = *e;
    if (element == topElement)
      foundTopElement = true;

    const LayoutObject* layoutObject = element->layoutObject();
    if (!layoutObject)
      continue;

    if (isHTMLCanvasElement(element) || isHTMLEmbedElement(element) ||
        isHTMLImageElement(element) || isHTMLObjectElement(element) ||
        isHTMLPictureElement(element) || element->isSVGElement() ||
        isHTMLVideoElement(element)) {
      colors.clear();
      foundOpaqueColor = false;
      continue;
    }

    const ComputedStyle* style = layoutObject->style();
    if (!style)
      continue;

    Color backgroundColor =
        style->visitedDependentColor(CSSPropertyBackgroundColor);
    bool foundNonTransparentColor = false;
    if (backgroundColor.alpha() != 0) {
      foundNonTransparentColor = true;
      if (colors.isEmpty()) {
        if (!backgroundColor.hasAlpha())
          foundOpaqueColor = true;
        colors.append(backgroundColor);
      } else {
        if (!backgroundColor.hasAlpha()) {
          colors.clear();
          colors.append(backgroundColor);
          foundOpaqueColor = true;
        } else {
          for (size_t i = 0; i < colors.size(); i++)
            colors[i] = colors[i].blend(backgroundColor);
          foundOpaqueColor = foundOpaqueColor || backgroundColor.hasAlpha();
        }
      }
    }

    addColorsFromImageStyle(*style, colors, foundOpaqueColor,
                            foundNonTransparentColor, *layoutObject);

    bool contains = foundTopElement || element->boundingBox().contains(rect);
    if (!contains && foundNonTransparentColor) {
      // Only return colors if some opaque element covers up this one.
      colors.clear();
      foundOpaqueColor = false;
    }
  }
  return foundOpaqueColor;
}

std::unique_ptr<protocol::DOM::Rect> buildRectForFloatRect(
    const FloatRect& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.x())
      .setY(rect.y())
      .setWidth(rect.width())
      .setHeight(rect.height())
      .build();
}

}  // namespace

namespace CSSAgentState {
static const char cssAgentEnabled[] = "cssAgentEnabled";
static const char ruleRecordingEnabled[] = "ruleRecordingEnabled";
}

typedef blink::protocol::CSS::Backend::EnableCallback EnableCallback;

namespace blink {

enum ForcePseudoClassFlags {
  PseudoNone = 0,
  PseudoHover = 1 << 0,
  PseudoFocus = 1 << 1,
  PseudoActive = 1 << 2,
  PseudoVisited = 1 << 3
};

static unsigned computePseudoClassMask(
    std::unique_ptr<protocol::Array<String>> pseudoClassArray) {
  DEFINE_STATIC_LOCAL(String, active, ("active"));
  DEFINE_STATIC_LOCAL(String, hover, ("hover"));
  DEFINE_STATIC_LOCAL(String, focus, ("focus"));
  DEFINE_STATIC_LOCAL(String, visited, ("visited"));
  if (!pseudoClassArray || !pseudoClassArray->length())
    return PseudoNone;

  unsigned result = PseudoNone;
  for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
    String pseudoClass = pseudoClassArray->get(i);
    if (pseudoClass == active)
      result |= PseudoActive;
    else if (pseudoClass == hover)
      result |= PseudoHover;
    else if (pseudoClass == focus)
      result |= PseudoFocus;
    else if (pseudoClass == visited)
      result |= PseudoVisited;
  }

  return result;
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
  WTF_MAKE_NONCOPYABLE(StyleSheetAction);

 public:
  StyleSheetAction(const String& name) : InspectorHistory::Action(name) {}

  virtual std::unique_ptr<protocol::CSS::CSSStyle> takeSerializedStyle() {
    return nullptr;
  }
};

struct InspectorCSSAgent::VectorStringHashTraits
    : public WTF::GenericHashTraits<Vector<String>> {
  static unsigned hash(const Vector<String>& vec) {
    unsigned h = DefaultHash<size_t>::Hash::hash(vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
      h = WTF::hashInts(h, DefaultHash<String>::Hash::hash(vec[i]));
    }
    return h;
  }

  static bool equal(const Vector<String>& a, const Vector<String>& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); i++) {
      if (a[i] != b[i])
        return false;
    }
    return true;
  }

  static void constructDeletedValue(Vector<String>& vec, bool) {
    vec.clear();
    vec.append(String(WTF::HashTableDeletedValue));
  }

  static bool isDeletedValue(const Vector<String>& vec) {
    return !vec.isEmpty() && vec[0].isHashTableDeletedValue();
  }

  static bool isEmptyValue(const Vector<String>& vec) { return vec.isEmpty(); }

  static const bool emptyValueIsZero = false;
  static const bool safeToCompareToEmptyOrDeleted = false;
  static const bool hasIsEmptyValueFunction = true;
};

class InspectorCSSAgent::SetStyleSheetTextAction final
    : public InspectorCSSAgent::StyleSheetAction {
  WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);

 public:
  SetStyleSheetTextAction(InspectorStyleSheetBase* styleSheet,
                          const String& text)
      : InspectorCSSAgent::StyleSheetAction("SetStyleSheetText"),
        m_styleSheet(styleSheet),
        m_text(text) {}

  bool perform(ExceptionState& exceptionState) override {
    if (!m_styleSheet->getText(&m_oldText))
      return false;
    return redo(exceptionState);
  }

  bool undo(ExceptionState& exceptionState) override {
    return m_styleSheet->setText(m_oldText, exceptionState);
  }

  bool redo(ExceptionState& exceptionState) override {
    return m_styleSheet->setText(m_text, exceptionState);
  }

  String mergeId() override {
    return String::format("SetStyleSheetText %s",
                          m_styleSheet->id().utf8().data());
  }

  void merge(Action* action) override {
    ASSERT(action->mergeId() == mergeId());

    SetStyleSheetTextAction* other =
        static_cast<SetStyleSheetTextAction*>(action);
    m_text = other->m_text;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_styleSheet);
    InspectorCSSAgent::StyleSheetAction::trace(visitor);
  }

 private:
  Member<InspectorStyleSheetBase> m_styleSheet;
  String m_text;
  String m_oldText;
};

class InspectorCSSAgent::ModifyRuleAction final
    : public InspectorCSSAgent::StyleSheetAction {
  WTF_MAKE_NONCOPYABLE(ModifyRuleAction);

 public:
  enum Type { SetRuleSelector, SetStyleText, SetMediaRuleText, SetKeyframeKey };

  ModifyRuleAction(Type type,
                   InspectorStyleSheet* styleSheet,
                   const SourceRange& range,
                   const String& text)
      : InspectorCSSAgent::StyleSheetAction("ModifyRuleAction"),
        m_styleSheet(styleSheet),
        m_type(type),
        m_newText(text),
        m_oldRange(range),
        m_cssRule(nullptr) {}

  bool perform(ExceptionState& exceptionState) override {
    return redo(exceptionState);
  }

  bool undo(ExceptionState& exceptionState) override {
    switch (m_type) {
      case SetRuleSelector:
        return m_styleSheet->setRuleSelector(m_newRange, m_oldText, nullptr,
                                             nullptr, exceptionState);
      case SetStyleText:
        return m_styleSheet->setStyleText(m_newRange, m_oldText, nullptr,
                                          nullptr, exceptionState);
      case SetMediaRuleText:
        return m_styleSheet->setMediaRuleText(m_newRange, m_oldText, nullptr,
                                              nullptr, exceptionState);
      case SetKeyframeKey:
        return m_styleSheet->setKeyframeKey(m_newRange, m_oldText, nullptr,
                                            nullptr, exceptionState);
      default:
        ASSERT_NOT_REACHED();
    }
    return false;
  }

  bool redo(ExceptionState& exceptionState) override {
    switch (m_type) {
      case SetRuleSelector:
        m_cssRule = m_styleSheet->setRuleSelector(
            m_oldRange, m_newText, &m_newRange, &m_oldText, exceptionState);
        break;
      case SetStyleText:
        m_cssRule = m_styleSheet->setStyleText(
            m_oldRange, m_newText, &m_newRange, &m_oldText, exceptionState);
        break;
      case SetMediaRuleText:
        m_cssRule = m_styleSheet->setMediaRuleText(
            m_oldRange, m_newText, &m_newRange, &m_oldText, exceptionState);
        break;
      case SetKeyframeKey:
        m_cssRule = m_styleSheet->setKeyframeKey(
            m_oldRange, m_newText, &m_newRange, &m_oldText, exceptionState);
        break;
      default:
        ASSERT_NOT_REACHED();
    }
    return m_cssRule;
  }

  CSSRule* takeRule() {
    CSSRule* result = m_cssRule;
    m_cssRule = nullptr;
    return result;
  }

  std::unique_ptr<protocol::CSS::CSSStyle> takeSerializedStyle() override {
    if (m_type != SetStyleText)
      return nullptr;
    CSSRule* rule = takeRule();
    if (rule->type() == CSSRule::kStyleRule)
      return m_styleSheet->buildObjectForStyle(toCSSStyleRule(rule)->style());
    if (rule->type() == CSSRule::kKeyframeRule)
      return m_styleSheet->buildObjectForStyle(
          toCSSKeyframeRule(rule)->style());
    return nullptr;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_styleSheet);
    visitor->trace(m_cssRule);
    InspectorCSSAgent::StyleSheetAction::trace(visitor);
  }

  String mergeId() override {
    return String::format("ModifyRuleAction:%d %s:%d", m_type,
                          m_styleSheet->id().utf8().data(), m_oldRange.start);
  }

  bool isNoop() override { return m_oldText == m_newText; }

  void merge(Action* action) override {
    ASSERT(action->mergeId() == mergeId());

    ModifyRuleAction* other = static_cast<ModifyRuleAction*>(action);
    m_newText = other->m_newText;
    m_newRange = other->m_newRange;
  }

 private:
  Member<InspectorStyleSheet> m_styleSheet;
  Type m_type;
  String m_oldText;
  String m_newText;
  SourceRange m_oldRange;
  SourceRange m_newRange;
  Member<CSSRule> m_cssRule;
};

class InspectorCSSAgent::SetElementStyleAction final
    : public InspectorCSSAgent::StyleSheetAction {
  WTF_MAKE_NONCOPYABLE(SetElementStyleAction);

 public:
  SetElementStyleAction(InspectorStyleSheetForInlineStyle* styleSheet,
                        const String& text)
      : InspectorCSSAgent::StyleSheetAction("SetElementStyleAction"),
        m_styleSheet(styleSheet),
        m_text(text) {}

  bool perform(ExceptionState& exceptionState) override {
    return redo(exceptionState);
  }

  bool undo(ExceptionState& exceptionState) override {
    return m_styleSheet->setText(m_oldText, exceptionState);
  }

  bool redo(ExceptionState& exceptionState) override {
    if (!m_styleSheet->getText(&m_oldText))
      return false;
    return m_styleSheet->setText(m_text, exceptionState);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_styleSheet);
    InspectorCSSAgent::StyleSheetAction::trace(visitor);
  }

  String mergeId() override {
    return String::format("SetElementStyleAction:%s",
                          m_styleSheet->id().utf8().data());
  }

  std::unique_ptr<protocol::CSS::CSSStyle> takeSerializedStyle() override {
    return m_styleSheet->buildObjectForStyle(m_styleSheet->inlineStyle());
  }

  void merge(Action* action) override {
    ASSERT(action->mergeId() == mergeId());

    SetElementStyleAction* other = static_cast<SetElementStyleAction*>(action);
    m_text = other->m_text;
  }

 private:
  Member<InspectorStyleSheetForInlineStyle> m_styleSheet;
  String m_text;
  String m_oldText;
};

class InspectorCSSAgent::AddRuleAction final
    : public InspectorCSSAgent::StyleSheetAction {
  WTF_MAKE_NONCOPYABLE(AddRuleAction);

 public:
  AddRuleAction(InspectorStyleSheet* styleSheet,
                const String& ruleText,
                const SourceRange& location)
      : InspectorCSSAgent::StyleSheetAction("AddRule"),
        m_styleSheet(styleSheet),
        m_ruleText(ruleText),
        m_location(location) {}

  bool perform(ExceptionState& exceptionState) override {
    return redo(exceptionState);
  }

  bool undo(ExceptionState& exceptionState) override {
    return m_styleSheet->deleteRule(m_addedRange, exceptionState);
  }

  bool redo(ExceptionState& exceptionState) override {
    m_cssRule = m_styleSheet->addRule(m_ruleText, m_location, &m_addedRange,
                                      exceptionState);
    if (exceptionState.hadException())
      return false;
    return true;
  }

  CSSStyleRule* takeRule() {
    CSSStyleRule* result = m_cssRule;
    m_cssRule = nullptr;
    return result;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_styleSheet);
    visitor->trace(m_cssRule);
    InspectorCSSAgent::StyleSheetAction::trace(visitor);
  }

 private:
  Member<InspectorStyleSheet> m_styleSheet;
  Member<CSSStyleRule> m_cssRule;
  String m_ruleText;
  String m_oldText;
  SourceRange m_location;
  SourceRange m_addedRange;
};

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule) {
  if (!rule || rule->type() != CSSRule::kStyleRule)
    return nullptr;
  return toCSSStyleRule(rule);
}

// static
CSSMediaRule* InspectorCSSAgent::asCSSMediaRule(CSSRule* rule) {
  if (!rule || rule->type() != CSSRule::kMediaRule)
    return nullptr;
  return toCSSMediaRule(rule);
}

InspectorCSSAgent::InspectorCSSAgent(
    InspectorDOMAgent* domAgent,
    InspectedFrames* inspectedFrames,
    InspectorNetworkAgent* networkAgent,
    InspectorResourceContentLoader* resourceContentLoader,
    InspectorResourceContainer* resourceContainer)
    : m_domAgent(domAgent),
      m_inspectedFrames(inspectedFrames),
      m_networkAgent(networkAgent),
      m_resourceContentLoader(resourceContentLoader),
      m_resourceContainer(resourceContainer),
      m_resourceContentLoaderClientId(resourceContentLoader->createClientId()) {
}

InspectorCSSAgent::~InspectorCSSAgent() {}

void InspectorCSSAgent::restore() {
  if (m_state->booleanProperty(CSSAgentState::cssAgentEnabled, false))
    wasEnabled();
  if (m_state->booleanProperty(CSSAgentState::ruleRecordingEnabled, false))
    setUsageTrackerStatus(true);
}

void InspectorCSSAgent::flushPendingProtocolNotifications() {
  if (!m_invalidatedDocuments.size())
    return;
  HeapHashSet<Member<Document>> invalidatedDocuments;
  m_invalidatedDocuments.swap(invalidatedDocuments);
  for (Document* document : invalidatedDocuments)
    updateActiveStyleSheets(document, ExistingFrontendRefresh);
}

void InspectorCSSAgent::reset() {
  m_idToInspectorStyleSheet.clear();
  m_idToInspectorStyleSheetForInlineStyle.clear();
  m_cssStyleSheetToInspectorStyleSheet.clear();
  m_documentToCSSStyleSheets.clear();
  m_invalidatedDocuments.clear();
  m_nodeToInspectorStyleSheet.clear();
  resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData() {
  resetPseudoStates();
}

void InspectorCSSAgent::enable(std::unique_ptr<EnableCallback> prpCallback) {
  if (!m_domAgent->enabled()) {
    prpCallback->sendFailure(
        Response::Error("DOM agent needs to be enabled first."));
    return;
  }
  m_state->setBoolean(CSSAgentState::cssAgentEnabled, true);
  m_resourceContentLoader->ensureResourcesContentLoaded(
      m_resourceContentLoaderClientId,
      WTF::bind(&InspectorCSSAgent::resourceContentLoaded, wrapPersistent(this),
                passed(std::move(prpCallback))));
}

void InspectorCSSAgent::resourceContentLoaded(
    std::unique_ptr<EnableCallback> callback) {
  wasEnabled();
  callback->sendSuccess();
}

void InspectorCSSAgent::wasEnabled() {
  if (!m_state->booleanProperty(CSSAgentState::cssAgentEnabled, false)) {
    // We were disabled while fetching resources.
    return;
  }

  m_instrumentingAgents->addInspectorCSSAgent(this);
  m_domAgent->setDOMListener(this);
  HeapVector<Member<Document>> documents = m_domAgent->documents();
  for (Document* document : documents)
    updateActiveStyleSheets(document, InitialFrontendLoad);
}

Response InspectorCSSAgent::disable() {
  reset();
  m_domAgent->setDOMListener(nullptr);
  m_instrumentingAgents->removeInspectorCSSAgent(this);
  m_state->setBoolean(CSSAgentState::cssAgentEnabled, false);
  m_resourceContentLoader->cancel(m_resourceContentLoaderClientId);
  m_state->setBoolean(CSSAgentState::ruleRecordingEnabled, false);
  setUsageTrackerStatus(false);
  return Response::OK();
}

void InspectorCSSAgent::didCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == m_inspectedFrames->root())
    reset();
}

void InspectorCSSAgent::mediaQueryResultChanged() {
  flushPendingProtocolNotifications();
  frontend()->mediaQueryResultChanged();
}

void InspectorCSSAgent::fontsUpdated() {
  flushPendingProtocolNotifications();
  frontend()->fontsUpdated();
}

void InspectorCSSAgent::activeStyleSheetsUpdated(Document* document) {
  m_invalidatedDocuments.add(document);
}

void InspectorCSSAgent::updateActiveStyleSheets(
    Document* document,
    StyleSheetsUpdateType styleSheetsUpdateType) {
  HeapVector<Member<CSSStyleSheet>> newSheetsVector;
  InspectorCSSAgent::collectAllDocumentStyleSheets(document, newSheetsVector);
  setActiveStyleSheets(document, newSheetsVector, styleSheetsUpdateType);
}

void InspectorCSSAgent::setActiveStyleSheets(
    Document* document,
    const HeapVector<Member<CSSStyleSheet>>& allSheetsVector,
    StyleSheetsUpdateType styleSheetsUpdateType) {
  bool isInitialFrontendLoad = styleSheetsUpdateType == InitialFrontendLoad;

  HeapHashSet<Member<CSSStyleSheet>>* documentCSSStyleSheets =
      m_documentToCSSStyleSheets.get(document);
  if (!documentCSSStyleSheets) {
    documentCSSStyleSheets = new HeapHashSet<Member<CSSStyleSheet>>();
    m_documentToCSSStyleSheets.set(document, documentCSSStyleSheets);
  }

  HeapHashSet<Member<CSSStyleSheet>> removedSheets(*documentCSSStyleSheets);
  HeapVector<Member<CSSStyleSheet>> addedSheets;
  for (CSSStyleSheet* cssStyleSheet : allSheetsVector) {
    if (removedSheets.contains(cssStyleSheet)) {
      removedSheets.remove(cssStyleSheet);
      if (isInitialFrontendLoad)
        addedSheets.append(cssStyleSheet);
    } else {
      addedSheets.append(cssStyleSheet);
    }
  }

  for (CSSStyleSheet* cssStyleSheet : removedSheets) {
    InspectorStyleSheet* inspectorStyleSheet =
        m_cssStyleSheetToInspectorStyleSheet.get(cssStyleSheet);
    ASSERT(inspectorStyleSheet);

    documentCSSStyleSheets->remove(cssStyleSheet);
    if (m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
      String id = unbindStyleSheet(inspectorStyleSheet);
      if (frontend() && !isInitialFrontendLoad)
        frontend()->styleSheetRemoved(id);
    }
  }

  for (CSSStyleSheet* cssStyleSheet : addedSheets) {
    bool isNew = isInitialFrontendLoad ||
                 !m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet);
    if (isNew) {
      InspectorStyleSheet* newStyleSheet = bindStyleSheet(cssStyleSheet);
      documentCSSStyleSheets->add(cssStyleSheet);
      if (frontend())
        frontend()->styleSheetAdded(
            newStyleSheet->buildObjectForStyleSheetInfo());
    }
  }

  if (documentCSSStyleSheets->isEmpty())
    m_documentToCSSStyleSheets.remove(document);
}

void InspectorCSSAgent::documentDetached(Document* document) {
  m_invalidatedDocuments.remove(document);
  setActiveStyleSheets(document, HeapVector<Member<CSSStyleSheet>>(),
                       ExistingFrontendRefresh);
}

bool InspectorCSSAgent::forcePseudoState(Element* element,
                                         CSSSelector::PseudoType pseudoType) {
  if (m_nodeIdToForcedPseudoState.isEmpty())
    return false;

  int nodeId = m_domAgent->boundNodeId(element);
  if (!nodeId)
    return false;

  NodeIdToForcedPseudoState::iterator it =
      m_nodeIdToForcedPseudoState.find(nodeId);
  if (it == m_nodeIdToForcedPseudoState.end())
    return false;

  unsigned forcedPseudoState = it->value;
  switch (pseudoType) {
    case CSSSelector::PseudoActive:
      return forcedPseudoState & PseudoActive;
    case CSSSelector::PseudoFocus:
      return forcedPseudoState & PseudoFocus;
    case CSSSelector::PseudoHover:
      return forcedPseudoState & PseudoHover;
    case CSSSelector::PseudoVisited:
      return forcedPseudoState & PseudoVisited;
    default:
      return false;
  }
}

Response InspectorCSSAgent::getMediaQueries(
    std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>* medias) {
  *medias = protocol::Array<protocol::CSS::CSSMedia>::create();
  for (auto& style : m_idToInspectorStyleSheet) {
    InspectorStyleSheet* styleSheet = style.value;
    collectMediaQueriesFromStyleSheet(styleSheet->pageStyleSheet(),
                                      medias->get());
    const CSSRuleVector& flatRules = styleSheet->flatRules();
    for (unsigned i = 0; i < flatRules.size(); ++i) {
      CSSRule* rule = flatRules.at(i).get();
      if (rule->type() == CSSRule::kMediaRule ||
          rule->type() == CSSRule::kImportRule)
        collectMediaQueriesFromRule(rule, medias->get());
    }
  }
  return Response::OK();
}

Response InspectorCSSAgent::getMatchedStylesForNode(
    int nodeId,
    Maybe<protocol::CSS::CSSStyle>* inlineStyle,
    Maybe<protocol::CSS::CSSStyle>* attributesStyle,
    Maybe<protocol::Array<protocol::CSS::RuleMatch>>* matchedCSSRules,
    Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>>*
        pseudoIdMatches,
    Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>>*
        inheritedEntries,
    Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>>*
        cssKeyframesRules) {
  Element* element = nullptr;
  Response response = m_domAgent->assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  Element* originalElement = element;
  PseudoId elementPseudoId = element->getPseudoId();
  if (elementPseudoId) {
    element = element->parentOrShadowHostElement();
    if (!element)
      return Response::Error("Pseudo element has no parent");
  }

  Document* ownerDocument = element->ownerDocument();
  // A non-active document has no styles.
  if (!ownerDocument->isActive())
    return Response::Error("Document is not active");

  // FIXME: It's really gross for the inspector to reach in and access
  // StyleResolver directly here. We need to provide the Inspector better APIs
  // to get this information without grabbing at internal style classes!

  // Matched rules.
  StyleResolver& styleResolver = ownerDocument->ensureStyleResolver();

  element->updateDistribution();
  CSSRuleList* matchedRules = styleResolver.pseudoCSSRulesForElement(
      element, elementPseudoId, StyleResolver::AllCSSRules);
  *matchedCSSRules =
      buildArrayForMatchedRuleList(matchedRules, originalElement, PseudoIdNone);

  // Pseudo elements.
  if (elementPseudoId)
    return Response::OK();

  InspectorStyleSheetForInlineStyle* inlineStyleSheet =
      asInspectorStyleSheet(element);
  if (inlineStyleSheet) {
    *inlineStyle = inlineStyleSheet->buildObjectForStyle(element->style());
    *attributesStyle = buildObjectForAttributesStyle(element);
  }

  *pseudoIdMatches =
      protocol::Array<protocol::CSS::PseudoElementMatches>::create();
  for (PseudoId pseudoId = FirstPublicPseudoId;
       pseudoId < AfterLastInternalPseudoId;
       pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
    CSSRuleList* matchedRules = styleResolver.pseudoCSSRulesForElement(
        element, pseudoId, StyleResolver::AllCSSRules);
    protocol::DOM::PseudoType pseudoType;
    if (matchedRules && matchedRules->length() &&
        m_domAgent->getPseudoElementType(pseudoId, &pseudoType)) {
      pseudoIdMatches->fromJust()->addItem(
          protocol::CSS::PseudoElementMatches::create()
              .setPseudoType(pseudoType)
              .setMatches(
                  buildArrayForMatchedRuleList(matchedRules, element, pseudoId))
              .build());
    }
  }

  // Inherited styles.
  *inheritedEntries =
      protocol::Array<protocol::CSS::InheritedStyleEntry>::create();
  Element* parentElement = element->parentOrShadowHostElement();
  while (parentElement) {
    StyleResolver& parentStyleResolver =
        parentElement->ownerDocument()->ensureStyleResolver();
    CSSRuleList* parentMatchedRules = parentStyleResolver.cssRulesForElement(
        parentElement, StyleResolver::AllCSSRules);
    std::unique_ptr<protocol::CSS::InheritedStyleEntry> entry =
        protocol::CSS::InheritedStyleEntry::create()
            .setMatchedCSSRules(buildArrayForMatchedRuleList(
                parentMatchedRules, parentElement, PseudoIdNone))
            .build();
    if (parentElement->style() && parentElement->style()->length()) {
      InspectorStyleSheetForInlineStyle* styleSheet =
          asInspectorStyleSheet(parentElement);
      if (styleSheet)
        entry->setInlineStyle(
            styleSheet->buildObjectForStyle(styleSheet->inlineStyle()));
    }

    inheritedEntries->fromJust()->addItem(std::move(entry));
    parentElement = parentElement->parentOrShadowHostElement();
  }

  *cssKeyframesRules = animationsForNode(element);
  return Response::OK();
}

template <class CSSRuleCollection>
static CSSKeyframesRule* findKeyframesRule(CSSRuleCollection* cssRules,
                                           StyleRuleKeyframes* keyframesRule) {
  CSSKeyframesRule* result = 0;
  for (unsigned j = 0; cssRules && j < cssRules->length() && !result; ++j) {
    CSSRule* cssRule = cssRules->item(j);
    if (cssRule->type() == CSSRule::kKeyframesRule) {
      CSSKeyframesRule* cssStyleRule = toCSSKeyframesRule(cssRule);
      if (cssStyleRule->keyframes() == keyframesRule)
        result = cssStyleRule;
    } else if (cssRule->type() == CSSRule::kImportRule) {
      CSSImportRule* cssImportRule = toCSSImportRule(cssRule);
      result = findKeyframesRule(cssImportRule->styleSheet(), keyframesRule);
    } else {
      result = findKeyframesRule(cssRule->cssRules(), keyframesRule);
    }
  }
  return result;
}

std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>>
InspectorCSSAgent::animationsForNode(Element* element) {
  std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>>
      cssKeyframesRules =
          protocol::Array<protocol::CSS::CSSKeyframesRule>::create();
  Document* ownerDocument = element->ownerDocument();

  StyleResolver& styleResolver = ownerDocument->ensureStyleResolver();
  RefPtr<ComputedStyle> style = styleResolver.styleForElement(element);
  if (!style)
    return cssKeyframesRules;
  const CSSAnimationData* animationData = style->animations();
  for (size_t i = 0; animationData && i < animationData->nameList().size();
       ++i) {
    AtomicString animationName(animationData->nameList()[i]);
    if (animationName == CSSAnimationData::initialName())
      continue;
    StyleRuleKeyframes* keyframesRule =
        styleResolver.findKeyframesRule(element, animationName);
    if (!keyframesRule)
      continue;

    // Find CSSOM wrapper.
    CSSKeyframesRule* cssKeyframesRule = nullptr;
    for (CSSStyleSheet* styleSheet :
         *m_documentToCSSStyleSheets.get(ownerDocument)) {
      cssKeyframesRule = findKeyframesRule(styleSheet, keyframesRule);
      if (cssKeyframesRule)
        break;
    }
    if (!cssKeyframesRule)
      continue;

    std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframeRule>> keyframes =
        protocol::Array<protocol::CSS::CSSKeyframeRule>::create();
    for (unsigned j = 0; j < cssKeyframesRule->length(); ++j) {
      InspectorStyleSheet* inspectorStyleSheet =
          bindStyleSheet(cssKeyframesRule->parentStyleSheet());
      keyframes->addItem(inspectorStyleSheet->buildObjectForKeyframeRule(
          cssKeyframesRule->item(j)));
    }

    InspectorStyleSheet* inspectorStyleSheet =
        bindStyleSheet(cssKeyframesRule->parentStyleSheet());
    CSSRuleSourceData* sourceData =
        inspectorStyleSheet->sourceDataForRule(cssKeyframesRule);
    std::unique_ptr<protocol::CSS::Value> name =
        protocol::CSS::Value::create()
            .setText(cssKeyframesRule->name())
            .build();
    if (sourceData)
      name->setRange(inspectorStyleSheet->buildSourceRangeObject(
          sourceData->ruleHeaderRange));
    cssKeyframesRules->addItem(protocol::CSS::CSSKeyframesRule::create()
                                   .setAnimationName(std::move(name))
                                   .setKeyframes(std::move(keyframes))
                                   .build());
  }
  return cssKeyframesRules;
}

Response InspectorCSSAgent::getInlineStylesForNode(
    int nodeId,
    Maybe<protocol::CSS::CSSStyle>* inlineStyle,
    Maybe<protocol::CSS::CSSStyle>* attributesStyle) {
  Element* element = nullptr;
  Response response = m_domAgent->assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  InspectorStyleSheetForInlineStyle* styleSheet =
      asInspectorStyleSheet(element);
  if (!styleSheet)
    return Response::Error("Element is not a style sheet");

  *inlineStyle = styleSheet->buildObjectForStyle(element->style());
  *attributesStyle = buildObjectForAttributesStyle(element);
  return Response::OK();
}

Response InspectorCSSAgent::getComputedStyleForNode(
    int nodeId,
    std::unique_ptr<protocol::Array<protocol::CSS::CSSComputedStyleProperty>>*
        style) {
  Node* node = nullptr;
  Response response = m_domAgent->assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  CSSComputedStyleDeclaration* computedStyleInfo =
      CSSComputedStyleDeclaration::create(node, true);
  *style = protocol::Array<protocol::CSS::CSSComputedStyleProperty>::create();
  for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
    CSSPropertyID propertyId = static_cast<CSSPropertyID>(id);
    if (!CSSPropertyMetadata::isEnabledProperty(propertyId) ||
        isShorthandProperty(propertyId) ||
        CSSPropertyMetadata::isDescriptorOnly(propertyId))
      continue;
    (*style)->addItem(
        protocol::CSS::CSSComputedStyleProperty::create()
            .setName(getPropertyNameString(propertyId))
            .setValue(computedStyleInfo->getPropertyValue(propertyId))
            .build());
  }

  std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> variables =
      computedStyleInfo->getVariables();

  if (variables && !variables->isEmpty()) {
    for (const auto& it : *variables) {
      if (!it.value)
        continue;
      (*style)->addItem(protocol::CSS::CSSComputedStyleProperty::create()
                            .setName(it.key)
                            .setValue(it.value->tokenRange().serialize())
                            .build());
    }
  }
  return Response::OK();
}

void InspectorCSSAgent::collectPlatformFontsForLayoutObject(
    LayoutObject* layoutObject,
    HashCountedSet<std::pair<int, String>>* fontStats) {
  if (!layoutObject->isText())
    return;

  FontCachePurgePreventer preventer;
  LayoutText* layoutText = toLayoutText(layoutObject);
  for (InlineTextBox* box = layoutText->firstTextBox(); box;
       box = box->nextTextBox()) {
    const ComputedStyle& style = layoutText->styleRef(box->isFirstLineStyle());
    const Font& font = style.font();
    TextRun run = box->constructTextRunForInspector(style);
    TextRunPaintInfo paintInfo(run);
    GlyphBuffer glyphBuffer;
    font.buildGlyphBuffer(paintInfo, glyphBuffer);
    for (unsigned i = 0; i < glyphBuffer.size(); ++i) {
      const SimpleFontData* simpleFontData = glyphBuffer.fontDataAt(i);
      String familyName = simpleFontData->platformData().fontFamilyName();
      if (familyName.isNull())
        familyName = "";
      fontStats->add(
          std::make_pair(simpleFontData->isCustomFont() ? 1 : 0, familyName));
    }
  }
}

Response InspectorCSSAgent::getPlatformFontsForNode(
    int nodeId,
    std::unique_ptr<protocol::Array<protocol::CSS::PlatformFontUsage>>*
        platformFonts) {
  Node* node = nullptr;
  Response response = m_domAgent->assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  HashCountedSet<std::pair<int, String>> fontStats;
  LayoutObject* root = node->layoutObject();
  if (root) {
    collectPlatformFontsForLayoutObject(root, &fontStats);
    // Iterate upto two layers deep.
    for (LayoutObject* child = root->slowFirstChild(); child;
         child = child->nextSibling()) {
      collectPlatformFontsForLayoutObject(child, &fontStats);
      for (LayoutObject* child2 = child->slowFirstChild(); child2;
           child2 = child2->nextSibling())
        collectPlatformFontsForLayoutObject(child2, &fontStats);
    }
  }
  *platformFonts = protocol::Array<protocol::CSS::PlatformFontUsage>::create();
  for (auto& font : fontStats) {
    std::pair<int, String>& fontDescription = font.key;
    bool isCustomFont = fontDescription.first == 1;
    String fontName = fontDescription.second;
    (*platformFonts)
        ->addItem(protocol::CSS::PlatformFontUsage::create()
                      .setFamilyName(fontName)
                      .setIsCustomFont(isCustomFont)
                      .setGlyphCount(font.value)
                      .build());
  }
  return Response::OK();
}

Response InspectorCSSAgent::getStyleSheetText(const String& styleSheetId,
                                              String* result) {
  InspectorStyleSheetBase* inspectorStyleSheet = nullptr;
  Response response = assertStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;

  inspectorStyleSheet->getText(result);
  return Response::OK();
}

Response InspectorCSSAgent::collectClassNames(
    const String& styleSheetId,
    std::unique_ptr<protocol::Array<String>>* classNames) {
  InspectorStyleSheet* inspectorStyleSheet = nullptr;
  Response response =
      assertInspectorStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;
  *classNames = inspectorStyleSheet->collectClassNames();
  return Response::OK();
}

Response InspectorCSSAgent::setStyleSheetText(
    const String& styleSheetId,
    const String& text,
    protocol::Maybe<String>* sourceMapURL) {
  FrontendOperationScope scope;
  InspectorStyleSheetBase* inspectorStyleSheet = nullptr;
  Response response = assertStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  m_domAgent->history()->perform(
      new SetStyleSheetTextAction(inspectorStyleSheet, text), exceptionState);
  response = InspectorDOMAgent::toResponse(exceptionState);
  if (!response.isSuccess())
    return response;
  if (!inspectorStyleSheet->sourceMapURL().isEmpty())
    *sourceMapURL = inspectorStyleSheet->sourceMapURL();
  return Response::OK();
}

static Response jsonRangeToSourceRange(
    InspectorStyleSheetBase* inspectorStyleSheet,
    protocol::CSS::SourceRange* range,
    SourceRange* sourceRange) {
  if (range->getStartLine() < 0)
    return Response::Error("range.startLine must be a non-negative integer");
  if (range->getStartColumn() < 0)
    return Response::Error("range.startColumn must be a non-negative integer");
  if (range->getEndLine() < 0)
    return Response::Error("range.endLine must be a non-negative integer");
  if (range->getEndColumn() < 0)
    return Response::Error("range.endColumn must be a non-negative integer");

  unsigned startOffset = 0;
  unsigned endOffset = 0;
  bool success =
      inspectorStyleSheet->lineNumberAndColumnToOffset(
          range->getStartLine(), range->getStartColumn(), &startOffset) &&
      inspectorStyleSheet->lineNumberAndColumnToOffset(
          range->getEndLine(), range->getEndColumn(), &endOffset);
  if (!success)
    return Response::Error("Specified range is out of bounds");

  if (startOffset > endOffset)
    return Response::Error("Range start must not succeed its end");
  sourceRange->start = startOffset;
  sourceRange->end = endOffset;
  return Response::OK();
}

Response InspectorCSSAgent::setRuleSelector(
    const String& styleSheetId,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& selector,
    std::unique_ptr<protocol::CSS::SelectorList>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspectorStyleSheet = nullptr;
  Response response =
      assertInspectorStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;
  SourceRange selectorRange;
  response =
      jsonRangeToSourceRange(inspectorStyleSheet, range.get(), &selectorRange);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  ModifyRuleAction* action =
      new ModifyRuleAction(ModifyRuleAction::SetRuleSelector,
                           inspectorStyleSheet, selectorRange, selector);
  bool success = m_domAgent->history()->perform(action, exceptionState);
  if (success) {
    CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(action->takeRule());
    InspectorStyleSheet* inspectorStyleSheet = inspectorStyleSheetForRule(rule);
    if (!inspectorStyleSheet)
      return Response::Error("Failed to get inspector style sheet for rule.");
    *result = inspectorStyleSheet->buildObjectForSelectorList(rule);
  }
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorCSSAgent::setKeyframeKey(
    const String& styleSheetId,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& keyText,
    std::unique_ptr<protocol::CSS::Value>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspectorStyleSheet = nullptr;
  Response response =
      assertInspectorStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;
  SourceRange keyRange;
  response =
      jsonRangeToSourceRange(inspectorStyleSheet, range.get(), &keyRange);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  ModifyRuleAction* action = new ModifyRuleAction(
      ModifyRuleAction::SetKeyframeKey, inspectorStyleSheet, keyRange, keyText);
  bool success = m_domAgent->history()->perform(action, exceptionState);
  if (success) {
    CSSKeyframeRule* rule = toCSSKeyframeRule(action->takeRule());
    InspectorStyleSheet* inspectorStyleSheet =
        bindStyleSheet(rule->parentStyleSheet());
    if (!inspectorStyleSheet)
      return Response::Error("Failed to get inspector style sheet for rule.");

    CSSRuleSourceData* sourceData =
        inspectorStyleSheet->sourceDataForRule(rule);
    *result = protocol::CSS::Value::create()
                  .setText(rule->keyText())
                  .setRange(inspectorStyleSheet->buildSourceRangeObject(
                      sourceData->ruleHeaderRange))
                  .build();
  }
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorCSSAgent::multipleStyleTextsActions(
    std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>> edits,
    HeapVector<Member<StyleSheetAction>>* actions) {
  int n = edits->length();
  if (n == 0)
    return Response::Error("Edits should not be empty");

  for (int i = 0; i < n; ++i) {
    protocol::CSS::StyleDeclarationEdit* edit = edits->get(i);
    InspectorStyleSheetBase* inspectorStyleSheet = nullptr;
    Response response =
        assertStyleSheetForId(edit->getStyleSheetId(), inspectorStyleSheet);
    if (!response.isSuccess()) {
      return Response::Error(
          String::format("StyleSheet not found for edit #%d of %d", i + 1, n));
    }

    SourceRange range;
    response =
        jsonRangeToSourceRange(inspectorStyleSheet, edit->getRange(), &range);
    if (!response.isSuccess())
      return response;

    if (inspectorStyleSheet->isInlineStyle()) {
      InspectorStyleSheetForInlineStyle* inlineStyleSheet =
          static_cast<InspectorStyleSheetForInlineStyle*>(inspectorStyleSheet);
      SetElementStyleAction* action =
          new SetElementStyleAction(inlineStyleSheet, edit->getText());
      actions->append(action);
    } else {
      ModifyRuleAction* action = new ModifyRuleAction(
          ModifyRuleAction::SetStyleText,
          static_cast<InspectorStyleSheet*>(inspectorStyleSheet), range,
          edit->getText());
      actions->append(action);
    }
  }
  return Response::OK();
}

Response InspectorCSSAgent::setStyleTexts(
    std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>> edits,
    std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* result) {
  FrontendOperationScope scope;
  HeapVector<Member<StyleSheetAction>> actions;
  Response response = multipleStyleTextsActions(std::move(edits), &actions);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;

  int n = actions.size();
  std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>> serializedStyles =
      protocol::Array<protocol::CSS::CSSStyle>::create();
  for (int i = 0; i < n; ++i) {
    Member<StyleSheetAction> action = actions.at(i);
    bool success = action->perform(exceptionState);
    if (!success) {
      for (int j = i - 1; j >= 0; --j) {
        Member<StyleSheetAction> revert = actions.at(j);
        TrackExceptionState undoExceptionState;
        revert->undo(undoExceptionState);
        ASSERT(!undoExceptionState.hadException());
      }
      return Response::Error(
          String::format("Failed applying edit #%d: ", i) +
          InspectorDOMAgent::toResponse(exceptionState).errorMessage());
    }
    serializedStyles->addItem(action->takeSerializedStyle());
  }

  for (int i = 0; i < n; ++i) {
    Member<StyleSheetAction> action = actions.at(i);
    m_domAgent->history()->appendPerformedAction(action);
  }
  *result = std::move(serializedStyles);
  return Response::OK();
}

Response InspectorCSSAgent::setStyleText(
    InspectorStyleSheetBase* inspectorStyleSheet,
    const SourceRange& range,
    const String& text,
    CSSStyleDeclaration*& result) {
  TrackExceptionState exceptionState;
  if (inspectorStyleSheet->isInlineStyle()) {
    InspectorStyleSheetForInlineStyle* inlineStyleSheet =
        static_cast<InspectorStyleSheetForInlineStyle*>(inspectorStyleSheet);
    SetElementStyleAction* action =
        new SetElementStyleAction(inlineStyleSheet, text);
    bool success = m_domAgent->history()->perform(action, exceptionState);
    if (success) {
      result = inlineStyleSheet->inlineStyle();
      return Response::OK();
    }
  } else {
    ModifyRuleAction* action = new ModifyRuleAction(
        ModifyRuleAction::SetStyleText,
        static_cast<InspectorStyleSheet*>(inspectorStyleSheet), range, text);
    bool success = m_domAgent->history()->perform(action, exceptionState);
    if (success) {
      CSSRule* rule = action->takeRule();
      if (rule->type() == CSSRule::kStyleRule) {
        result = toCSSStyleRule(rule)->style();
        return Response::OK();
      }
      if (rule->type() == CSSRule::kKeyframeRule) {
        result = toCSSKeyframeRule(rule)->style();
        return Response::OK();
      }
    }
  }
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorCSSAgent::setMediaText(
    const String& styleSheetId,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& text,
    std::unique_ptr<protocol::CSS::CSSMedia>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspectorStyleSheet = nullptr;
  Response response =
      assertInspectorStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;
  SourceRange textRange;
  response =
      jsonRangeToSourceRange(inspectorStyleSheet, range.get(), &textRange);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  ModifyRuleAction* action = new ModifyRuleAction(
      ModifyRuleAction::SetMediaRuleText, inspectorStyleSheet, textRange, text);
  bool success = m_domAgent->history()->perform(action, exceptionState);
  if (success) {
    CSSMediaRule* rule = InspectorCSSAgent::asCSSMediaRule(action->takeRule());
    String sourceURL = rule->parentStyleSheet()->contents()->baseURL();
    if (sourceURL.isEmpty())
      sourceURL = InspectorDOMAgent::documentURLString(
          rule->parentStyleSheet()->ownerDocument());
    *result = buildMediaObject(rule->media(), MediaListSourceMediaRule,
                               sourceURL, rule->parentStyleSheet());
  }
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorCSSAgent::createStyleSheet(
    const String& frameId,
    protocol::CSS::StyleSheetId* outStyleSheetId) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  if (!frame)
    return Response::Error("Frame not found");

  Document* document = frame->document();
  if (!document)
    return Response::Error("Frame does not have a document");

  InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(document);
  if (!inspectorStyleSheet)
    return Response::Error("No target stylesheet found");

  updateActiveStyleSheets(document, ExistingFrontendRefresh);

  *outStyleSheetId = inspectorStyleSheet->id();
  return Response::OK();
}

Response InspectorCSSAgent::addRule(
    const String& styleSheetId,
    const String& ruleText,
    std::unique_ptr<protocol::CSS::SourceRange> location,
    std::unique_ptr<protocol::CSS::CSSRule>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspectorStyleSheet = nullptr;
  Response response =
      assertInspectorStyleSheetForId(styleSheetId, inspectorStyleSheet);
  if (!response.isSuccess())
    return response;
  SourceRange ruleLocation;
  response = jsonRangeToSourceRange(inspectorStyleSheet, location.get(),
                                    &ruleLocation);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  AddRuleAction* action =
      new AddRuleAction(inspectorStyleSheet, ruleText, ruleLocation);
  bool success = m_domAgent->history()->perform(action, exceptionState);
  if (!success)
    return InspectorDOMAgent::toResponse(exceptionState);

  CSSStyleRule* rule = action->takeRule();
  *result = buildObjectForRule(rule);
  return Response::OK();
}

Response InspectorCSSAgent::forcePseudoState(
    int nodeId,
    std::unique_ptr<protocol::Array<String>> forcedPseudoClasses) {
  Element* element = nullptr;
  Response response = m_domAgent->assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  unsigned forcedPseudoState =
      computePseudoClassMask(std::move(forcedPseudoClasses));
  NodeIdToForcedPseudoState::iterator it =
      m_nodeIdToForcedPseudoState.find(nodeId);
  unsigned currentForcedPseudoState =
      it == m_nodeIdToForcedPseudoState.end() ? 0 : it->value;
  bool needStyleRecalc = forcedPseudoState != currentForcedPseudoState;
  if (!needStyleRecalc)
    return Response::OK();

  if (forcedPseudoState)
    m_nodeIdToForcedPseudoState.set(nodeId, forcedPseudoState);
  else
    m_nodeIdToForcedPseudoState.remove(nodeId);
  element->ownerDocument()->setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
  return Response::OK();
}

std::unique_ptr<protocol::CSS::CSSMedia> InspectorCSSAgent::buildMediaObject(
    const MediaList* media,
    MediaListSource mediaListSource,
    const String& sourceURL,
    CSSStyleSheet* parentStyleSheet) {
  // Make certain compilers happy by initializing |source| up-front.
  String source = protocol::CSS::CSSMedia::SourceEnum::InlineSheet;
  switch (mediaListSource) {
    case MediaListSourceMediaRule:
      source = protocol::CSS::CSSMedia::SourceEnum::MediaRule;
      break;
    case MediaListSourceImportRule:
      source = protocol::CSS::CSSMedia::SourceEnum::ImportRule;
      break;
    case MediaListSourceLinkedSheet:
      source = protocol::CSS::CSSMedia::SourceEnum::LinkedSheet;
      break;
    case MediaListSourceInlineSheet:
      source = protocol::CSS::CSSMedia::SourceEnum::InlineSheet;
      break;
  }

  const MediaQuerySet* queries = media->queries();
  const HeapVector<Member<MediaQuery>>& queryVector = queries->queryVector();
  LocalFrame* frame = nullptr;
  if (parentStyleSheet) {
    if (Document* document = parentStyleSheet->ownerDocument())
      frame = document->frame();
  }
  MediaQueryEvaluator* mediaEvaluator = new MediaQueryEvaluator(frame);

  InspectorStyleSheet* inspectorStyleSheet =
      parentStyleSheet
          ? m_cssStyleSheetToInspectorStyleSheet.get(parentStyleSheet)
          : nullptr;
  std::unique_ptr<protocol::Array<protocol::CSS::MediaQuery>> mediaListArray =
      protocol::Array<protocol::CSS::MediaQuery>::create();
  MediaValues* mediaValues = MediaValues::createDynamicIfFrameExists(frame);
  bool hasMediaQueryItems = false;
  for (size_t i = 0; i < queryVector.size(); ++i) {
    MediaQuery* query = queryVector.at(i).get();
    const ExpressionHeapVector& expressions = query->expressions();
    std::unique_ptr<protocol::Array<protocol::CSS::MediaQueryExpression>>
        expressionArray =
            protocol::Array<protocol::CSS::MediaQueryExpression>::create();
    bool hasExpressionItems = false;
    for (size_t j = 0; j < expressions.size(); ++j) {
      MediaQueryExp* mediaQueryExp = expressions.at(j).get();
      MediaQueryExpValue expValue = mediaQueryExp->expValue();
      if (!expValue.isValue)
        continue;
      const char* valueName =
          CSSPrimitiveValue::unitTypeToString(expValue.unit);
      std::unique_ptr<protocol::CSS::MediaQueryExpression>
          mediaQueryExpression = protocol::CSS::MediaQueryExpression::create()
                                     .setValue(expValue.value)
                                     .setUnit(String(valueName))
                                     .setFeature(mediaQueryExp->mediaFeature())
                                     .build();

      if (inspectorStyleSheet && media->parentRule())
        mediaQueryExpression->setValueRange(
            inspectorStyleSheet->mediaQueryExpValueSourceRange(
                media->parentRule(), i, j));

      int computedLength;
      if (mediaValues->computeLength(expValue.value, expValue.unit,
                                     computedLength))
        mediaQueryExpression->setComputedLength(computedLength);

      expressionArray->addItem(std::move(mediaQueryExpression));
      hasExpressionItems = true;
    }
    if (!hasExpressionItems)
      continue;
    std::unique_ptr<protocol::CSS::MediaQuery> mediaQuery =
        protocol::CSS::MediaQuery::create()
            .setActive(mediaEvaluator->eval(query, nullptr))
            .setExpressions(std::move(expressionArray))
            .build();
    mediaListArray->addItem(std::move(mediaQuery));
    hasMediaQueryItems = true;
  }

  std::unique_ptr<protocol::CSS::CSSMedia> mediaObject =
      protocol::CSS::CSSMedia::create()
          .setText(media->mediaText())
          .setSource(source)
          .build();
  if (hasMediaQueryItems)
    mediaObject->setMediaList(std::move(mediaListArray));

  if (inspectorStyleSheet && mediaListSource != MediaListSourceLinkedSheet)
    mediaObject->setStyleSheetId(inspectorStyleSheet->id());

  if (!sourceURL.isEmpty()) {
    mediaObject->setSourceURL(sourceURL);

    CSSRule* parentRule = media->parentRule();
    if (!parentRule)
      return mediaObject;
    InspectorStyleSheet* inspectorStyleSheet =
        bindStyleSheet(parentRule->parentStyleSheet());
    mediaObject->setRange(
        inspectorStyleSheet->ruleHeaderSourceRange(parentRule));
  }
  return mediaObject;
}

void InspectorCSSAgent::collectMediaQueriesFromStyleSheet(
    CSSStyleSheet* styleSheet,
    protocol::Array<protocol::CSS::CSSMedia>* mediaArray) {
  MediaList* mediaList = styleSheet->media();
  String sourceURL;
  if (mediaList && mediaList->length()) {
    Document* doc = styleSheet->ownerDocument();
    if (doc)
      sourceURL = doc->url();
    else if (!styleSheet->contents()->baseURL().isEmpty())
      sourceURL = styleSheet->contents()->baseURL();
    else
      sourceURL = "";
    mediaArray->addItem(buildMediaObject(
        mediaList, styleSheet->ownerNode() ? MediaListSourceLinkedSheet
                                           : MediaListSourceInlineSheet,
        sourceURL, styleSheet));
  }
}

void InspectorCSSAgent::collectMediaQueriesFromRule(
    CSSRule* rule,
    protocol::Array<protocol::CSS::CSSMedia>* mediaArray) {
  MediaList* mediaList;
  String sourceURL;
  CSSStyleSheet* parentStyleSheet = nullptr;
  bool isMediaRule = true;
  if (rule->type() == CSSRule::kMediaRule) {
    CSSMediaRule* mediaRule = toCSSMediaRule(rule);
    mediaList = mediaRule->media();
    parentStyleSheet = mediaRule->parentStyleSheet();
  } else if (rule->type() == CSSRule::kImportRule) {
    CSSImportRule* importRule = toCSSImportRule(rule);
    mediaList = importRule->media();
    parentStyleSheet = importRule->parentStyleSheet();
    isMediaRule = false;
  } else {
    mediaList = nullptr;
  }

  if (parentStyleSheet) {
    sourceURL = parentStyleSheet->contents()->baseURL();
    if (sourceURL.isEmpty())
      sourceURL = InspectorDOMAgent::documentURLString(
          parentStyleSheet->ownerDocument());
  } else {
    sourceURL = "";
  }

  if (mediaList && mediaList->length())
    mediaArray->addItem(buildMediaObject(
        mediaList,
        isMediaRule ? MediaListSourceMediaRule : MediaListSourceImportRule,
        sourceURL, parentStyleSheet));
}

std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>
InspectorCSSAgent::buildMediaListChain(CSSRule* rule) {
  if (!rule)
    return nullptr;
  std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>> mediaArray =
      protocol::Array<protocol::CSS::CSSMedia>::create();
  CSSRule* parentRule = rule;
  while (parentRule) {
    collectMediaQueriesFromRule(parentRule, mediaArray.get());
    if (parentRule->parentRule()) {
      parentRule = parentRule->parentRule();
    } else {
      CSSStyleSheet* styleSheet = parentRule->parentStyleSheet();
      while (styleSheet) {
        collectMediaQueriesFromStyleSheet(styleSheet, mediaArray.get());
        parentRule = styleSheet->ownerRule();
        if (parentRule)
          break;
        styleSheet = styleSheet->parentStyleSheet();
      }
    }
  }
  return mediaArray;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(
    Element* element) {
  NodeToInspectorStyleSheet::iterator it =
      m_nodeToInspectorStyleSheet.find(element);
  if (it != m_nodeToInspectorStyleSheet.end())
    return it->value.get();

  CSSStyleDeclaration* style = element->style();
  if (!style)
    return nullptr;

  InspectorStyleSheetForInlineStyle* inspectorStyleSheet =
      InspectorStyleSheetForInlineStyle::create(element, this);
  m_idToInspectorStyleSheetForInlineStyle.set(inspectorStyleSheet->id(),
                                              inspectorStyleSheet);
  m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
  return inspectorStyleSheet;
}

// static
void InspectorCSSAgent::collectAllDocumentStyleSheets(
    Document* document,
    HeapVector<Member<CSSStyleSheet>>& result) {
  const HeapVector<Member<CSSStyleSheet>> activeStyleSheets =
      document->styleEngine().activeStyleSheetsForInspector();
  for (const auto& style : activeStyleSheets) {
    CSSStyleSheet* styleSheet = style.get();
    InspectorCSSAgent::collectStyleSheets(styleSheet, result);
  }
}

// static
void InspectorCSSAgent::collectStyleSheets(
    CSSStyleSheet* styleSheet,
    HeapVector<Member<CSSStyleSheet>>& result) {
  result.append(styleSheet);
  for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
    CSSRule* rule = styleSheet->item(i);
    if (rule->type() == CSSRule::kImportRule) {
      CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
      if (importedStyleSheet)
        InspectorCSSAgent::collectStyleSheets(importedStyleSheet, result);
    }
  }
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(
    CSSStyleSheet* styleSheet) {
  InspectorStyleSheet* inspectorStyleSheet =
      m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
  if (!inspectorStyleSheet) {
    Document* document = styleSheet->ownerDocument();
    inspectorStyleSheet = InspectorStyleSheet::create(
        m_networkAgent, styleSheet, detectOrigin(styleSheet, document),
        InspectorDOMAgent::documentURLString(document), this,
        m_resourceContainer);
    m_idToInspectorStyleSheet.set(inspectorStyleSheet->id(),
                                  inspectorStyleSheet);
    m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
  }
  return inspectorStyleSheet;
}

String InspectorCSSAgent::styleSheetId(CSSStyleSheet* styleSheet) {
  return bindStyleSheet(styleSheet)->id();
}

String InspectorCSSAgent::unbindStyleSheet(
    InspectorStyleSheet* inspectorStyleSheet) {
  String id = inspectorStyleSheet->id();
  m_idToInspectorStyleSheet.remove(id);
  if (inspectorStyleSheet->pageStyleSheet())
    m_cssStyleSheetToInspectorStyleSheet.remove(
        inspectorStyleSheet->pageStyleSheet());
  return id;
}

InspectorStyleSheet* InspectorCSSAgent::inspectorStyleSheetForRule(
    CSSStyleRule* rule) {
  if (!rule)
    return nullptr;

  // CSSRules returned by StyleResolver::pseudoCSSRulesForElement lack parent
  // pointers if they are coming from user agent stylesheets. To work around
  // this issue, we use CSSOM wrapper created by inspector.
  if (!rule->parentStyleSheet()) {
    if (!m_inspectorUserAgentStyleSheet)
      m_inspectorUserAgentStyleSheet = CSSStyleSheet::create(
          CSSDefaultStyleSheets::instance().defaultStyleSheet());
    rule->setParentStyleSheet(m_inspectorUserAgentStyleSheet.get());
  }
  return bindStyleSheet(rule->parentStyleSheet());
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(
    Document* document) {
  if (!document)
    return nullptr;

  if (!document->isHTMLDocument() && !document->isSVGDocument())
    return nullptr;

  CSSStyleSheet& inspectorSheet =
      document->styleEngine().ensureInspectorStyleSheet();

  flushPendingProtocolNotifications();

  return m_cssStyleSheetToInspectorStyleSheet.get(&inspectorSheet);
}

Response InspectorCSSAgent::assertInspectorStyleSheetForId(
    const String& styleSheetId,
    InspectorStyleSheet*& result) {
  IdToInspectorStyleSheet::iterator it =
      m_idToInspectorStyleSheet.find(styleSheetId);
  if (it == m_idToInspectorStyleSheet.end())
    return Response::Error("No style sheet with given id found");
  result = it->value.get();
  return Response::OK();
}

Response InspectorCSSAgent::assertStyleSheetForId(
    const String& styleSheetId,
    InspectorStyleSheetBase*& result) {
  InspectorStyleSheet* styleSheet = nullptr;
  Response response = assertInspectorStyleSheetForId(styleSheetId, styleSheet);
  if (response.isSuccess()) {
    result = styleSheet;
    return response;
  }
  IdToInspectorStyleSheetForInlineStyle::iterator it =
      m_idToInspectorStyleSheetForInlineStyle.find(styleSheetId);
  if (it == m_idToInspectorStyleSheetForInlineStyle.end())
    return Response::Error("No style sheet with given id found");
  result = it->value.get();
  return Response::OK();
}

protocol::CSS::StyleSheetOrigin InspectorCSSAgent::detectOrigin(
    CSSStyleSheet* pageStyleSheet,
    Document* ownerDocument) {
  DCHECK(pageStyleSheet);

  if (!pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
    return protocol::CSS::StyleSheetOriginEnum::UserAgent;

  if (pageStyleSheet->ownerNode() &&
      pageStyleSheet->ownerNode()->isDocumentNode()) {
    if (pageStyleSheet == ownerDocument->styleEngine().inspectorStyleSheet())
      return protocol::CSS::StyleSheetOriginEnum::Inspector;
    return protocol::CSS::StyleSheetOriginEnum::Injected;
  }
  return protocol::CSS::StyleSheetOriginEnum::Regular;
}

std::unique_ptr<protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(
    CSSStyleRule* rule) {
  InspectorStyleSheet* inspectorStyleSheet = inspectorStyleSheetForRule(rule);
  if (!inspectorStyleSheet)
    return nullptr;

  std::unique_ptr<protocol::CSS::CSSRule> result =
      inspectorStyleSheet->buildObjectForRuleWithoutMedia(rule);
  result->setMedia(buildMediaListChain(rule));
  return result;
}

static inline bool matchesPseudoElement(const CSSSelector* selector,
                                        PseudoId elementPseudoId) {
  // According to http://www.w3.org/TR/css3-selectors/#pseudo-elements, "Only
  // one pseudo-element may appear per selector."
  // As such, check the last selector in the tag history.
  for (; !selector->isLastInTagHistory(); ++selector) {
  }
  PseudoId selectorPseudoId = CSSSelector::pseudoId(selector->getPseudoType());

  // FIXME: This only covers the case of matching pseudo-element selectors
  // against PseudoElements.  We should come up with a solution for matching
  // pseudo-element selectors against ordinary Elements, too.
  return selectorPseudoId == elementPseudoId;
}

std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>>
InspectorCSSAgent::buildArrayForMatchedRuleList(CSSRuleList* ruleList,
                                                Element* element,
                                                PseudoId matchesForPseudoId) {
  std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>> result =
      protocol::Array<protocol::CSS::RuleMatch>::create();
  if (!ruleList)
    return result;

  HeapVector<Member<CSSStyleRule>> uniqRules = filterDuplicateRules(ruleList);
  for (unsigned i = 0; i < uniqRules.size(); ++i) {
    CSSStyleRule* rule = uniqRules.at(i).get();
    std::unique_ptr<protocol::CSS::CSSRule> ruleObject =
        buildObjectForRule(rule);
    if (!ruleObject)
      continue;
    std::unique_ptr<protocol::Array<int>> matchingSelectors =
        protocol::Array<int>::create();
    const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
    long index = 0;
    PseudoId elementPseudoId =
        matchesForPseudoId ? matchesForPseudoId : element->getPseudoId();
    for (const CSSSelector* selector = selectorList.first(); selector;
         selector = CSSSelectorList::next(*selector)) {
      const CSSSelector* firstTagHistorySelector = selector;
      bool matched = false;
      if (elementPseudoId)
        matched = matchesPseudoElement(
            selector, elementPseudoId);  // Modifies |selector|.
      else
        matched = element->matches(firstTagHistorySelector->selectorText(),
                                   IGNORE_EXCEPTION);
      if (matched)
        matchingSelectors->addItem(index);
      ++index;
    }
    result->addItem(protocol::CSS::RuleMatch::create()
                        .setRule(std::move(ruleObject))
                        .setMatchingSelectors(std::move(matchingSelectors))
                        .build());
  }

  return result;
}

std::unique_ptr<protocol::CSS::CSSStyle>
InspectorCSSAgent::buildObjectForAttributesStyle(Element* element) {
  if (!element->isStyledElement())
    return nullptr;

  // FIXME: Ugliness below.
  StylePropertySet* attributeStyle =
      const_cast<StylePropertySet*>(element->presentationAttributeStyle());
  if (!attributeStyle)
    return nullptr;

  MutableStylePropertySet* mutableAttributeStyle =
      toMutableStylePropertySet(attributeStyle);

  InspectorStyle* inspectorStyle = InspectorStyle::create(
      mutableAttributeStyle->ensureCSSStyleDeclaration(), nullptr, nullptr);
  return inspectorStyle->buildObjectForStyle();
}

void InspectorCSSAgent::didAddDocument(Document* document) {
  if (!m_tracker)
    return;

  document->styleEngine().setRuleUsageTracker(m_tracker);
  document->setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
}

void InspectorCSSAgent::didRemoveDocument(Document* document) {}

void InspectorCSSAgent::didRemoveDOMNode(Node* node) {
  if (!node)
    return;

  int nodeId = m_domAgent->boundNodeId(node);
  if (nodeId)
    m_nodeIdToForcedPseudoState.remove(nodeId);

  NodeToInspectorStyleSheet::iterator it =
      m_nodeToInspectorStyleSheet.find(node);
  if (it == m_nodeToInspectorStyleSheet.end())
    return;

  m_idToInspectorStyleSheetForInlineStyle.remove(it->value->id());
  m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element) {
  if (!element)
    return;

  NodeToInspectorStyleSheet::iterator it =
      m_nodeToInspectorStyleSheet.find(element);
  if (it == m_nodeToInspectorStyleSheet.end())
    return;

  it->value->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheetBase* styleSheet) {
  if (s_frontendOperationCounter)
    return;
  flushPendingProtocolNotifications();
  frontend()->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::resetPseudoStates() {
  HeapHashSet<Member<Document>> documentsToChange;
  for (auto& state : m_nodeIdToForcedPseudoState) {
    Element* element = toElement(m_domAgent->nodeForId(state.key));
    if (element && element->ownerDocument())
      documentsToChange.add(element->ownerDocument());
  }

  m_nodeIdToForcedPseudoState.clear();
  for (auto& document : documentsToChange)
    document->setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
}

HeapVector<Member<CSSStyleDeclaration>> InspectorCSSAgent::matchingStyles(
    Element* element) {
  PseudoId pseudoId = element->getPseudoId();
  if (pseudoId)
    element = element->parentElement();
  StyleResolver& styleResolver =
      element->ownerDocument()->ensureStyleResolver();
  element->updateDistribution();

  HeapVector<Member<CSSStyleRule>> rules =
      filterDuplicateRules(styleResolver.pseudoCSSRulesForElement(
          element, pseudoId, StyleResolver::AllCSSRules));
  HeapVector<Member<CSSStyleDeclaration>> styles;
  if (!pseudoId && element->style())
    styles.append(element->style());
  for (unsigned i = rules.size(); i > 0; --i) {
    CSSStyleSheet* parentStyleSheet = rules.at(i - 1)->parentStyleSheet();
    if (!parentStyleSheet || !parentStyleSheet->ownerNode())
      continue;  // User agent.
    styles.append(rules.at(i - 1)->style());
  }
  return styles;
}

CSSStyleDeclaration* InspectorCSSAgent::findEffectiveDeclaration(
    CSSPropertyID propertyId,
    const HeapVector<Member<CSSStyleDeclaration>>& styles) {
  if (!styles.size())
    return nullptr;

  String longhand = getPropertyNameString(propertyId);
  CSSStyleDeclaration* foundStyle = nullptr;

  for (unsigned i = 0; i < styles.size(); ++i) {
    CSSStyleDeclaration* style = styles.at(i).get();
    if (style->getPropertyValue(longhand).isEmpty())
      continue;
    if (style->getPropertyPriority(longhand) == "important")
      return style;
    if (!foundStyle)
      foundStyle = style;
  }

  return foundStyle ? foundStyle : styles.at(0).get();
}

Response InspectorCSSAgent::setLayoutEditorValue(Element* element,
                                                 CSSStyleDeclaration* style,
                                                 CSSPropertyID propertyId,
                                                 const String& value,
                                                 bool forceImportant) {
  InspectorStyleSheetBase* inspectorStyleSheet = nullptr;
  RefPtr<CSSRuleSourceData> sourceData;
  // An absence of the parent rule means that given style is an inline style.
  if (style->parentRule()) {
    InspectorStyleSheet* styleSheet = bindStyleSheet(style->parentStyleSheet());
    inspectorStyleSheet = styleSheet;
    sourceData = styleSheet->sourceDataForRule(style->parentRule());
  } else {
    InspectorStyleSheetForInlineStyle* inlineStyleSheet =
        asInspectorStyleSheet(element);
    inspectorStyleSheet = inlineStyleSheet;
    sourceData = inlineStyleSheet->ruleSourceData();
  }

  if (!sourceData)
    return Response::Error("Can't find a source to edit");

  Vector<StylePropertyShorthand, 4> shorthands;
  getMatchingShorthandsForLonghand(propertyId, &shorthands);

  String shorthand = shorthands.size() > 0
                         ? getPropertyNameString(shorthands[0].id())
                         : String();
  String longhand = getPropertyNameString(propertyId);

  int foundIndex = -1;
  Vector<CSSPropertySourceData> properties =
      sourceData->styleSourceData->propertyData;
  for (unsigned i = 0; i < properties.size(); ++i) {
    CSSPropertySourceData property = properties[properties.size() - i - 1];
    String name = property.name;
    if (property.disabled)
      continue;

    if (name != shorthand && name != longhand)
      continue;

    if (property.important || foundIndex == -1)
      foundIndex = properties.size() - i - 1;

    if (property.important)
      break;
  }

  SourceRange bodyRange = sourceData->ruleBodyRange;
  String styleSheetText;
  inspectorStyleSheet->getText(&styleSheetText);
  String styleText =
      styleSheetText.substring(bodyRange.start, bodyRange.length());
  SourceRange changeRange;
  if (foundIndex == -1) {
    String newPropertyText = "\n" + longhand + ": " + value +
                             (forceImportant ? " !important" : "") + ";";
    if (!styleText.isEmpty() && !styleText.stripWhiteSpace().endsWith(';'))
      newPropertyText = ";" + newPropertyText;
    styleText.append(newPropertyText);
    changeRange.start = bodyRange.end;
    changeRange.end = bodyRange.end + newPropertyText.length();
  } else {
    CSSPropertySourceData declaration = properties[foundIndex];
    String newValueText;
    if (declaration.name == shorthand)
      newValueText = createShorthandValue(element->ownerDocument(), shorthand,
                                          declaration.value, longhand, value);
    else
      newValueText = value;

    String newPropertyText =
        declaration.name + ": " + newValueText +
        (declaration.important || forceImportant ? " !important" : "") + ";";
    styleText.replace(declaration.range.start - bodyRange.start,
                      declaration.range.length(), newPropertyText);
    changeRange.start = declaration.range.start;
    changeRange.end = changeRange.start + newPropertyText.length();
  }
  CSSStyleDeclaration* resultStyle = nullptr;
  Response response =
      setStyleText(inspectorStyleSheet, bodyRange, styleText, resultStyle);
  if (!response.isSuccess())
    return response;
  frontend()->layoutEditorChange(
      inspectorStyleSheet->id(),
      inspectorStyleSheet->buildSourceRangeObject(changeRange));
  return Response::OK();
}

void InspectorCSSAgent::layoutEditorItemSelected(Element* element,
                                                 CSSStyleDeclaration* style) {
  InspectorStyleSheetBase* inspectorStyleSheet = nullptr;
  RefPtr<CSSRuleSourceData> sourceData;
  if (style->parentRule()) {
    InspectorStyleSheet* styleSheet = bindStyleSheet(style->parentStyleSheet());
    inspectorStyleSheet = styleSheet;
    sourceData = styleSheet->sourceDataForRule(style->parentRule());
  } else {
    InspectorStyleSheetForInlineStyle* inlineStyleSheet =
        asInspectorStyleSheet(element);
    inspectorStyleSheet = inlineStyleSheet;
    sourceData = inlineStyleSheet->ruleSourceData();
  }

  if (sourceData)
    frontend()->layoutEditorChange(inspectorStyleSheet->id(),
                                   inspectorStyleSheet->buildSourceRangeObject(
                                       sourceData->ruleHeaderRange));
}

Response InspectorCSSAgent::setEffectivePropertyValueForNode(
    int nodeId,
    const String& propertyName,
    const String& value) {
  // TODO: move testing from CSSAgent to layout editor.
  Element* element = nullptr;
  Response response = m_domAgent->assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;
  if (element->getPseudoId())
    return Response::Error("Elements is pseudo");

  CSSPropertyID property = cssPropertyID(propertyName);
  if (!property)
    return Response::Error("Invalid property name");

  Document* ownerDocument = element->ownerDocument();
  if (!ownerDocument->isActive())
    return Response::Error("Can't edit a node from a non-active document");

  CSSPropertyID propertyId = cssPropertyID(propertyName);
  CSSStyleDeclaration* style =
      findEffectiveDeclaration(propertyId, matchingStyles(element));
  if (!style)
    return Response::Error("Can't find a style to edit");

  return setLayoutEditorValue(element, style, propertyId, value);
}

Response InspectorCSSAgent::getBackgroundColors(
    int nodeId,
    Maybe<protocol::Array<String>>* result) {
  Element* element = nullptr;
  Response response = m_domAgent->assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  LayoutRect textBounds;
  LayoutObject* elementLayout = element->layoutObject();
  if (!elementLayout)
    return Response::OK();

  for (const LayoutObject* child = elementLayout->slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->isText())
      continue;
    textBounds.unite(LayoutRect(child->absoluteBoundingBoxRect()));
  }
  if (textBounds.size().isEmpty())
    return Response::OK();

  Vector<Color> colors;
  FrameView* view = element->document().view();
  if (!view)
    return Response::Error("No view.");
  Document& document = element->document();
  bool isMainFrame = document.isInMainFrame();
  bool foundOpaqueColor = false;
  if (isMainFrame && !view->isTransparent()) {
    // Start with the "default" page color (typically white).
    Color baseBackgroundColor = view->baseBackgroundColor();
    colors.append(view->baseBackgroundColor());
    foundOpaqueColor = !baseBackgroundColor.hasAlpha();
  }

  foundOpaqueColor =
      getColorsFromRect(textBounds, element->document(), element, colors);

  if (!foundOpaqueColor && !isMainFrame) {
    for (HTMLFrameOwnerElement* ownerElement = document.localOwner();
         !foundOpaqueColor && ownerElement;
         ownerElement = ownerElement->document().localOwner()) {
      foundOpaqueColor = getColorsFromRect(textBounds, ownerElement->document(),
                                           nullptr, colors);
    }
  }

  *result = protocol::Array<String>::create();
  for (auto color : colors)
    result->fromJust()->addItem(color.serializedAsCSSComponentValue());
  return Response::OK();
}

Response InspectorCSSAgent::getLayoutTreeAndStyles(
    std::unique_ptr<protocol::Array<String>> styleWhitelist,
    std::unique_ptr<protocol::Array<protocol::CSS::LayoutTreeNode>>*
        layoutTreeNodes,
    std::unique_ptr<protocol::Array<protocol::CSS::ComputedStyle>>*
        computedStyles) {
  m_domAgent->document()->updateStyleAndLayoutTree();

  // Look up the CSSPropertyIDs for each entry in |styleWhitelist|.
  Vector<std::pair<String, CSSPropertyID>> cssPropertyWhitelist;
  for (size_t i = 0; i < styleWhitelist->length(); i++) {
    CSSPropertyID propertyId = cssPropertyID(styleWhitelist->get(i));
    if (propertyId == CSSPropertyInvalid)
      continue;
    cssPropertyWhitelist.append(
        std::make_pair(styleWhitelist->get(i), propertyId));
  }

  *layoutTreeNodes = protocol::Array<protocol::CSS::LayoutTreeNode>::create();
  *computedStyles = protocol::Array<protocol::CSS::ComputedStyle>::create();

  ComputedStylesMap styleToIndexMap;
  visitLayoutTreeNodes(m_domAgent->document(), *layoutTreeNodes->get(),
                       cssPropertyWhitelist, styleToIndexMap,
                       *computedStyles->get());
  return Response::OK();
}

int InspectorCSSAgent::getStyleIndexForNode(
    Node* node,
    const Vector<std::pair<String, CSSPropertyID>>& cssPropertyWhitelist,
    ComputedStylesMap& styleToIndexMap,
    protocol::Array<protocol::CSS::ComputedStyle>& computedStyles) {
  CSSComputedStyleDeclaration* computedStyleInfo =
      CSSComputedStyleDeclaration::create(node, true);

  Vector<String> style;
  bool allPropertiesEmpty = true;
  for (const auto& pair : cssPropertyWhitelist) {
    String value = computedStyleInfo->getPropertyValue(pair.second);
    if (!value.isEmpty())
      allPropertiesEmpty = false;
    style.append(value);
  }

  // -1 means an empty style.
  if (allPropertiesEmpty)
    return -1;

  ComputedStylesMap::iterator it = styleToIndexMap.find(style);
  if (it != styleToIndexMap.end())
    return it->value;

  // It's a distinct style, so append to |computedStyles|.
  std::unique_ptr<protocol::Array<protocol::CSS::CSSComputedStyleProperty>>
      styleProperties =
          protocol::Array<protocol::CSS::CSSComputedStyleProperty>::create();

  for (size_t i = 0; i < style.size(); i++) {
    if (style[i].isEmpty())
      continue;
    styleProperties->addItem(protocol::CSS::CSSComputedStyleProperty::create()
                                 .setName(cssPropertyWhitelist[i].first)
                                 .setValue(style[i])
                                 .build());
  }
  computedStyles.addItem(protocol::CSS::ComputedStyle::create()
                             .setProperties(std::move(styleProperties))
                             .build());

  size_t index = styleToIndexMap.size();
  styleToIndexMap.add(std::move(style), index);
  return index;
}

void InspectorCSSAgent::visitLayoutTreeNodes(
    Node* node,
    protocol::Array<protocol::CSS::LayoutTreeNode>& layoutTreeNodes,
    const Vector<std::pair<String, CSSPropertyID>>& cssPropertyWhitelist,
    ComputedStylesMap& styleToIndexMap,
    protocol::Array<protocol::CSS::ComputedStyle>& computedStyles) {
  for (; node; node = NodeTraversal::next(*node)) {
    // Visit shadow dom nodes.
    if (node->isElementNode()) {
      const Element* element = toElement(node);
      ElementShadow* elementShadow = element->shadow();
      if (elementShadow) {
        visitLayoutTreeNodes(&elementShadow->youngestShadowRoot(),
                             layoutTreeNodes, cssPropertyWhitelist,
                             styleToIndexMap, computedStyles);
      }
    }

    // Pierce iframe boundaries.
    if (node->isFrameOwnerElement()) {
      Document* contentDocument =
          toHTMLFrameOwnerElement(node)->contentDocument();
      contentDocument->updateStyleAndLayoutTree();
      visitLayoutTreeNodes(contentDocument->documentElement(), layoutTreeNodes,
                           cssPropertyWhitelist, styleToIndexMap,
                           computedStyles);
    }

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
      continue;

    int nodeId = m_domAgent->boundNodeId(node);
    if (!nodeId)
      continue;

    std::unique_ptr<protocol::CSS::LayoutTreeNode> layoutTreeNode =
        protocol::CSS::LayoutTreeNode::create()
            .setNodeId(nodeId)
            .setBoundingBox(buildRectForFloatRect(
                node->isElementNode()
                    ? FloatRect(toElement(node)->boundsInViewport())
                    : layoutObject->absoluteBoundingBoxRect()))
            .build();
    int styleIndex = getStyleIndexForNode(node, cssPropertyWhitelist,
                                          styleToIndexMap, computedStyles);
    if (styleIndex != -1)
      layoutTreeNode->setStyleIndex(styleIndex);

    if (layoutObject->isText()) {
      LayoutText* layoutText = toLayoutText(layoutObject);
      layoutTreeNode->setLayoutText(layoutText->text());
      if (layoutText->hasTextBoxes()) {
        std::unique_ptr<protocol::Array<protocol::CSS::InlineTextBox>>
            inlineTextNodes =
                protocol::Array<protocol::CSS::InlineTextBox>::create();
        for (const InlineTextBox* textBox = layoutText->firstTextBox(); textBox;
             textBox = textBox->nextTextBox()) {
          FloatRect localCoordsTextBoxRect(textBox->calculateBoundaries());
          FloatRect absoluteCoordsTextBoxRect =
              layoutObject->localToAbsoluteQuad(localCoordsTextBoxRect)
                  .boundingBox();
          inlineTextNodes->addItem(protocol::CSS::InlineTextBox::create()
                                       .setStartCharacterIndex(textBox->start())
                                       .setNumCharacters(textBox->len())
                                       .setBoundingBox(buildRectForFloatRect(
                                           absoluteCoordsTextBoxRect))
                                       .build());
        }
        layoutTreeNode->setInlineTextNodes(std::move(inlineTextNodes));
      }
    }

    layoutTreeNodes.addItem(std::move(layoutTreeNode));
  }
}

void InspectorCSSAgent::setUsageTrackerStatus(bool enabled) {
  if (enabled) {
    if (!m_tracker)
      m_tracker = new StyleRuleUsageTracker();
  } else {
    m_tracker = nullptr;
  }

  HeapVector<Member<Document>> documents = m_domAgent->documents();
  for (Document* document : documents) {
    document->styleEngine().setRuleUsageTracker(m_tracker);

    document->setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
  }
}

Response InspectorCSSAgent::startRuleUsageTracking() {
  m_state->setBoolean(CSSAgentState::ruleRecordingEnabled, true);
  setUsageTrackerStatus(true);
  return Response::OK();
}

std::unique_ptr<protocol::CSS::RuleUsage>
InspectorCSSAgent::buildObjectForRuleUsage(CSSStyleRule* rule, bool used) {
  InspectorStyleSheet* inspectorStyleSheet = inspectorStyleSheetForRule(rule);
  if (!inspectorStyleSheet)
    return nullptr;

  std::unique_ptr<protocol::CSS::RuleUsage> result =
      inspectorStyleSheet->buildObjectForRuleUsage(rule, used);

  return result;
}

Response InspectorCSSAgent::stopRuleUsageTracking(
    std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result) {
  if (!m_tracker) {
    return Response::Error("CSS rule usage tracking is not enabled");
  }

  *result = protocol::Array<protocol::CSS::RuleUsage>::create();

  HeapVector<Member<Document>> documents = m_domAgent->documents();
  for (Document* document : documents) {
    HeapHashSet<Member<CSSStyleSheet>>* newSheetsVector =
        m_documentToCSSStyleSheets.get(document);

    if (!newSheetsVector)
      continue;

    for (auto sheet : *newSheetsVector) {
      InspectorStyleSheet* styleSheet =
          m_cssStyleSheetToInspectorStyleSheet.get(sheet);
      const CSSRuleVector ruleVector = styleSheet->flatRules();
      for (auto rule : ruleVector) {
        if (rule->type() != CSSRule::kStyleRule)
          continue;

        CSSStyleRule* cssRule = static_cast<CSSStyleRule*>(rule.get());

        StyleRule* styleRule = cssRule->styleRule();

        std::unique_ptr<protocol::CSS::RuleUsage> protocolRule =
            buildObjectForRuleUsage(cssRule, m_tracker->contains(styleRule));
        if (!protocolRule)
          continue;

        result->get()->addItem(std::move(protocolRule));
      }
    }
  }

  setUsageTrackerStatus(false);

  return Response::OK();
}

DEFINE_TRACE(InspectorCSSAgent) {
  visitor->trace(m_domAgent);
  visitor->trace(m_inspectedFrames);
  visitor->trace(m_networkAgent);
  visitor->trace(m_resourceContentLoader);
  visitor->trace(m_resourceContainer);
  visitor->trace(m_idToInspectorStyleSheet);
  visitor->trace(m_idToInspectorStyleSheetForInlineStyle);
  visitor->trace(m_cssStyleSheetToInspectorStyleSheet);
  visitor->trace(m_documentToCSSStyleSheets);
  visitor->trace(m_invalidatedDocuments);
  visitor->trace(m_nodeToInspectorStyleSheet);
  visitor->trace(m_inspectorUserAgentStyleSheet);
  visitor->trace(m_tracker);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
