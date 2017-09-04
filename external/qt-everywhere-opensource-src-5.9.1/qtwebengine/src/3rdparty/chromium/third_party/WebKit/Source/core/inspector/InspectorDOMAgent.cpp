/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorDOMAgent.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Node.h"
#include "core/InputTypeNames.h"
#include "core/dom/Attr.h"
#include "core/dom/CharacterData.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/inspector/DOMEditor.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/V8InspectorString.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/xml/DocumentXPathEvaluator.h"
#include "core/xml/XPathResult.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "wtf/ListHashSet.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

namespace DOMAgentState {
static const char domAgentEnabled[] = "domAgentEnabled";
};

namespace {

const size_t maxTextSize = 10000;
const UChar ellipsisUChar[] = {0x2026, 0};

Color parseColor(protocol::DOM::RGBA* rgba) {
  if (!rgba)
    return Color::transparent;

  int r = rgba->getR();
  int g = rgba->getG();
  int b = rgba->getB();
  if (!rgba->hasA())
    return Color(r, g, b);

  double a = rgba->getA(1);
  // Clamp alpha to the [0..1] range.
  if (a < 0)
    a = 0;
  else if (a > 1)
    a = 1;

  return Color(r, g, b, static_cast<int>(a * 255));
}

bool parseQuad(std::unique_ptr<protocol::Array<double>> quadArray,
               FloatQuad* quad) {
  const size_t coordinatesInQuad = 8;
  if (!quadArray || quadArray->length() != coordinatesInQuad)
    return false;
  quad->setP1(FloatPoint(quadArray->get(0), quadArray->get(1)));
  quad->setP2(FloatPoint(quadArray->get(2), quadArray->get(3)));
  quad->setP3(FloatPoint(quadArray->get(4), quadArray->get(5)));
  quad->setP4(FloatPoint(quadArray->get(6), quadArray->get(7)));
  return true;
}

v8::Local<v8::Value> nodeV8Value(v8::Local<v8::Context> context, Node* node) {
  v8::Isolate* isolate = context->GetIsolate();
  if (!node ||
      !BindingSecurity::shouldAllowAccessTo(
          currentDOMWindow(isolate), node,
          BindingSecurity::ErrorReportOption::DoNotReport))
    return v8::Null(isolate);
  return toV8(node, context->Global(), isolate);
}

}  // namespace

class InspectorRevalidateDOMTask final
    : public GarbageCollectedFinalized<InspectorRevalidateDOMTask> {
 public:
  explicit InspectorRevalidateDOMTask(InspectorDOMAgent*);
  void scheduleStyleAttrRevalidationFor(Element*);
  void reset() { m_timer.stop(); }
  void onTimer(TimerBase*);
  DECLARE_TRACE();

 private:
  Member<InspectorDOMAgent> m_domAgent;
  Timer<InspectorRevalidateDOMTask> m_timer;
  HeapHashSet<Member<Element>> m_styleAttrInvalidatedElements;
};

InspectorRevalidateDOMTask::InspectorRevalidateDOMTask(
    InspectorDOMAgent* domAgent)
    : m_domAgent(domAgent),
      m_timer(this, &InspectorRevalidateDOMTask::onTimer) {}

void InspectorRevalidateDOMTask::scheduleStyleAttrRevalidationFor(
    Element* element) {
  m_styleAttrInvalidatedElements.add(element);
  if (!m_timer.isActive())
    m_timer.startOneShot(0, BLINK_FROM_HERE);
}

void InspectorRevalidateDOMTask::onTimer(TimerBase*) {
  // The timer is stopped on m_domAgent destruction, so this method will never
  // be called after m_domAgent has been destroyed.
  HeapVector<Member<Element>> elements;
  for (auto& attribute : m_styleAttrInvalidatedElements)
    elements.append(attribute.get());
  m_domAgent->styleAttributeInvalidated(elements);
  m_styleAttrInvalidatedElements.clear();
}

DEFINE_TRACE(InspectorRevalidateDOMTask) {
  visitor->trace(m_domAgent);
  visitor->trace(m_styleAttrInvalidatedElements);
}

Response InspectorDOMAgent::toResponse(ExceptionState& exceptionState) {
  if (exceptionState.hadException()) {
    return Response::Error(DOMException::getErrorName(exceptionState.code()) +
                           " " + exceptionState.message());
  }
  return Response::OK();
}

bool InspectorDOMAgent::getPseudoElementType(PseudoId pseudoId,
                                             protocol::DOM::PseudoType* type) {
  switch (pseudoId) {
    case PseudoIdFirstLine:
      *type = protocol::DOM::PseudoTypeEnum::FirstLine;
      return true;
    case PseudoIdFirstLetter:
      *type = protocol::DOM::PseudoTypeEnum::FirstLetter;
      return true;
    case PseudoIdBefore:
      *type = protocol::DOM::PseudoTypeEnum::Before;
      return true;
    case PseudoIdAfter:
      *type = protocol::DOM::PseudoTypeEnum::After;
      return true;
    case PseudoIdBackdrop:
      *type = protocol::DOM::PseudoTypeEnum::Backdrop;
      return true;
    case PseudoIdSelection:
      *type = protocol::DOM::PseudoTypeEnum::Selection;
      return true;
    case PseudoIdFirstLineInherited:
      *type = protocol::DOM::PseudoTypeEnum::FirstLineInherited;
      return true;
    case PseudoIdScrollbar:
      *type = protocol::DOM::PseudoTypeEnum::Scrollbar;
      return true;
    case PseudoIdScrollbarThumb:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarThumb;
      return true;
    case PseudoIdScrollbarButton:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarButton;
      return true;
    case PseudoIdScrollbarTrack:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarTrack;
      return true;
    case PseudoIdScrollbarTrackPiece:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarTrackPiece;
      return true;
    case PseudoIdScrollbarCorner:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarCorner;
      return true;
    case PseudoIdResizer:
      *type = protocol::DOM::PseudoTypeEnum::Resizer;
      return true;
    case PseudoIdInputListButton:
      *type = protocol::DOM::PseudoTypeEnum::InputListButton;
      return true;
    default:
      return false;
  }
}

InspectorDOMAgent::InspectorDOMAgent(
    v8::Isolate* isolate,
    InspectedFrames* inspectedFrames,
    v8_inspector::V8InspectorSession* v8Session,
    Client* client)
    : m_isolate(isolate),
      m_inspectedFrames(inspectedFrames),
      m_v8Session(v8Session),
      m_client(client),
      m_domListener(nullptr),
      m_documentNodeToIdMap(new NodeToIdMap()),
      m_lastNodeId(1),
      m_suppressAttributeModifiedEvent(false),
      m_backendNodeIdToInspect(0) {}

InspectorDOMAgent::~InspectorDOMAgent() {}

void InspectorDOMAgent::restore() {
  if (!enabled())
    return;
  innerEnable();
}

HeapVector<Member<Document>> InspectorDOMAgent::documents() {
  HeapVector<Member<Document>> result;
  if (m_document) {
    for (LocalFrame* frame : *m_inspectedFrames) {
      if (Document* document = frame->document())
        result.append(document);
    }
  }
  return result;
}

void InspectorDOMAgent::setDOMListener(DOMListener* listener) {
  m_domListener = listener;
}

void InspectorDOMAgent::setDocument(Document* doc) {
  if (doc == m_document.get())
    return;

  discardFrontendBindings();
  m_document = doc;

  if (!enabled())
    return;

  // Immediately communicate 0 document or document that has finished loading.
  if (!doc || !doc->parsing())
    frontend()->documentUpdated();
}

void InspectorDOMAgent::releaseDanglingNodes() {
  m_danglingNodeToIdMaps.clear();
}

int InspectorDOMAgent::bind(Node* node, NodeToIdMap* nodesMap) {
  int id = nodesMap->get(node);
  if (id)
    return id;
  id = m_lastNodeId++;
  nodesMap->set(node, id);
  m_idToNode.set(id, node);
  m_idToNodesMap.set(id, nodesMap);
  return id;
}

void InspectorDOMAgent::unbind(Node* node, NodeToIdMap* nodesMap) {
  int id = nodesMap->get(node);
  if (!id)
    return;

  m_idToNode.remove(id);
  m_idToNodesMap.remove(id);

  if (node->isFrameOwnerElement()) {
    Document* contentDocument =
        toHTMLFrameOwnerElement(node)->contentDocument();
    if (m_domListener)
      m_domListener->didRemoveDocument(contentDocument);
    if (contentDocument)
      unbind(contentDocument, nodesMap);
  }

  for (ShadowRoot* root = node->youngestShadowRoot(); root;
       root = root->olderShadowRoot())
    unbind(root, nodesMap);

  if (node->isElementNode()) {
    Element* element = toElement(node);
    if (element->pseudoElement(PseudoIdBefore))
      unbind(element->pseudoElement(PseudoIdBefore), nodesMap);
    if (element->pseudoElement(PseudoIdAfter))
      unbind(element->pseudoElement(PseudoIdAfter), nodesMap);

    if (isHTMLLinkElement(*element)) {
      HTMLLinkElement& linkElement = toHTMLLinkElement(*element);
      if (linkElement.isImport() && linkElement.import())
        unbind(linkElement.import(), nodesMap);
    }
  }

  nodesMap->remove(node);
  if (m_domListener)
    m_domListener->didRemoveDOMNode(node);

  bool childrenRequested = m_childrenRequested.contains(id);
  if (childrenRequested) {
    // Unbind subtree known to client recursively.
    m_childrenRequested.remove(id);
    Node* child = innerFirstChild(node);
    while (child) {
      unbind(child, nodesMap);
      child = innerNextSibling(child);
    }
  }
  if (nodesMap == m_documentNodeToIdMap.get())
    m_cachedChildCount.remove(id);
}

Response InspectorDOMAgent::assertNode(int nodeId, Node*& node) {
  node = nodeForId(nodeId);
  if (!node)
    return Response::Error("Could not find node with given id");
  return Response::OK();
}

Response InspectorDOMAgent::assertElement(int nodeId, Element*& element) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  if (!node->isElementNode())
    return Response::Error("Node is not an Element");
  element = toElement(node);
  return Response::OK();
}

// static
ShadowRoot* InspectorDOMAgent::userAgentShadowRoot(Node* node) {
  if (!node || !node->isInShadowTree())
    return nullptr;

  Node* candidate = node;
  while (candidate && !candidate->isShadowRoot())
    candidate = candidate->parentOrShadowHostNode();
  ASSERT(candidate);
  ShadowRoot* shadowRoot = toShadowRoot(candidate);

  return shadowRoot->type() == ShadowRootType::UserAgent ? shadowRoot : nullptr;
}

Response InspectorDOMAgent::assertEditableNode(int nodeId, Node*& node) {
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  if (node->isInShadowTree()) {
    if (node->isShadowRoot())
      return Response::Error("Cannot edit shadow roots");
    if (userAgentShadowRoot(node))
      return Response::Error("Cannot edit nodes from user-agent shadow trees");
  }

  if (node->isPseudoElement())
    return Response::Error("Cannot edit pseudo elements");
  return Response::OK();
}

Response InspectorDOMAgent::assertEditableChildNode(Element* parentElement,
                                                    int nodeId,
                                                    Node*& node) {
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  if (node->parentNode() != parentElement)
    return Response::Error("Anchor node must be child of the target element");
  return Response::OK();
}

Response InspectorDOMAgent::assertEditableElement(int nodeId,
                                                  Element*& element) {
  Response response = assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  if (element->isInShadowTree() && userAgentShadowRoot(element))
    return Response::Error("Cannot edit elements from user-agent shadow trees");

  if (element->isPseudoElement())
    return Response::Error("Cannot edit pseudo elements");

  return Response::OK();
}

void InspectorDOMAgent::innerEnable() {
  m_state->setBoolean(DOMAgentState::domAgentEnabled, true);
  m_history = new InspectorHistory();
  m_domEditor = new DOMEditor(m_history.get());
  m_document = m_inspectedFrames->root()->document();
  m_instrumentingAgents->addInspectorDOMAgent(this);
  if (m_backendNodeIdToInspect)
    frontend()->inspectNodeRequested(m_backendNodeIdToInspect);
  m_backendNodeIdToInspect = 0;
}

Response InspectorDOMAgent::enable() {
  if (!enabled())
    innerEnable();
  return Response::OK();
}

bool InspectorDOMAgent::enabled() const {
  return m_state->booleanProperty(DOMAgentState::domAgentEnabled, false);
}

Response InspectorDOMAgent::disable() {
  if (!enabled())
    return Response::Error("DOM agent hasn't been enabled");
  m_state->setBoolean(DOMAgentState::domAgentEnabled, false);
  setSearchingForNode(NotSearching, Maybe<protocol::DOM::HighlightConfig>());
  m_instrumentingAgents->removeInspectorDOMAgent(this);
  m_history.clear();
  m_domEditor.clear();
  setDocument(nullptr);
  return Response::OK();
}

Response InspectorDOMAgent::getDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* root) {
  // Backward compatibility. Mark agent as enabled when it requests document.
  if (!enabled())
    innerEnable();

  if (!m_document)
    return Response::Error("Document is not available");

  discardFrontendBindings();

  int sanitizedDepth = depth.fromMaybe(2);
  if (sanitizedDepth == -1)
    sanitizedDepth = INT_MAX;

  *root =
      buildObjectForNode(m_document.get(), sanitizedDepth,
                         pierce.fromMaybe(false), m_documentNodeToIdMap.get());
  return Response::OK();
}

void InspectorDOMAgent::pushChildNodesToFrontend(int nodeId,
                                                 int depth,
                                                 bool pierce) {
  Node* node = nodeForId(nodeId);
  if (!node || (!node->isElementNode() && !node->isDocumentNode() &&
                !node->isDocumentFragment()))
    return;

  NodeToIdMap* nodeMap = m_idToNodesMap.get(nodeId);

  if (m_childrenRequested.contains(nodeId)) {
    if (depth <= 1)
      return;

    depth--;

    for (node = innerFirstChild(node); node; node = innerNextSibling(node)) {
      int childNodeId = nodeMap->get(node);
      ASSERT(childNodeId);
      pushChildNodesToFrontend(childNodeId, depth, pierce);
    }

    return;
  }

  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      buildArrayForContainerChildren(node, depth, pierce, nodeMap);
  frontend()->setChildNodes(nodeId, std::move(children));
}

void InspectorDOMAgent::discardFrontendBindings() {
  if (m_history)
    m_history->reset();
  m_searchResults.clear();
  m_documentNodeToIdMap->clear();
  m_idToNode.clear();
  m_idToNodesMap.clear();
  releaseDanglingNodes();
  m_childrenRequested.clear();
  m_cachedChildCount.clear();
  if (m_revalidateTask)
    m_revalidateTask->reset();
}

Node* InspectorDOMAgent::nodeForId(int id) {
  if (!id)
    return nullptr;

  HeapHashMap<int, Member<Node>>::iterator it = m_idToNode.find(id);
  if (it != m_idToNode.end())
    return it->value;
  return nullptr;
}

Response InspectorDOMAgent::collectClassNamesFromSubtree(
    int nodeId,
    std::unique_ptr<protocol::Array<String>>* classNames) {
  HashSet<String> uniqueNames;
  *classNames = protocol::Array<String>::create();
  Node* parentNode = nodeForId(nodeId);
  if (!parentNode ||
      (!parentNode->isElementNode() && !parentNode->isDocumentNode() &&
       !parentNode->isDocumentFragment()))
    return Response::Error("No suitable node with given id found");

  for (Node* node = parentNode; node;
       node = FlatTreeTraversal::next(*node, parentNode)) {
    if (node->isElementNode()) {
      const Element& element = toElement(*node);
      if (!element.hasClass())
        continue;
      const SpaceSplitString& classNameList = element.classNames();
      for (unsigned i = 0; i < classNameList.size(); ++i)
        uniqueNames.add(classNameList[i]);
    }
  }
  for (const String& className : uniqueNames)
    (*classNames)->addItem(className);
  return Response::OK();
}

Response InspectorDOMAgent::requestChildNodes(int nodeId,
                                              Maybe<int> depth,
                                              Maybe<bool> maybeTaverseFrames) {
  int sanitizedDepth = depth.fromMaybe(1);
  if (sanitizedDepth == 0 || sanitizedDepth < -1) {
    return Response::Error(
        "Please provide a positive integer as a depth or -1 for entire "
        "subtree");
  }
  if (sanitizedDepth == -1)
    sanitizedDepth = INT_MAX;

  pushChildNodesToFrontend(nodeId, sanitizedDepth,
                           maybeTaverseFrames.fromMaybe(false));
  return Response::OK();
}

Response InspectorDOMAgent::querySelector(int nodeId,
                                          const String& selectors,
                                          int* elementId) {
  *elementId = 0;
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  if (!node || !node->isContainerNode())
    return Response::Error("Not a container node");

  TrackExceptionState exceptionState;
  Element* element = toContainerNode(node)->querySelector(
      AtomicString(selectors), exceptionState);
  if (exceptionState.hadException())
    return Response::Error("DOM Error while querying");

  if (element)
    *elementId = pushNodePathToFrontend(element);
  return Response::OK();
}

Response InspectorDOMAgent::querySelectorAll(
    int nodeId,
    const String& selectors,
    std::unique_ptr<protocol::Array<int>>* result) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  if (!node || !node->isContainerNode())
    return Response::Error("Not a container node");

  TrackExceptionState exceptionState;
  StaticElementList* elements = toContainerNode(node)->querySelectorAll(
      AtomicString(selectors), exceptionState);
  if (exceptionState.hadException())
    return Response::Error("DOM Error while querying");

  *result = protocol::Array<int>::create();

  for (unsigned i = 0; i < elements->length(); ++i)
    (*result)->addItem(pushNodePathToFrontend(elements->item(i)));
  return Response::OK();
}

int InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush,
                                              NodeToIdMap* nodeMap) {
  ASSERT(nodeToPush);  // Invalid input
  // InspectorDOMAgent might have been resetted already. See crbug.com/450491
  if (!m_document)
    return 0;
  if (!m_documentNodeToIdMap->contains(m_document))
    return 0;

  // Return id in case the node is known.
  int result = nodeMap->get(nodeToPush);
  if (result)
    return result;

  Node* node = nodeToPush;
  HeapVector<Member<Node>> path;

  while (true) {
    Node* parent = innerParentNode(node);
    if (!parent)
      return 0;
    path.append(parent);
    if (nodeMap->get(parent))
      break;
    node = parent;
  }

  for (int i = path.size() - 1; i >= 0; --i) {
    int nodeId = nodeMap->get(path.at(i).get());
    ASSERT(nodeId);
    pushChildNodesToFrontend(nodeId);
  }
  return nodeMap->get(nodeToPush);
}

int InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush) {
  if (!m_document)
    return 0;

  int nodeId = pushNodePathToFrontend(nodeToPush, m_documentNodeToIdMap.get());
  if (nodeId)
    return nodeId;

  Node* node = nodeToPush;
  while (Node* parent = innerParentNode(node))
    node = parent;

  // Node being pushed is detached -> push subtree root.
  NodeToIdMap* newMap = new NodeToIdMap;
  NodeToIdMap* danglingMap = newMap;
  m_danglingNodeToIdMaps.append(newMap);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      protocol::Array<protocol::DOM::Node>::create();
  children->addItem(buildObjectForNode(node, 0, false, danglingMap));
  frontend()->setChildNodes(0, std::move(children));

  return pushNodePathToFrontend(nodeToPush, danglingMap);
}

int InspectorDOMAgent::boundNodeId(Node* node) {
  return m_documentNodeToIdMap->get(node);
}

Response InspectorDOMAgent::setAttributeValue(int elementId,
                                              const String& name,
                                              const String& value) {
  Element* element = nullptr;
  Response response = assertEditableElement(elementId, element);
  if (!response.isSuccess())
    return response;
  return m_domEditor->setAttribute(element, name, value);
}

Response InspectorDOMAgent::setAttributesAsText(int elementId,
                                                const String& text,
                                                Maybe<String> name) {
  Element* element = nullptr;
  Response response = assertEditableElement(elementId, element);
  if (!response.isSuccess())
    return response;

  String markup = "<span " + text + "></span>";
  DocumentFragment* fragment = element->document().createDocumentFragment();

  bool shouldIgnoreCase =
      element->document().isHTMLDocument() && element->isHTMLElement();
  // Not all elements can represent the context (i.e. IFRAME), hence using
  // document.body.
  if (shouldIgnoreCase && element->document().body())
    fragment->parseHTML(markup, element->document().body(),
                        AllowScriptingContent);
  else
    fragment->parseXML(markup, 0, AllowScriptingContent);

  Element* parsedElement =
      fragment->firstChild() && fragment->firstChild()->isElementNode()
          ? toElement(fragment->firstChild())
          : nullptr;
  if (!parsedElement)
    return Response::Error("Could not parse value as attributes");

  String caseAdjustedName =
      shouldIgnoreCase ? name.fromMaybe("").lower() : name.fromMaybe("");

  AttributeCollection attributes = parsedElement->attributes();
  if (attributes.isEmpty() && name.isJust())
    return m_domEditor->removeAttribute(element, caseAdjustedName);

  bool foundOriginalAttribute = false;
  for (auto& attribute : attributes) {
    // Add attribute pair
    String attributeName = attribute.name().toString();
    if (shouldIgnoreCase)
      attributeName = attributeName.lower();
    foundOriginalAttribute |=
        name.isJust() && attributeName == caseAdjustedName;
    Response response =
        m_domEditor->setAttribute(element, attributeName, attribute.value());
    if (!response.isSuccess())
      return response;
  }

  if (!foundOriginalAttribute && name.isJust() &&
      !name.fromJust().stripWhiteSpace().isEmpty()) {
    return m_domEditor->removeAttribute(element, caseAdjustedName);
  }
  return Response::OK();
}

Response InspectorDOMAgent::removeAttribute(int elementId, const String& name) {
  Element* element = nullptr;
  Response response = assertEditableElement(elementId, element);
  if (!response.isSuccess())
    return response;

  return m_domEditor->removeAttribute(element, name);
}

Response InspectorDOMAgent::removeNode(int nodeId) {
  Node* node = nullptr;
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  ContainerNode* parentNode = node->parentNode();
  if (!parentNode)
    return Response::Error("Cannot remove detached node");

  return m_domEditor->removeChild(parentNode, node);
}

Response InspectorDOMAgent::setNodeName(int nodeId,
                                        const String& tagName,
                                        int* newId) {
  *newId = 0;

  Element* oldElement = nullptr;
  Response response = assertElement(nodeId, oldElement);
  if (!response.isSuccess())
    return response;

  TrackExceptionState exceptionState;
  Element* newElem = oldElement->document().createElement(AtomicString(tagName),
                                                          exceptionState);
  if (exceptionState.hadException())
    return toResponse(exceptionState);

  // Copy over the original node's attributes.
  newElem->cloneAttributesFromElement(*oldElement);

  // Copy over the original node's children.
  for (Node* child = oldElement->firstChild(); child;
       child = oldElement->firstChild()) {
    response = m_domEditor->insertBefore(newElem, child, 0);
    if (!response.isSuccess())
      return response;
  }

  // Replace the old node with the new node
  ContainerNode* parent = oldElement->parentNode();
  response =
      m_domEditor->insertBefore(parent, newElem, oldElement->nextSibling());
  if (!response.isSuccess())
    return response;
  response = m_domEditor->removeChild(parent, oldElement);
  if (!response.isSuccess())
    return response;

  *newId = pushNodePathToFrontend(newElem);
  if (m_childrenRequested.contains(nodeId))
    pushChildNodesToFrontend(*newId);
  return Response::OK();
}

Response InspectorDOMAgent::getOuterHTML(int nodeId, WTF::String* outerHTML) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  *outerHTML = createMarkup(node);
  return Response::OK();
}

Response InspectorDOMAgent::setOuterHTML(int nodeId, const String& outerHTML) {
  if (!nodeId) {
    ASSERT(m_document);
    DOMPatchSupport domPatchSupport(m_domEditor.get(), *m_document.get());
    domPatchSupport.patchDocument(outerHTML);
    return Response::OK();
  }

  Node* node = nullptr;
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  Document* document =
      node->isDocumentNode() ? toDocument(node) : node->ownerDocument();
  if (!document || (!document->isHTMLDocument() && !document->isXMLDocument()))
    return Response::Error("Not an HTML/XML document");

  Node* newNode = nullptr;
  response = m_domEditor->setOuterHTML(node, outerHTML, &newNode);
  if (!response.isSuccess())
    return response;

  if (!newNode) {
    // The only child node has been deleted.
    return Response::OK();
  }

  int newId = pushNodePathToFrontend(newNode);

  bool childrenRequested = m_childrenRequested.contains(nodeId);
  if (childrenRequested)
    pushChildNodesToFrontend(newId);
  return Response::OK();
}

Response InspectorDOMAgent::setNodeValue(int nodeId, const String& value) {
  Node* node = nullptr;
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  if (node->getNodeType() != Node::kTextNode)
    return Response::Error("Can only set value of text nodes");

  return m_domEditor->replaceWholeText(toText(node), value);
}

static Node* nextNodeWithShadowDOMInMind(const Node& current,
                                         const Node* stayWithin,
                                         bool includeUserAgentShadowDOM) {
  // At first traverse the subtree.
  if (current.isElementNode()) {
    const Element& element = toElement(current);
    ElementShadow* elementShadow = element.shadow();
    if (elementShadow) {
      ShadowRoot& shadowRoot = elementShadow->youngestShadowRoot();
      if (shadowRoot.type() != ShadowRootType::UserAgent ||
          includeUserAgentShadowDOM)
        return &shadowRoot;
    }
  }
  if (current.hasChildren())
    return current.firstChild();

  // Then traverse siblings of the node itself and its ancestors.
  const Node* node = &current;
  do {
    if (node == stayWithin)
      return nullptr;
    if (node->isShadowRoot()) {
      const ShadowRoot* shadowRoot = toShadowRoot(node);
      if (shadowRoot->olderShadowRoot())
        return shadowRoot->olderShadowRoot();
      Element& host = shadowRoot->host();
      if (host.hasChildren())
        return host.firstChild();
    }
    if (node->nextSibling())
      return node->nextSibling();
    node =
        node->isShadowRoot() ? &toShadowRoot(node)->host() : node->parentNode();
  } while (node);

  return nullptr;
}

Response InspectorDOMAgent::performSearch(
    const String& whitespaceTrimmedQuery,
    Maybe<bool> optionalIncludeUserAgentShadowDOM,
    String* searchId,
    int* resultCount) {
  // FIXME: Few things are missing here:
  // 1) Search works with node granularity - number of matches within node is
  //    not calculated.
  // 2) There is no need to push all search results to the front-end at a time,
  //    pushing next / previous result is sufficient.

  bool includeUserAgentShadowDOM =
      optionalIncludeUserAgentShadowDOM.fromMaybe(false);

  unsigned queryLength = whitespaceTrimmedQuery.length();
  bool startTagFound = !whitespaceTrimmedQuery.find('<');
  bool endTagFound = whitespaceTrimmedQuery.reverseFind('>') + 1 == queryLength;
  bool startQuoteFound = !whitespaceTrimmedQuery.find('"');
  bool endQuoteFound =
      whitespaceTrimmedQuery.reverseFind('"') + 1 == queryLength;
  bool exactAttributeMatch = startQuoteFound && endQuoteFound;

  String tagNameQuery = whitespaceTrimmedQuery;
  String attributeQuery = whitespaceTrimmedQuery;
  if (startTagFound)
    tagNameQuery = tagNameQuery.right(tagNameQuery.length() - 1);
  if (endTagFound)
    tagNameQuery = tagNameQuery.left(tagNameQuery.length() - 1);
  if (startQuoteFound)
    attributeQuery = attributeQuery.right(attributeQuery.length() - 1);
  if (endQuoteFound)
    attributeQuery = attributeQuery.left(attributeQuery.length() - 1);

  HeapVector<Member<Document>> docs = documents();
  HeapListHashSet<Member<Node>> resultCollector;

  for (Document* document : docs) {
    Node* documentElement = document->documentElement();
    Node* node = documentElement;
    if (!node)
      continue;

    // Manual plain text search.
    for (; node; node = nextNodeWithShadowDOMInMind(
                     *node, documentElement, includeUserAgentShadowDOM)) {
      switch (node->getNodeType()) {
        case Node::kTextNode:
        case Node::kCommentNode:
        case Node::kCdataSectionNode: {
          String text = node->nodeValue();
          if (text.findIgnoringCase(whitespaceTrimmedQuery) != kNotFound)
            resultCollector.add(node);
          break;
        }
        case Node::kElementNode: {
          if ((!startTagFound && !endTagFound &&
               (node->nodeName().findIgnoringCase(tagNameQuery) !=
                kNotFound)) ||
              (startTagFound && endTagFound &&
               equalIgnoringCase(node->nodeName(), tagNameQuery)) ||
              (startTagFound && !endTagFound &&
               node->nodeName().startsWith(tagNameQuery,
                                           TextCaseInsensitive)) ||
              (!startTagFound && endTagFound &&
               node->nodeName().endsWith(tagNameQuery, TextCaseInsensitive))) {
            resultCollector.add(node);
            break;
          }
          // Go through all attributes and serialize them.
          const Element* element = toElement(node);
          AttributeCollection attributes = element->attributes();
          for (auto& attribute : attributes) {
            // Add attribute pair
            if (attribute.localName().find(whitespaceTrimmedQuery, 0,
                                           TextCaseInsensitive) != kNotFound) {
              resultCollector.add(node);
              break;
            }
            size_t foundPosition =
                attribute.value().find(attributeQuery, 0, TextCaseInsensitive);
            if (foundPosition != kNotFound) {
              if (!exactAttributeMatch ||
                  (!foundPosition &&
                   attribute.value().length() == attributeQuery.length())) {
                resultCollector.add(node);
                break;
              }
            }
          }
          break;
        }
        default:
          break;
      }
    }

    // XPath evaluation
    for (Document* document : docs) {
      ASSERT(document);
      TrackExceptionState exceptionState;
      XPathResult* result = DocumentXPathEvaluator::evaluate(
          *document, whitespaceTrimmedQuery, document, nullptr,
          XPathResult::kOrderedNodeSnapshotType, ScriptValue(), exceptionState);
      if (exceptionState.hadException() || !result)
        continue;

      unsigned long size = result->snapshotLength(exceptionState);
      for (unsigned long i = 0; !exceptionState.hadException() && i < size;
           ++i) {
        Node* node = result->snapshotItem(i, exceptionState);
        if (exceptionState.hadException())
          break;

        if (node->getNodeType() == Node::kAttributeNode)
          node = toAttr(node)->ownerElement();
        resultCollector.add(node);
      }
    }

    // Selector evaluation
    for (Document* document : docs) {
      TrackExceptionState exceptionState;
      StaticElementList* elementList = document->querySelectorAll(
          AtomicString(whitespaceTrimmedQuery), exceptionState);
      if (exceptionState.hadException() || !elementList)
        continue;

      unsigned size = elementList->length();
      for (unsigned i = 0; i < size; ++i)
        resultCollector.add(elementList->item(i));
    }
  }

  *searchId = IdentifiersFactory::createIdentifier();
  HeapVector<Member<Node>>* resultsIt =
      &m_searchResults.add(*searchId, HeapVector<Member<Node>>())
           .storedValue->value;

  for (auto& result : resultCollector)
    resultsIt->append(result);

  *resultCount = resultsIt->size();
  return Response::OK();
}

Response InspectorDOMAgent::getSearchResults(
    const String& searchId,
    int fromIndex,
    int toIndex,
    std::unique_ptr<protocol::Array<int>>* nodeIds) {
  SearchResults::iterator it = m_searchResults.find(searchId);
  if (it == m_searchResults.end())
    return Response::Error("No search session with given id found");

  int size = it->value.size();
  if (fromIndex < 0 || toIndex > size || fromIndex >= toIndex)
    return Response::Error("Invalid search result range");

  *nodeIds = protocol::Array<int>::create();
  for (int i = fromIndex; i < toIndex; ++i)
    (*nodeIds)->addItem(pushNodePathToFrontend((it->value)[i].get()));
  return Response::OK();
}

Response InspectorDOMAgent::discardSearchResults(const String& searchId) {
  m_searchResults.remove(searchId);
  return Response::OK();
}

void InspectorDOMAgent::inspect(Node* inspectedNode) {
  if (!inspectedNode)
    return;

  Node* node = inspectedNode;
  while (node && !node->isElementNode() && !node->isDocumentNode() &&
         !node->isDocumentFragment())
    node = node->parentOrShadowHostNode();
  if (!node)
    return;

  int backendNodeId = DOMNodeIds::idForNode(node);
  if (!frontend() || !enabled()) {
    m_backendNodeIdToInspect = backendNodeId;
    return;
  }

  frontend()->inspectNodeRequested(backendNodeId);
}

void InspectorDOMAgent::nodeHighlightedInOverlay(Node* node) {
  if (!frontend() || !enabled())
    return;

  while (node && !node->isElementNode() && !node->isDocumentNode() &&
         !node->isDocumentFragment())
    node = node->parentOrShadowHostNode();

  if (!node)
    return;

  int nodeId = pushNodePathToFrontend(node);
  frontend()->nodeHighlightRequested(nodeId);
}

Response InspectorDOMAgent::setSearchingForNode(
    SearchMode searchMode,
    Maybe<protocol::DOM::HighlightConfig> highlightInspectorObject) {
  if (!m_client)
    return Response::OK();
  if (searchMode == NotSearching) {
    m_client->setInspectMode(NotSearching, nullptr);
    return Response::OK();
  }
  std::unique_ptr<InspectorHighlightConfig> config;
  Response response = highlightConfigFromInspectorObject(
      std::move(highlightInspectorObject), &config);
  if (!response.isSuccess())
    return response;
  m_client->setInspectMode(searchMode, std::move(config));
  return Response::OK();
}

Response InspectorDOMAgent::highlightConfigFromInspectorObject(
    Maybe<protocol::DOM::HighlightConfig> highlightInspectorObject,
    std::unique_ptr<InspectorHighlightConfig>* outConfig) {
  if (!highlightInspectorObject.isJust()) {
    return Response::Error(
        "Internal error: highlight configuration parameter is missing");
  }

  protocol::DOM::HighlightConfig* config = highlightInspectorObject.fromJust();
  std::unique_ptr<InspectorHighlightConfig> highlightConfig =
      makeUnique<InspectorHighlightConfig>();
  highlightConfig->showInfo = config->getShowInfo(false);
  highlightConfig->showRulers = config->getShowRulers(false);
  highlightConfig->showExtensionLines = config->getShowExtensionLines(false);
  highlightConfig->displayAsMaterial = config->getDisplayAsMaterial(false);
  highlightConfig->content = parseColor(config->getContentColor(nullptr));
  highlightConfig->padding = parseColor(config->getPaddingColor(nullptr));
  highlightConfig->border = parseColor(config->getBorderColor(nullptr));
  highlightConfig->margin = parseColor(config->getMarginColor(nullptr));
  highlightConfig->eventTarget =
      parseColor(config->getEventTargetColor(nullptr));
  highlightConfig->shape = parseColor(config->getShapeColor(nullptr));
  highlightConfig->shapeMargin =
      parseColor(config->getShapeMarginColor(nullptr));
  highlightConfig->selectorList = config->getSelectorList("");

  *outConfig = std::move(highlightConfig);
  return Response::OK();
}

Response InspectorDOMAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::DOM::HighlightConfig> highlightConfig) {
  SearchMode searchMode;
  if (mode == protocol::DOM::InspectModeEnum::SearchForNode) {
    searchMode = SearchingForNormal;
  } else if (mode == protocol::DOM::InspectModeEnum::SearchForUAShadowDOM) {
    searchMode = SearchingForUAShadow;
  } else if (mode == protocol::DOM::InspectModeEnum::None) {
    searchMode = NotSearching;
  } else if (mode == protocol::DOM::InspectModeEnum::ShowLayoutEditor) {
    searchMode = ShowLayoutEditor;
  } else {
    return Response::Error(
        String("Unknown mode \"" + mode + "\" was provided."));
  }

  if (searchMode != NotSearching) {
    Response response = pushDocumentUponHandlelessOperation();
    if (!response.isSuccess())
      return response;
  }

  return setSearchingForNode(searchMode, std::move(highlightConfig));
}

Response InspectorDOMAgent::highlightRect(
    int x,
    int y,
    int width,
    int height,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outlineColor) {
  std::unique_ptr<FloatQuad> quad =
      wrapUnique(new FloatQuad(FloatRect(x, y, width, height)));
  innerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outlineColor));
  return Response::OK();
}

Response InspectorDOMAgent::highlightQuad(
    std::unique_ptr<protocol::Array<double>> quadArray,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outlineColor) {
  std::unique_ptr<FloatQuad> quad = makeUnique<FloatQuad>();
  if (!parseQuad(std::move(quadArray), quad.get()))
    return Response::Error("Invalid Quad format");
  innerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outlineColor));
  return Response::OK();
}

void InspectorDOMAgent::innerHighlightQuad(
    std::unique_ptr<FloatQuad> quad,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outlineColor) {
  std::unique_ptr<InspectorHighlightConfig> highlightConfig =
      makeUnique<InspectorHighlightConfig>();
  highlightConfig->content = parseColor(color.fromMaybe(nullptr));
  highlightConfig->contentOutline = parseColor(outlineColor.fromMaybe(nullptr));
  if (m_client)
    m_client->highlightQuad(std::move(quad), *highlightConfig);
}

Response InspectorDOMAgent::nodeForRemoteId(const String& objectId,
                                            Node*& node) {
  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!m_v8Session->unwrapObject(&error, toV8InspectorStringView(objectId),
                                 &value, &context, nullptr))
    return Response::Error(toCoreString(std::move(error)));
  if (!V8Node::hasInstance(value, m_isolate))
    return Response::Error("Object id doesn't reference a Node");
  node = V8Node::toImpl(v8::Local<v8::Object>::Cast(value));
  if (!node) {
    return Response::Error(
        "Couldn't convert object with given objectId to Node");
  }
  return Response::OK();
}

Response InspectorDOMAgent::highlightNode(
    std::unique_ptr<protocol::DOM::HighlightConfig> highlightInspectorObject,
    Maybe<int> nodeId,
    Maybe<int> backendNodeId,
    Maybe<String> objectId) {
  Node* node = nullptr;
  Response response;
  if (nodeId.isJust()) {
    response = assertNode(nodeId.fromJust(), node);
  } else if (backendNodeId.isJust()) {
    node = DOMNodeIds::nodeForId(backendNodeId.fromJust());
    response = !node ? Response::Error("No node found for given backend id")
                     : Response::OK();
  } else if (objectId.isJust()) {
    response = nodeForRemoteId(objectId.fromJust(), node);
  } else {
    response = Response::Error("Either nodeId or objectId must be specified");
  }

  if (!response.isSuccess())
    return response;

  std::unique_ptr<InspectorHighlightConfig> highlightConfig;
  response = highlightConfigFromInspectorObject(
      std::move(highlightInspectorObject), &highlightConfig);
  if (!response.isSuccess())
    return response;

  if (m_client)
    m_client->highlightNode(node, *highlightConfig, false);
  return Response::OK();
}

Response InspectorDOMAgent::highlightFrame(
    const String& frameId,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outlineColor) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  // FIXME: Inspector doesn't currently work cross process.
  if (frame && frame->deprecatedLocalOwner()) {
    std::unique_ptr<InspectorHighlightConfig> highlightConfig =
        makeUnique<InspectorHighlightConfig>();
    highlightConfig->showInfo = true;  // Always show tooltips for frames.
    highlightConfig->content = parseColor(color.fromMaybe(nullptr));
    highlightConfig->contentOutline =
        parseColor(outlineColor.fromMaybe(nullptr));
    if (m_client)
      m_client->highlightNode(frame->deprecatedLocalOwner(), *highlightConfig,
                              false);
  }
  return Response::OK();
}

Response InspectorDOMAgent::hideHighlight() {
  if (m_client)
    m_client->hideHighlight();
  return Response::OK();
}

Response InspectorDOMAgent::copyTo(int nodeId,
                                   int targetElementId,
                                   Maybe<int> anchorNodeId,
                                   int* newNodeId) {
  Node* node = nullptr;
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  Element* targetElement = nullptr;
  response = assertEditableElement(targetElementId, targetElement);
  if (!response.isSuccess())
    return response;

  Node* anchorNode = nullptr;
  if (anchorNodeId.isJust() && anchorNodeId.fromJust()) {
    response = assertEditableChildNode(targetElement, anchorNodeId.fromJust(),
                                       anchorNode);
    if (!response.isSuccess())
      return response;
  }

  // The clone is deep by default.
  Node* clonedNode = node->cloneNode(true);
  if (!clonedNode)
    return Response::Error("Failed to clone node");
  response = m_domEditor->insertBefore(targetElement, clonedNode, anchorNode);
  if (!response.isSuccess())
    return response;

  *newNodeId = pushNodePathToFrontend(clonedNode);
  return Response::OK();
}

Response InspectorDOMAgent::moveTo(int nodeId,
                                   int targetElementId,
                                   Maybe<int> anchorNodeId,
                                   int* newNodeId) {
  Node* node = nullptr;
  Response response = assertEditableNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  Element* targetElement = nullptr;
  response = assertEditableElement(targetElementId, targetElement);
  if (!response.isSuccess())
    return response;

  Node* current = targetElement;
  while (current) {
    if (current == node)
      return Response::Error("Unable to move node into self or descendant");
    current = current->parentNode();
  }

  Node* anchorNode = nullptr;
  if (anchorNodeId.isJust() && anchorNodeId.fromJust()) {
    response = assertEditableChildNode(targetElement, anchorNodeId.fromJust(),
                                       anchorNode);
    if (!response.isSuccess())
      return response;
  }

  response = m_domEditor->insertBefore(targetElement, node, anchorNode);
  if (!response.isSuccess())
    return response;

  *newNodeId = pushNodePathToFrontend(node);
  return Response::OK();
}

Response InspectorDOMAgent::undo() {
  TrackExceptionState exceptionState;
  m_history->undo(exceptionState);
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorDOMAgent::redo() {
  TrackExceptionState exceptionState;
  m_history->redo(exceptionState);
  return InspectorDOMAgent::toResponse(exceptionState);
}

Response InspectorDOMAgent::markUndoableState() {
  m_history->markUndoableState();
  return Response::OK();
}

Response InspectorDOMAgent::focus(int nodeId) {
  Element* element = nullptr;
  Response response = assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  element->document().updateStyleAndLayoutIgnorePendingStylesheets();
  if (!element->isFocusable())
    return Response::Error("Element is not focusable");
  element->focus();
  return Response::OK();
}

Response InspectorDOMAgent::setFileInputFiles(
    int nodeId,
    std::unique_ptr<protocol::Array<String>> files) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  if (!isHTMLInputElement(*node) ||
      toHTMLInputElement(*node).type() != InputTypeNames::file)
    return Response::Error("Node is not a file input element");

  Vector<String> paths;
  for (size_t index = 0; index < files->length(); ++index)
    paths.append(files->get(index));
  toHTMLInputElement(node)->setFilesFromPaths(paths);
  return Response::OK();
}

Response InspectorDOMAgent::getBoxModel(
    int nodeId,
    std::unique_ptr<protocol::DOM::BoxModel>* model) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  bool result = InspectorHighlight::getBoxModel(node, model);
  if (!result)
    return Response::Error("Could not compute box model.");
  return Response::OK();
}

Response InspectorDOMAgent::getNodeForLocation(int x, int y, int* nodeId) {
  Response response = pushDocumentUponHandlelessOperation();
  if (!response.isSuccess())
    return response;
  HitTestRequest request(HitTestRequest::Move | HitTestRequest::ReadOnly |
                         HitTestRequest::AllowChildFrameContent);
  HitTestResult result(request, IntPoint(x, y));
  m_document->frame()->contentLayoutItem().hitTest(result);
  Node* node = result.innerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  if (!node)
    return Response::Error("No node found at given location");
  *nodeId = pushNodePathToFrontend(node);
  return Response::OK();
}

Response InspectorDOMAgent::resolveNode(
    int nodeId,
    Maybe<String> objectGroup,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  String objectGroupName = objectGroup.fromMaybe("");
  Node* node = nodeForId(nodeId);
  if (!node)
    return Response::Error("No node with given id found");
  *result = resolveNode(node, objectGroupName);
  if (!*result) {
    return Response::Error(
        "Node with given id does not belong to the document");
  }
  return Response::OK();
}

Response InspectorDOMAgent::getAttributes(
    int nodeId,
    std::unique_ptr<protocol::Array<String>>* result) {
  Element* element = nullptr;
  Response response = assertElement(nodeId, element);
  if (!response.isSuccess())
    return response;

  *result = buildArrayForElementAttributes(element);
  return Response::OK();
}

Response InspectorDOMAgent::requestNode(const String& objectId, int* nodeId) {
  Node* node = nullptr;
  Response response = nodeForRemoteId(objectId, node);
  if (!response.isSuccess())
    return response;
  *nodeId = pushNodePathToFrontend(node);
  return Response::OK();
}

// static
String InspectorDOMAgent::documentURLString(Document* document) {
  if (!document || document->url().isNull())
    return "";
  return document->url().getString();
}

static String documentBaseURLString(Document* document) {
  return document->baseURLForOverride(document->baseURL()).getString();
}

static protocol::DOM::ShadowRootType shadowRootType(ShadowRoot* shadowRoot) {
  switch (shadowRoot->type()) {
    case ShadowRootType::UserAgent:
      return protocol::DOM::ShadowRootTypeEnum::UserAgent;
    case ShadowRootType::V0:
    case ShadowRootType::Open:
      return protocol::DOM::ShadowRootTypeEnum::Open;
    case ShadowRootType::Closed:
      return protocol::DOM::ShadowRootTypeEnum::Closed;
  }
  ASSERT_NOT_REACHED();
  return protocol::DOM::ShadowRootTypeEnum::UserAgent;
}

std::unique_ptr<protocol::DOM::Node> InspectorDOMAgent::buildObjectForNode(
    Node* node,
    int depth,
    bool pierce,
    NodeToIdMap* nodesMap) {
  int id = bind(node, nodesMap);
  String localName;
  String nodeValue;

  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
      nodeValue = node->nodeValue();
      if (nodeValue.length() > maxTextSize)
        nodeValue = nodeValue.left(maxTextSize) + ellipsisUChar;
      break;
    case Node::kAttributeNode:
      localName = toAttr(node)->localName();
      break;
    case Node::kElementNode:
      localName = toElement(node)->localName();
      break;
    default:
      break;
  }

  std::unique_ptr<protocol::DOM::Node> value =
      protocol::DOM::Node::create()
          .setNodeId(id)
          .setBackendNodeId(DOMNodeIds::idForNode(node))
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setLocalName(localName)
          .setNodeValue(nodeValue)
          .build();

  bool forcePushChildren = false;
  if (node->isElementNode()) {
    Element* element = toElement(node);
    value->setAttributes(buildArrayForElementAttributes(element));

    if (node->isFrameOwnerElement()) {
      HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(node);
      if (LocalFrame* frame = frameOwner->contentFrame() &&
                                      frameOwner->contentFrame()->isLocalFrame()
                                  ? toLocalFrame(frameOwner->contentFrame())
                                  : nullptr)
        value->setFrameId(IdentifiersFactory::frameId(frame));
      if (Document* doc = frameOwner->contentDocument()) {
        value->setContentDocument(
            buildObjectForNode(doc, pierce ? depth : 0, pierce, nodesMap));
      }
    }

    if (node->parentNode() && node->parentNode()->isDocumentNode()) {
      LocalFrame* frame = node->document().frame();
      if (frame)
        value->setFrameId(IdentifiersFactory::frameId(frame));
    }

    ElementShadow* shadow = element->shadow();
    if (shadow) {
      std::unique_ptr<protocol::Array<protocol::DOM::Node>> shadowRoots =
          protocol::Array<protocol::DOM::Node>::create();
      for (ShadowRoot* root = &shadow->youngestShadowRoot(); root;
           root = root->olderShadowRoot()) {
        shadowRoots->addItem(
            buildObjectForNode(root, pierce ? depth : 0, pierce, nodesMap));
      }
      value->setShadowRoots(std::move(shadowRoots));
      forcePushChildren = true;
    }

    if (isHTMLLinkElement(*element)) {
      HTMLLinkElement& linkElement = toHTMLLinkElement(*element);
      if (linkElement.isImport() && linkElement.import() &&
          innerParentNode(linkElement.import()) == linkElement) {
        value->setImportedDocument(
            buildObjectForNode(linkElement.import(), 0, pierce, nodesMap));
      }
      forcePushChildren = true;
    }

    if (isHTMLTemplateElement(*element)) {
      value->setTemplateContent(buildObjectForNode(
          toHTMLTemplateElement(*element).content(), 0, pierce, nodesMap));
      forcePushChildren = true;
    }

    if (element->getPseudoId()) {
      protocol::DOM::PseudoType pseudoType;
      if (InspectorDOMAgent::getPseudoElementType(element->getPseudoId(),
                                                  &pseudoType))
        value->setPseudoType(pseudoType);
    } else {
      std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudoElements =
          buildArrayForPseudoElements(element, nodesMap);
      if (pseudoElements) {
        value->setPseudoElements(std::move(pseudoElements));
        forcePushChildren = true;
      }
      if (!element->ownerDocument()->xmlVersion().isEmpty())
        value->setXmlVersion(element->ownerDocument()->xmlVersion());
    }

    if (element->isInsertionPoint()) {
      value->setDistributedNodes(
          buildArrayForDistributedNodes(toInsertionPoint(element)));
      forcePushChildren = true;
    }
    if (isHTMLSlotElement(*element)) {
      value->setDistributedNodes(
          buildDistributedNodesForSlot(toHTMLSlotElement(element)));
      forcePushChildren = true;
    }
  } else if (node->isDocumentNode()) {
    Document* document = toDocument(node);
    value->setDocumentURL(documentURLString(document));
    value->setBaseURL(documentBaseURLString(document));
    value->setXmlVersion(document->xmlVersion());
  } else if (node->isDocumentTypeNode()) {
    DocumentType* docType = toDocumentType(node);
    value->setPublicId(docType->publicId());
    value->setSystemId(docType->systemId());
  } else if (node->isAttributeNode()) {
    Attr* attribute = toAttr(node);
    value->setName(attribute->name());
    value->setValue(attribute->value());
  } else if (node->isShadowRoot()) {
    value->setShadowRootType(shadowRootType(toShadowRoot(node)));
  }

  if (node->isContainerNode()) {
    int nodeCount = innerChildNodeCount(node);
    value->setChildNodeCount(nodeCount);
    if (nodesMap == m_documentNodeToIdMap)
      m_cachedChildCount.set(id, nodeCount);
    if (forcePushChildren && !depth)
      depth = 1;
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
        buildArrayForContainerChildren(node, depth, pierce, nodesMap);
    if (children->length() > 0 ||
        depth)  // Push children along with shadow in any case.
      value->setChildren(std::move(children));
  }

  return value;
}

std::unique_ptr<protocol::Array<String>>
InspectorDOMAgent::buildArrayForElementAttributes(Element* element) {
  std::unique_ptr<protocol::Array<String>> attributesValue =
      protocol::Array<String>::create();
  // Go through all attributes and serialize them.
  AttributeCollection attributes = element->attributes();
  for (auto& attribute : attributes) {
    // Add attribute pair
    attributesValue->addItem(attribute.name().toString());
    attributesValue->addItem(attribute.value());
  }
  return attributesValue;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::buildArrayForContainerChildren(Node* container,
                                                  int depth,
                                                  bool pierce,
                                                  NodeToIdMap* nodesMap) {
  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      protocol::Array<protocol::DOM::Node>::create();
  if (depth == 0) {
    // Special-case the only text child - pretend that container's children have
    // been requested.
    Node* firstChild = container->firstChild();
    if (firstChild && firstChild->getNodeType() == Node::kTextNode &&
        !firstChild->nextSibling()) {
      children->addItem(buildObjectForNode(firstChild, 0, pierce, nodesMap));
      m_childrenRequested.add(bind(container, nodesMap));
    }
    return children;
  }

  Node* child = innerFirstChild(container);
  depth--;
  m_childrenRequested.add(bind(container, nodesMap));

  while (child) {
    children->addItem(buildObjectForNode(child, depth, pierce, nodesMap));
    child = innerNextSibling(child);
  }
  return children;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::buildArrayForPseudoElements(Element* element,
                                               NodeToIdMap* nodesMap) {
  if (!element->pseudoElement(PseudoIdBefore) &&
      !element->pseudoElement(PseudoIdAfter))
    return nullptr;

  std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudoElements =
      protocol::Array<protocol::DOM::Node>::create();
  if (element->pseudoElement(PseudoIdBefore)) {
    pseudoElements->addItem(buildObjectForNode(
        element->pseudoElement(PseudoIdBefore), 0, false, nodesMap));
  }
  if (element->pseudoElement(PseudoIdAfter)) {
    pseudoElements->addItem(buildObjectForNode(
        element->pseudoElement(PseudoIdAfter), 0, false, nodesMap));
  }
  return pseudoElements;
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::buildArrayForDistributedNodes(
    InsertionPoint* insertionPoint) {
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
      distributedNodes = protocol::Array<protocol::DOM::BackendNode>::create();
  for (size_t i = 0; i < insertionPoint->distributedNodesSize(); ++i) {
    Node* distributedNode = insertionPoint->distributedNodeAt(i);
    if (isWhitespace(distributedNode))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backendNode =
        protocol::DOM::BackendNode::create()
            .setNodeType(distributedNode->getNodeType())
            .setNodeName(distributedNode->nodeName())
            .setBackendNodeId(DOMNodeIds::idForNode(distributedNode))
            .build();
    distributedNodes->addItem(std::move(backendNode));
  }
  return distributedNodes;
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::buildDistributedNodesForSlot(HTMLSlotElement* slotElement) {
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
      distributedNodes = protocol::Array<protocol::DOM::BackendNode>::create();
  for (Node* node = slotElement->firstDistributedNode(); node;
       node = slotElement->distributedNodeNextTo(*node)) {
    if (isWhitespace(node))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backendNode =
        protocol::DOM::BackendNode::create()
            .setNodeType(node->getNodeType())
            .setNodeName(node->nodeName())
            .setBackendNodeId(DOMNodeIds::idForNode(node))
            .build();
    distributedNodes->addItem(std::move(backendNode));
  }
  return distributedNodes;
}

Node* InspectorDOMAgent::innerFirstChild(Node* node) {
  node = node->firstChild();
  while (isWhitespace(node))
    node = node->nextSibling();
  return node;
}

Node* InspectorDOMAgent::innerNextSibling(Node* node) {
  do {
    node = node->nextSibling();
  } while (isWhitespace(node));
  return node;
}

Node* InspectorDOMAgent::innerPreviousSibling(Node* node) {
  do {
    node = node->previousSibling();
  } while (isWhitespace(node));
  return node;
}

unsigned InspectorDOMAgent::innerChildNodeCount(Node* node) {
  unsigned count = 0;
  Node* child = innerFirstChild(node);
  while (child) {
    count++;
    child = innerNextSibling(child);
  }
  return count;
}

Node* InspectorDOMAgent::innerParentNode(Node* node) {
  if (node->isDocumentNode()) {
    Document* document = toDocument(node);
    if (HTMLImportLoader* loader = document->importLoader())
      return loader->firstImport()->link();
    return document->localOwner();
  }
  return node->parentOrShadowHostNode();
}

bool InspectorDOMAgent::isWhitespace(Node* node) {
  // TODO: pull ignoreWhitespace setting from the frontend and use here.
  return node && node->getNodeType() == Node::kTextNode &&
         node->nodeValue().stripWhiteSpace().length() == 0;
}

void InspectorDOMAgent::domContentLoadedEventFired(LocalFrame* frame) {
  if (frame != m_inspectedFrames->root())
    return;

  // Re-push document once it is loaded.
  discardFrontendBindings();
  if (enabled())
    frontend()->documentUpdated();
}

void InspectorDOMAgent::invalidateFrameOwnerElement(LocalFrame* frame) {
  HTMLFrameOwnerElement* frameOwner = frame->document()->localOwner();
  if (!frameOwner)
    return;

  int frameOwnerId = m_documentNodeToIdMap->get(frameOwner);
  if (!frameOwnerId)
    return;

  // Re-add frame owner element together with its new children.
  int parentId = m_documentNodeToIdMap->get(innerParentNode(frameOwner));
  frontend()->childNodeRemoved(parentId, frameOwnerId);
  unbind(frameOwner, m_documentNodeToIdMap.get());

  std::unique_ptr<protocol::DOM::Node> value =
      buildObjectForNode(frameOwner, 0, false, m_documentNodeToIdMap.get());
  Node* previousSibling = innerPreviousSibling(frameOwner);
  int prevId =
      previousSibling ? m_documentNodeToIdMap->get(previousSibling) : 0;
  frontend()->childNodeInserted(parentId, prevId, std::move(value));
}

void InspectorDOMAgent::didCommitLoad(LocalFrame*, DocumentLoader* loader) {
  Document* document = loader->frame()->document();
  if (m_domListener)
    m_domListener->didAddDocument(document);

  LocalFrame* inspectedFrame = m_inspectedFrames->root();
  if (loader->frame() != inspectedFrame) {
    invalidateFrameOwnerElement(loader->frame());
    return;
  }

  setDocument(inspectedFrame->document());
}

void InspectorDOMAgent::didInsertDOMNode(Node* node) {
  if (isWhitespace(node))
    return;

  // We could be attaching existing subtree. Forget the bindings.
  unbind(node, m_documentNodeToIdMap.get());

  ContainerNode* parent = node->parentNode();
  if (!parent)
    return;
  int parentId = m_documentNodeToIdMap->get(parent);
  // Return if parent is not mapped yet.
  if (!parentId)
    return;

  if (!m_childrenRequested.contains(parentId)) {
    // No children are mapped yet -> only notify on changes of child count.
    int count = m_cachedChildCount.get(parentId) + 1;
    m_cachedChildCount.set(parentId, count);
    frontend()->childNodeCountUpdated(parentId, count);
  } else {
    // Children have been requested -> return value of a new child.
    Node* prevSibling = innerPreviousSibling(node);
    int prevId = prevSibling ? m_documentNodeToIdMap->get(prevSibling) : 0;
    std::unique_ptr<protocol::DOM::Node> value =
        buildObjectForNode(node, 0, false, m_documentNodeToIdMap.get());
    frontend()->childNodeInserted(parentId, prevId, std::move(value));
  }
}

void InspectorDOMAgent::willRemoveDOMNode(Node* node) {
  if (isWhitespace(node))
    return;

  ContainerNode* parent = node->parentNode();

  // If parent is not mapped yet -> ignore the event.
  if (!m_documentNodeToIdMap->contains(parent))
    return;

  int parentId = m_documentNodeToIdMap->get(parent);

  if (!m_childrenRequested.contains(parentId)) {
    // No children are mapped yet -> only notify on changes of child count.
    int count = m_cachedChildCount.get(parentId) - 1;
    m_cachedChildCount.set(parentId, count);
    frontend()->childNodeCountUpdated(parentId, count);
  } else {
    frontend()->childNodeRemoved(parentId, m_documentNodeToIdMap->get(node));
  }
  unbind(node, m_documentNodeToIdMap.get());
}

void InspectorDOMAgent::willModifyDOMAttr(Element*,
                                          const AtomicString& oldValue,
                                          const AtomicString& newValue) {
  m_suppressAttributeModifiedEvent = (oldValue == newValue);
}

void InspectorDOMAgent::didModifyDOMAttr(Element* element,
                                         const QualifiedName& name,
                                         const AtomicString& value) {
  bool shouldSuppressEvent = m_suppressAttributeModifiedEvent;
  m_suppressAttributeModifiedEvent = false;
  if (shouldSuppressEvent)
    return;

  int id = boundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  if (m_domListener)
    m_domListener->didModifyDOMAttr(element);

  frontend()->attributeModified(id, name.toString(), value);
}

void InspectorDOMAgent::didRemoveDOMAttr(Element* element,
                                         const QualifiedName& name) {
  int id = boundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  if (m_domListener)
    m_domListener->didModifyDOMAttr(element);

  frontend()->attributeRemoved(id, name.toString());
}

void InspectorDOMAgent::styleAttributeInvalidated(
    const HeapVector<Member<Element>>& elements) {
  std::unique_ptr<protocol::Array<int>> nodeIds =
      protocol::Array<int>::create();
  for (unsigned i = 0, size = elements.size(); i < size; ++i) {
    Element* element = elements.at(i);
    int id = boundNodeId(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
      continue;

    if (m_domListener)
      m_domListener->didModifyDOMAttr(element);
    nodeIds->addItem(id);
  }
  frontend()->inlineStyleInvalidated(std::move(nodeIds));
}

void InspectorDOMAgent::characterDataModified(CharacterData* characterData) {
  int id = m_documentNodeToIdMap->get(characterData);
  if (!id) {
    // Push text node if it is being created.
    didInsertDOMNode(characterData);
    return;
  }
  frontend()->characterDataModified(id, characterData->data());
}

Member<InspectorRevalidateDOMTask> InspectorDOMAgent::revalidateTask() {
  if (!m_revalidateTask)
    m_revalidateTask = new InspectorRevalidateDOMTask(this);
  return m_revalidateTask.get();
}

void InspectorDOMAgent::didInvalidateStyleAttr(Node* node) {
  int id = m_documentNodeToIdMap->get(node);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  revalidateTask()->scheduleStyleAttrRevalidationFor(toElement(node));
}

void InspectorDOMAgent::didPushShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int hostId = m_documentNodeToIdMap->get(host);
  if (!hostId)
    return;

  pushChildNodesToFrontend(hostId, 1);
  frontend()->shadowRootPushed(
      hostId, buildObjectForNode(root, 0, false, m_documentNodeToIdMap.get()));
}

void InspectorDOMAgent::willPopShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int hostId = m_documentNodeToIdMap->get(host);
  int rootId = m_documentNodeToIdMap->get(root);
  if (hostId && rootId)
    frontend()->shadowRootPopped(hostId, rootId);
}

void InspectorDOMAgent::didPerformElementShadowDistribution(
    Element* shadowHost) {
  int shadowHostId = m_documentNodeToIdMap->get(shadowHost);
  if (!shadowHostId)
    return;

  for (ShadowRoot* root = shadowHost->youngestShadowRoot(); root;
       root = root->olderShadowRoot()) {
    const HeapVector<Member<InsertionPoint>>& insertionPoints =
        root->descendantInsertionPoints();
    for (const auto& it : insertionPoints) {
      InsertionPoint* insertionPoint = it.get();
      int insertionPointId = m_documentNodeToIdMap->get(insertionPoint);
      if (insertionPointId)
        frontend()->distributedNodesUpdated(
            insertionPointId, buildArrayForDistributedNodes(insertionPoint));
    }
  }
}

void InspectorDOMAgent::didPerformSlotDistribution(
    HTMLSlotElement* slotElement) {
  int insertionPointId = m_documentNodeToIdMap->get(slotElement);
  if (insertionPointId)
    frontend()->distributedNodesUpdated(
        insertionPointId, buildDistributedNodesForSlot(slotElement));
}

void InspectorDOMAgent::frameDocumentUpdated(LocalFrame* frame) {
  Document* document = frame->document();
  if (!document)
    return;

  if (frame != m_inspectedFrames->root())
    return;

  // Only update the main frame document, nested frame document updates are not
  // required (will be handled by invalidateFrameOwnerElement()).
  setDocument(document);
}

void InspectorDOMAgent::pseudoElementCreated(PseudoElement* pseudoElement) {
  Element* parent = pseudoElement->parentOrShadowHostElement();
  if (!parent)
    return;
  int parentId = m_documentNodeToIdMap->get(parent);
  if (!parentId)
    return;

  pushChildNodesToFrontend(parentId, 1);
  frontend()->pseudoElementAdded(
      parentId,
      buildObjectForNode(pseudoElement, 0, false, m_documentNodeToIdMap.get()));
}

void InspectorDOMAgent::pseudoElementDestroyed(PseudoElement* pseudoElement) {
  int pseudoElementId = m_documentNodeToIdMap->get(pseudoElement);
  if (!pseudoElementId)
    return;

  // If a PseudoElement is bound, its parent element must be bound, too.
  Element* parent = pseudoElement->parentOrShadowHostElement();
  ASSERT(parent);
  int parentId = m_documentNodeToIdMap->get(parent);
  ASSERT(parentId);

  unbind(pseudoElement, m_documentNodeToIdMap.get());
  frontend()->pseudoElementRemoved(parentId, pseudoElementId);
}

static ShadowRoot* shadowRootForNode(Node* node, const String& type) {
  if (!node->isElementNode())
    return nullptr;
  if (type == "a")
    return toElement(node)->authorShadowRoot();
  if (type == "u")
    return toElement(node)->userAgentShadowRoot();
  return nullptr;
}

Node* InspectorDOMAgent::nodeForPath(const String& path) {
  // The path is of form "1,HTML,2,BODY,1,DIV" (<index> and <nodeName>
  // interleaved).  <index> may also be "a" (author shadow root) or "u"
  // (user-agent shadow root), in which case <nodeName> MUST be
  // "#document-fragment".
  if (!m_document)
    return nullptr;

  Node* node = m_document.get();
  Vector<String> pathTokens;
  path.split(',', pathTokens);
  if (!pathTokens.size())
    return nullptr;

  for (size_t i = 0; i < pathTokens.size() - 1; i += 2) {
    bool success = true;
    String& indexValue = pathTokens[i];
    unsigned childNumber = indexValue.toUInt(&success);
    Node* child;
    if (!success) {
      child = shadowRootForNode(node, indexValue);
    } else {
      if (childNumber >= innerChildNodeCount(node))
        return nullptr;

      child = innerFirstChild(node);
    }
    String childName = pathTokens[i + 1];
    for (size_t j = 0; child && j < childNumber; ++j)
      child = innerNextSibling(child);

    if (!child || child->nodeName() != childName)
      return nullptr;
    node = child;
  }
  return node;
}

Response InspectorDOMAgent::pushNodeByPathToFrontend(const String& path,
                                                     int* nodeId) {
  if (Node* node = nodeForPath(path))
    *nodeId = pushNodePathToFrontend(node);
  else
    return Response::Error("No node with given path found");
  return Response::OK();
}

Response InspectorDOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backendNodeIds,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = protocol::Array<int>::create();
  for (size_t index = 0; index < backendNodeIds->length(); ++index) {
    Node* node = DOMNodeIds::nodeForId(backendNodeIds->get(index));
    if (node && node->document().frame() &&
        m_inspectedFrames->contains(node->document().frame()))
      (*result)->addItem(pushNodePathToFrontend(node));
    else
      (*result)->addItem(0);
  }
  return Response::OK();
}

class InspectableNode final
    : public v8_inspector::V8InspectorSession::Inspectable {
 public:
  explicit InspectableNode(Node* node)
      : m_nodeId(DOMNodeIds::idForNode(node)) {}

  v8::Local<v8::Value> get(v8::Local<v8::Context> context) override {
    return nodeV8Value(context, DOMNodeIds::nodeForId(m_nodeId));
  }

 private:
  int m_nodeId;
};

Response InspectorDOMAgent::setInspectedNode(int nodeId) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  m_v8Session->addInspectedObject(makeUnique<InspectableNode>(node));
  if (m_client)
    m_client->setInspectedNode(node);
  return Response::OK();
}

Response InspectorDOMAgent::getRelayoutBoundary(int nodeId,
                                                int* relayoutBoundaryNodeId) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  LayoutObject* layoutObject = node->layoutObject();
  if (!layoutObject) {
    return Response::Error(
        "No layout object for node, perhaps orphan or hidden node");
  }
  while (layoutObject && !layoutObject->isDocumentElement() &&
         !layoutObject->isRelayoutBoundaryForInspector())
    layoutObject = layoutObject->container();
  Node* resultNode =
      layoutObject ? layoutObject->generatingNode() : node->ownerDocument();
  *relayoutBoundaryNodeId = pushNodePathToFrontend(resultNode);
  return Response::OK();
}

Response InspectorDOMAgent::getHighlightObjectForTest(
    int nodeId,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  Response response = assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;
  InspectorHighlight highlight(node, InspectorHighlight::defaultConfig(), true);
  *result = highlight.asProtocolValue();
  return Response::OK();
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
InspectorDOMAgent::resolveNode(Node* node, const String& objectGroup) {
  Document* document =
      node->isDocumentNode() ? &node->document() : node->ownerDocument();
  LocalFrame* frame = document ? document->frame() : nullptr;
  if (!frame)
    return nullptr;

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState)
    return nullptr;

  ScriptState::Scope scope(scriptState);
  return m_v8Session->wrapObject(scriptState->context(),
                                 nodeV8Value(scriptState->context(), node),
                                 toV8InspectorStringView(objectGroup));
}

Response InspectorDOMAgent::pushDocumentUponHandlelessOperation() {
  if (!m_documentNodeToIdMap->contains(m_document)) {
    std::unique_ptr<protocol::DOM::Node> root;
    return getDocument(Maybe<int>(), Maybe<bool>(), &root);
  }
  return Response::OK();
}

DEFINE_TRACE(InspectorDOMAgent) {
  visitor->trace(m_domListener);
  visitor->trace(m_inspectedFrames);
  visitor->trace(m_documentNodeToIdMap);
  visitor->trace(m_danglingNodeToIdMaps);
  visitor->trace(m_idToNode);
  visitor->trace(m_idToNodesMap);
  visitor->trace(m_document);
  visitor->trace(m_revalidateTask);
  visitor->trace(m_searchResults);
  visitor->trace(m_history);
  visitor->trace(m_domEditor);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
