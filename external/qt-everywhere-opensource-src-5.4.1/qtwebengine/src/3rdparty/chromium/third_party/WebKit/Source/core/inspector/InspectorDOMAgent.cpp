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

#include "config.h"
#include "core/inspector/InspectorDOMAgent.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/dom/Attr.h"
#include "core/dom/CharacterData.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/markup.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/inspector/DOMEditor.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderView.h"
#include "core/xml/DocumentXPathEvaluator.h"
#include "core/xml/XPathResult.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "wtf/ListHashSet.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

namespace DOMAgentState {
static const char domAgentEnabled[] = "domAgentEnabled";
};

static const size_t maxTextSize = 10000;
static const UChar ellipsisUChar[] = { 0x2026, 0 };

static Color parseColor(const RefPtr<JSONObject>* colorObject)
{
    if (!colorObject || !(*colorObject))
        return Color::transparent;

    int r;
    int g;
    int b;
    bool success = (*colorObject)->getNumber("r", &r);
    success |= (*colorObject)->getNumber("g", &g);
    success |= (*colorObject)->getNumber("b", &b);
    if (!success)
        return Color::transparent;

    double a;
    success = (*colorObject)->getNumber("a", &a);
    if (!success)
        return Color(r, g, b);

    // Clamp alpha to the [0..1] range.
    if (a < 0)
        a = 0;
    else if (a > 1)
        a = 1;

    return Color(r, g, b, static_cast<int>(a * 255));
}

static Color parseConfigColor(const String& fieldName, JSONObject* configObject)
{
    const RefPtr<JSONObject> colorObject = configObject->getObject(fieldName);
    return parseColor(&colorObject);
}

static bool parseQuad(const RefPtr<JSONArray>& quadArray, FloatQuad* quad)
{
    if (!quadArray)
        return false;
    const size_t coordinatesInQuad = 8;
    double coordinates[coordinatesInQuad];
    if (quadArray->length() != coordinatesInQuad)
        return false;
    for (size_t i = 0; i < coordinatesInQuad; ++i) {
        if (!quadArray->get(i)->asNumber(coordinates + i))
            return false;
    }
    quad->setP1(FloatPoint(coordinates[0], coordinates[1]));
    quad->setP2(FloatPoint(coordinates[2], coordinates[3]));
    quad->setP3(FloatPoint(coordinates[4], coordinates[5]));
    quad->setP4(FloatPoint(coordinates[6], coordinates[7]));

    return true;
}

static Node* hoveredNodeForPoint(LocalFrame* frame, const IntPoint& point, bool ignorePointerEventsNone)
{
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
    if (ignorePointerEventsNone)
        hitType |= HitTestRequest::IgnorePointerEventsNone;
    HitTestRequest request(hitType);
    HitTestResult result(frame->view()->windowToContents(point));
    frame->contentRenderer()->hitTest(request, result);
    Node* node = result.innerPossiblyPseudoNode();
    while (node && node->nodeType() == Node::TEXT_NODE)
        node = node->parentNode();
    return node;
}

static Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformGestureEvent& event, bool ignorePointerEventsNone)
{
    return hoveredNodeForPoint(frame, event.position(), ignorePointerEventsNone);
}

static Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformMouseEvent& event, bool ignorePointerEventsNone)
{
    return hoveredNodeForPoint(frame, event.position(), ignorePointerEventsNone);
}

static Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformTouchEvent& event, bool ignorePointerEventsNone)
{
    const Vector<PlatformTouchPoint>& points = event.touchPoints();
    if (!points.size())
        return 0;
    return hoveredNodeForPoint(frame, roundedIntPoint(points[0].pos()), ignorePointerEventsNone);
}

class RevalidateStyleAttributeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RevalidateStyleAttributeTask(InspectorDOMAgent*);
    void scheduleFor(Element*);
    void reset() { m_timer.stop(); }
    void onTimer(Timer<RevalidateStyleAttributeTask>*);

private:
    InspectorDOMAgent* m_domAgent;
    Timer<RevalidateStyleAttributeTask> m_timer;
    WillBePersistentHeapHashSet<RefPtrWillBeMember<Element> > m_elements;
};

RevalidateStyleAttributeTask::RevalidateStyleAttributeTask(InspectorDOMAgent* domAgent)
    : m_domAgent(domAgent)
    , m_timer(this, &RevalidateStyleAttributeTask::onTimer)
{
}

void RevalidateStyleAttributeTask::scheduleFor(Element* element)
{
    m_elements.add(element);
    if (!m_timer.isActive())
        m_timer.startOneShot(0, FROM_HERE);
}

void RevalidateStyleAttributeTask::onTimer(Timer<RevalidateStyleAttributeTask>*)
{
    // The timer is stopped on m_domAgent destruction, so this method will never be called after m_domAgent has been destroyed.
    WillBeHeapVector<RawPtrWillBeMember<Element> > elements;
    for (WillBePersistentHeapHashSet<RefPtrWillBeMember<Element> >::iterator it = m_elements.begin(), end = m_elements.end(); it != end; ++it)
        elements.append(it->get());
    m_domAgent->styleAttributeInvalidated(elements);

    m_elements.clear();
}

String InspectorDOMAgent::toErrorString(ExceptionState& exceptionState)
{
    if (exceptionState.hadException())
        return DOMException::getErrorName(exceptionState.code()) + " " + exceptionState.message();
    return "";
}

InspectorDOMAgent::InspectorDOMAgent(InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager, InspectorOverlay* overlay)
    : InspectorBaseAgent<InspectorDOMAgent>("DOM")
    , m_pageAgent(pageAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_overlay(overlay)
    , m_frontend(0)
    , m_domListener(0)
    , m_documentNodeToIdMap(adoptPtrWillBeNoop(new NodeToIdMap()))
    , m_lastNodeId(1)
    , m_searchingForNode(NotSearching)
    , m_suppressAttributeModifiedEvent(false)
    , m_listener(0)
{
}

InspectorDOMAgent::~InspectorDOMAgent()
{
    reset();
    ASSERT(m_searchingForNode == NotSearching);
}

void InspectorDOMAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_history = adoptPtrWillBeNoop(new InspectorHistory());
    m_domEditor = adoptPtrWillBeNoop(new DOMEditor(m_history.get()));

    m_frontend = frontend->dom();
    m_instrumentingAgents->setInspectorDOMAgent(this);
    m_document = m_pageAgent->mainFrame()->document();
}

void InspectorDOMAgent::clearFrontend()
{
    ASSERT(m_frontend);

    m_history.clear();
    m_domEditor.clear();

    ErrorString error;
    setSearchingForNode(&error, NotSearching, 0);
    hideHighlight(&error);

    m_frontend = 0;
    m_instrumentingAgents->setInspectorDOMAgent(0);
    disable(0);
    reset();
}

void InspectorDOMAgent::restore()
{
    // Reset document to avoid early return from setDocument.
    m_document = nullptr;
    setDocument(m_pageAgent->mainFrame()->document());
}

Vector<Document*> InspectorDOMAgent::documents()
{
    Vector<Document*> result;
    for (Frame* frame = m_document->frame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        Document* document = toLocalFrame(frame)->document();
        if (!document)
            continue;
        result.append(document);
    }
    return result;
}

void InspectorDOMAgent::reset()
{
    discardFrontendBindings();
    m_document = nullptr;
}

void InspectorDOMAgent::setDOMListener(DOMListener* listener)
{
    m_domListener = listener;
}

void InspectorDOMAgent::setDocument(Document* doc)
{
    if (doc == m_document.get())
        return;

    reset();

    m_document = doc;

    if (!enabled())
        return;

    // Immediately communicate 0 document or document that has finished loading.
    if (!doc || !doc->parsing())
        m_frontend->documentUpdated();
}

void InspectorDOMAgent::releaseDanglingNodes()
{
    m_danglingNodeToIdMaps.clear();
}

int InspectorDOMAgent::bind(Node* node, NodeToIdMap* nodesMap)
{
    int id = nodesMap->get(node);
    if (id)
        return id;
    id = m_lastNodeId++;
    nodesMap->set(node, id);
    m_idToNode.set(id, node);
    m_idToNodesMap.set(id, nodesMap);
    return id;
}

void InspectorDOMAgent::unbind(Node* node, NodeToIdMap* nodesMap)
{
    int id = nodesMap->get(node);
    if (!id)
        return;

    m_idToNode.remove(id);
    m_idToNodesMap.remove(id);

    if (node->isFrameOwnerElement()) {
        Document* contentDocument = toHTMLFrameOwnerElement(node)->contentDocument();
        if (m_domListener)
            m_domListener->didRemoveDocument(contentDocument);
        if (contentDocument)
            unbind(contentDocument, nodesMap);
    }

    for (ShadowRoot* root = node->youngestShadowRoot(); root; root = root->olderShadowRoot())
        unbind(root, nodesMap);

    if (node->isElementNode()) {
        Element* element = toElement(node);
        if (element->pseudoElement(BEFORE))
            unbind(element->pseudoElement(BEFORE), nodesMap);
        if (element->pseudoElement(AFTER))
            unbind(element->pseudoElement(AFTER), nodesMap);

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

Node* InspectorDOMAgent::assertNode(ErrorString* errorString, int nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node) {
        *errorString = "Could not find node with given id";
        return 0;
    }
    return node;
}

Document* InspectorDOMAgent::assertDocument(ErrorString* errorString, int nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return 0;

    if (!(node->isDocumentNode())) {
        *errorString = "Document is not available";
        return 0;
    }
    return toDocument(node);
}

Element* InspectorDOMAgent::assertElement(ErrorString* errorString, int nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return 0;

    if (!node->isElementNode()) {
        *errorString = "Node is not an Element";
        return 0;
    }
    return toElement(node);
}

static ShadowRoot* userAgentShadowRoot(Node* node)
{
    if (!node || !node->isInShadowTree())
        return 0;

    Node* candidate = node;
    while (candidate && !candidate->isShadowRoot())
        candidate = candidate->parentOrShadowHostNode();
    ASSERT(candidate);
    ShadowRoot* shadowRoot = toShadowRoot(candidate);

    return shadowRoot->type() == ShadowRoot::UserAgentShadowRoot ? shadowRoot : 0;
}

Node* InspectorDOMAgent::assertEditableNode(ErrorString* errorString, int nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return 0;

    if (node->isInShadowTree()) {
        if (node->isShadowRoot()) {
            *errorString = "Cannot edit shadow roots";
            return 0;
        }
        if (userAgentShadowRoot(node)) {
            *errorString = "Cannot edit nodes from user-agent shadow trees";
            return 0;
        }
    }

    if (node->isPseudoElement()) {
        *errorString = "Cannot edit pseudo elements";
        return 0;
    }

    return node;
}

Element* InspectorDOMAgent::assertEditableElement(ErrorString* errorString, int nodeId)
{
    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return 0;

    if (element->isInShadowTree() && userAgentShadowRoot(element)) {
        *errorString = "Cannot edit elements from user-agent shadow trees";
        return 0;
    }

    if (element->isPseudoElement()) {
        *errorString = "Cannot edit pseudo elements";
        return 0;
    }

    return element;
}

void InspectorDOMAgent::enable(ErrorString*)
{
    if (enabled())
        return;
    m_state->setBoolean(DOMAgentState::domAgentEnabled, true);
    if (m_listener)
        m_listener->domAgentWasEnabled();
}

bool InspectorDOMAgent::enabled() const
{
    return m_state->getBoolean(DOMAgentState::domAgentEnabled);
}

void InspectorDOMAgent::disable(ErrorString*)
{
    if (!enabled())
        return;
    m_state->setBoolean(DOMAgentState::domAgentEnabled, false);
    reset();
    if (m_listener)
        m_listener->domAgentWasDisabled();
}

void InspectorDOMAgent::getDocument(ErrorString* errorString, RefPtr<TypeBuilder::DOM::Node>& root)
{
    // Backward compatibility. Mark agent as enabled when it requests document.
    enable(errorString);

    if (!m_document) {
        *errorString = "Document is not available";
        return;
    }

    discardFrontendBindings();

    root = buildObjectForNode(m_document.get(), 2, m_documentNodeToIdMap.get());
}

void InspectorDOMAgent::pushChildNodesToFrontend(int nodeId, int depth)
{
    Node* node = nodeForId(nodeId);
    if (!node || (!node->isElementNode() && !node->isDocumentNode() && !node->isDocumentFragment()))
        return;

    NodeToIdMap* nodeMap = m_idToNodesMap.get(nodeId);

    if (m_childrenRequested.contains(nodeId)) {
        if (depth <= 1)
            return;

        depth--;

        for (node = innerFirstChild(node); node; node = innerNextSibling(node)) {
            int childNodeId = nodeMap->get(node);
            ASSERT(childNodeId);
            pushChildNodesToFrontend(childNodeId, depth);
        }

        return;
    }

    RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > children = buildArrayForContainerChildren(node, depth, nodeMap);
    m_frontend->setChildNodes(nodeId, children.release());
}

void InspectorDOMAgent::discardFrontendBindings()
{
    if (m_history)
        m_history->reset();
    m_searchResults.clear();
    m_documentNodeToIdMap->clear();
    m_idToNode.clear();
    m_idToNodesMap.clear();
    releaseDanglingNodes();
    m_childrenRequested.clear();
    m_cachedChildCount.clear();
    if (m_revalidateStyleAttrTask)
        m_revalidateStyleAttrTask->reset();
}

Node* InspectorDOMAgent::nodeForId(int id)
{
    if (!id)
        return 0;

    WillBeHeapHashMap<int, RawPtrWillBeMember<Node> >::iterator it = m_idToNode.find(id);
    if (it != m_idToNode.end())
        return it->value;
    return 0;
}

void InspectorDOMAgent::requestChildNodes(ErrorString* errorString, int nodeId, const int* depth)
{
    int sanitizedDepth;

    if (!depth)
        sanitizedDepth = 1;
    else if (*depth == -1)
        sanitizedDepth = INT_MAX;
    else if (*depth > 0)
        sanitizedDepth = *depth;
    else {
        *errorString = "Please provide a positive integer as a depth or -1 for entire subtree";
        return;
    }

    pushChildNodesToFrontend(nodeId, sanitizedDepth);
}

void InspectorDOMAgent::querySelector(ErrorString* errorString, int nodeId, const String& selectors, int* elementId)
{
    *elementId = 0;
    Node* node = assertNode(errorString, nodeId);
    if (!node || !node->isContainerNode())
        return;

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<Element> element = toContainerNode(node)->querySelector(AtomicString(selectors), exceptionState);
    if (exceptionState.hadException()) {
        *errorString = "DOM Error while querying";
        return;
    }

    if (element)
        *elementId = pushNodePathToFrontend(element.get());
}

void InspectorDOMAgent::querySelectorAll(ErrorString* errorString, int nodeId, const String& selectors, RefPtr<TypeBuilder::Array<int> >& result)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node || !node->isContainerNode())
        return;

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<StaticNodeList> nodes = toContainerNode(node)->querySelectorAll(AtomicString(selectors), exceptionState);
    if (exceptionState.hadException()) {
        *errorString = "DOM Error while querying";
        return;
    }

    result = TypeBuilder::Array<int>::create();

    for (unsigned i = 0; i < nodes->length(); ++i)
        result->addItem(pushNodePathToFrontend(nodes->item(i)));
}

int InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    ASSERT(nodeToPush);  // Invalid input

    if (!m_document)
        return 0;
    // FIXME: Oilpan: .get will be unnecessary if m_document is a Member<>.
    if (!m_documentNodeToIdMap->contains(m_document.get()))
        return 0;

    // Return id in case the node is known.
    int result = m_documentNodeToIdMap->get(nodeToPush);
    if (result)
        return result;

    Node* node = nodeToPush;
    Vector<Node*> path;
    NodeToIdMap* danglingMap = 0;

    while (true) {
        Node* parent = innerParentNode(node);
        if (!parent) {
            // Node being pushed is detached -> push subtree root.
            OwnPtrWillBeRawPtr<NodeToIdMap> newMap = adoptPtrWillBeNoop(new NodeToIdMap);
            danglingMap = newMap.get();
            m_danglingNodeToIdMaps.append(newMap.release());
            RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > children = TypeBuilder::Array<TypeBuilder::DOM::Node>::create();
            children->addItem(buildObjectForNode(node, 0, danglingMap));
            m_frontend->setChildNodes(0, children);
            break;
        } else {
            path.append(parent);
            if (m_documentNodeToIdMap->get(parent))
                break;
            node = parent;
        }
    }

    NodeToIdMap* map = danglingMap ? danglingMap : m_documentNodeToIdMap.get();
    for (int i = path.size() - 1; i >= 0; --i) {
        int nodeId = map->get(path.at(i));
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return map->get(nodeToPush);
}

int InspectorDOMAgent::boundNodeId(Node* node)
{
    return m_documentNodeToIdMap->get(node);
}

void InspectorDOMAgent::setAttributeValue(ErrorString* errorString, int elementId, const String& name, const String& value)
{
    Element* element = assertEditableElement(errorString, elementId);
    if (!element)
        return;

    m_domEditor->setAttribute(element, name, value, errorString);
}

void InspectorDOMAgent::setAttributesAsText(ErrorString* errorString, int elementId, const String& text, const String* const name)
{
    Element* element = assertEditableElement(errorString, elementId);
    if (!element)
        return;

    String markup = "<span " + text + "></span>";
    RefPtrWillBeRawPtr<DocumentFragment> fragment = element->document().createDocumentFragment();

    bool shouldIgnoreCase = element->document().isHTMLDocument() && element->isHTMLElement();
    // Not all elements can represent the context (i.e. IFRAME), hence using document.body.
    if (shouldIgnoreCase && element->document().body())
        fragment->parseHTML(markup, element->document().body(), AllowScriptingContent);
    else
        fragment->parseXML(markup, 0, AllowScriptingContent);

    Element* parsedElement = fragment->firstChild() && fragment->firstChild()->isElementNode() ? toElement(fragment->firstChild()) : 0;
    if (!parsedElement) {
        *errorString = "Could not parse value as attributes";
        return;
    }

    String caseAdjustedName = name ? (shouldIgnoreCase ? name->lower() : *name) : String();

    if (!parsedElement->hasAttributes() && name) {
        m_domEditor->removeAttribute(element, caseAdjustedName, errorString);
        return;
    }

    bool foundOriginalAttribute = false;
    AttributeCollection attributes = parsedElement->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
        // Add attribute pair
        String attributeName = it->name().toString();
        if (shouldIgnoreCase)
            attributeName = attributeName.lower();
        foundOriginalAttribute |= name && attributeName == caseAdjustedName;
        if (!m_domEditor->setAttribute(element, attributeName, it->value(), errorString))
            return;
    }

    if (!foundOriginalAttribute && name && !name->stripWhiteSpace().isEmpty())
        m_domEditor->removeAttribute(element, caseAdjustedName, errorString);
}

void InspectorDOMAgent::removeAttribute(ErrorString* errorString, int elementId, const String& name)
{
    Element* element = assertEditableElement(errorString, elementId);
    if (!element)
        return;

    m_domEditor->removeAttribute(element, name, errorString);
}

void InspectorDOMAgent::removeNode(ErrorString* errorString, int nodeId)
{
    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return;

    ContainerNode* parentNode = node->parentNode();
    if (!parentNode) {
        *errorString = "Cannot remove detached node";
        return;
    }

    m_domEditor->removeChild(parentNode, node, errorString);
}

void InspectorDOMAgent::setNodeName(ErrorString* errorString, int nodeId, const String& tagName, int* newId)
{
    *newId = 0;

    Node* oldNode = nodeForId(nodeId);
    if (!oldNode || !oldNode->isElementNode())
        return;

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<Element> newElem = oldNode->document().createElement(AtomicString(tagName), exceptionState);
    if (exceptionState.hadException())
        return;

    // Copy over the original node's attributes.
    newElem->cloneAttributesFromElement(*toElement(oldNode));

    // Copy over the original node's children.
    Node* child;
    while ((child = oldNode->firstChild())) {
        if (!m_domEditor->insertBefore(newElem.get(), child, 0, errorString))
            return;
    }

    // Replace the old node with the new node
    ContainerNode* parent = oldNode->parentNode();
    if (!m_domEditor->insertBefore(parent, newElem.get(), oldNode->nextSibling(), errorString))
        return;
    if (!m_domEditor->removeChild(parent, oldNode, errorString))
        return;

    *newId = pushNodePathToFrontend(newElem.get());
    if (m_childrenRequested.contains(nodeId))
        pushChildNodesToFrontend(*newId);
}

void InspectorDOMAgent::getOuterHTML(ErrorString* errorString, int nodeId, WTF::String* outerHTML)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return;

    *outerHTML = createMarkup(node);
}

void InspectorDOMAgent::setOuterHTML(ErrorString* errorString, int nodeId, const String& outerHTML)
{
    if (!nodeId) {
        ASSERT(m_document);
        DOMPatchSupport domPatchSupport(m_domEditor.get(), *m_document.get());
        domPatchSupport.patchDocument(outerHTML);
        return;
    }

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return;

    Document* document = node->isDocumentNode() ? toDocument(node) : node->ownerDocument();
    if (!document || (!document->isHTMLDocument() && !document->isXMLDocument())) {
        *errorString = "Not an HTML/XML document";
        return;
    }

    Node* newNode = 0;
    if (!m_domEditor->setOuterHTML(node, outerHTML, &newNode, errorString))
        return;

    if (!newNode) {
        // The only child node has been deleted.
        return;
    }

    int newId = pushNodePathToFrontend(newNode);

    bool childrenRequested = m_childrenRequested.contains(nodeId);
    if (childrenRequested)
        pushChildNodesToFrontend(newId);
}

void InspectorDOMAgent::setNodeValue(ErrorString* errorString, int nodeId, const String& value)
{
    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return;

    if (node->nodeType() != Node::TEXT_NODE) {
        *errorString = "Can only set value of text nodes";
        return;
    }

    m_domEditor->replaceWholeText(toText(node), value, errorString);
}

void InspectorDOMAgent::getEventListenersForNode(ErrorString* errorString, int nodeId, const String* objectGroup, RefPtr<TypeBuilder::Array<TypeBuilder::DOM::EventListener> >& listenersArray)
{
    listenersArray = TypeBuilder::Array<TypeBuilder::DOM::EventListener>::create();
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return;
    Vector<EventListenerInfo> eventInformation;
    getEventListeners(node, eventInformation, true);

    // Get Capturing Listeners (in this order)
    size_t eventInformationLength = eventInformation.size();
    for (size_t i = 0; i < eventInformationLength; ++i) {
        const EventListenerInfo& info = eventInformation[i];
        const EventListenerVector& vector = info.eventListenerVector;
        for (size_t j = 0; j < vector.size(); ++j) {
            const RegisteredEventListener& listener = vector[j];
            if (listener.useCapture) {
                RefPtr<TypeBuilder::DOM::EventListener> listenerObject = buildObjectForEventListener(listener, info.eventType, info.eventTarget->toNode(), objectGroup);
                if (listenerObject)
                    listenersArray->addItem(listenerObject);
            }
        }
    }

    // Get Bubbling Listeners (reverse order)
    for (size_t i = eventInformationLength; i; --i) {
        const EventListenerInfo& info = eventInformation[i - 1];
        const EventListenerVector& vector = info.eventListenerVector;
        for (size_t j = 0; j < vector.size(); ++j) {
            const RegisteredEventListener& listener = vector[j];
            if (!listener.useCapture) {
                RefPtr<TypeBuilder::DOM::EventListener> listenerObject = buildObjectForEventListener(listener, info.eventType, info.eventTarget->toNode(), objectGroup);
                if (listenerObject)
                    listenersArray->addItem(listenerObject);
            }
        }
    }
}

void InspectorDOMAgent::getEventListeners(EventTarget* target, Vector<EventListenerInfo>& eventInformation, bool includeAncestors)
{
    // The Node's Ancestors including self.
    Vector<EventTarget*> ancestors;
    ancestors.append(target);
    if (includeAncestors) {
        Node* node = target->toNode();
        for (ContainerNode* ancestor = node ? node->parentOrShadowHostNode() : 0; ancestor; ancestor = ancestor->parentOrShadowHostNode())
            ancestors.append(ancestor);
    }

    // Nodes and their Listeners for the concerned event types (order is top to bottom)
    for (size_t i = ancestors.size(); i; --i) {
        EventTarget* ancestor = ancestors[i - 1];
        Vector<AtomicString> eventTypes = ancestor->eventTypes();
        for (size_t j = 0; j < eventTypes.size(); ++j) {
            AtomicString& type = eventTypes[j];
            const EventListenerVector& listeners = ancestor->getEventListeners(type);
            EventListenerVector filteredListeners;
            filteredListeners.reserveCapacity(listeners.size());
            for (size_t k = 0; k < listeners.size(); ++k) {
                if (listeners[k].listener->type() == EventListener::JSEventListenerType)
                    filteredListeners.append(listeners[k]);
            }
            if (!filteredListeners.isEmpty())
                eventInformation.append(EventListenerInfo(ancestor, type, filteredListeners));
        }
    }
}

void InspectorDOMAgent::performSearch(ErrorString*, const String& whitespaceTrimmedQuery, String* searchId, int* resultCount)
{
    // FIXME: Few things are missing here:
    // 1) Search works with node granularity - number of matches within node is not calculated.
    // 2) There is no need to push all search results to the front-end at a time, pushing next / previous result
    //    is sufficient.

    unsigned queryLength = whitespaceTrimmedQuery.length();
    bool startTagFound = !whitespaceTrimmedQuery.find('<');
    bool endTagFound = whitespaceTrimmedQuery.reverseFind('>') + 1 == queryLength;
    bool startQuoteFound = !whitespaceTrimmedQuery.find('"');
    bool endQuoteFound = whitespaceTrimmedQuery.reverseFind('"') + 1 == queryLength;
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

    Vector<Document*> docs = documents();
    ListHashSet<Node*> resultCollector;

    for (Vector<Document*>::iterator it = docs.begin(); it != docs.end(); ++it) {
        Document* document = *it;
        Node* node = document->documentElement();
        if (!node)
            continue;

        // Manual plain text search.
        while ((node = NodeTraversal::next(*node, document->documentElement()))) {
            switch (node->nodeType()) {
            case Node::TEXT_NODE:
            case Node::COMMENT_NODE:
            case Node::CDATA_SECTION_NODE: {
                String text = node->nodeValue();
                if (text.findIgnoringCase(whitespaceTrimmedQuery) != kNotFound)
                    resultCollector.add(node);
                break;
            }
            case Node::ELEMENT_NODE: {
                if ((!startTagFound && !endTagFound && (node->nodeName().findIgnoringCase(tagNameQuery) != kNotFound))
                    || (startTagFound && endTagFound && equalIgnoringCase(node->nodeName(), tagNameQuery))
                    || (startTagFound && !endTagFound && node->nodeName().startsWith(tagNameQuery, false))
                    || (!startTagFound && endTagFound && node->nodeName().endsWith(tagNameQuery, false))) {
                    resultCollector.add(node);
                    break;
                }
                // Go through all attributes and serialize them.
                const Element* element = toElement(node);
                if (!element->hasAttributes())
                    break;

                AttributeCollection attributes = element->attributes();
                AttributeCollection::const_iterator end = attributes.end();
                for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
                    // Add attribute pair
                    if (it->localName().find(whitespaceTrimmedQuery, 0, false) != kNotFound) {
                        resultCollector.add(node);
                        break;
                    }
                    size_t foundPosition = it->value().find(attributeQuery, 0, false);
                    if (foundPosition != kNotFound) {
                        if (!exactAttributeMatch || (!foundPosition && it->value().length() == attributeQuery.length())) {
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
        for (Vector<Document*>::iterator it = docs.begin(); it != docs.end(); ++it) {
            Document* document = *it;
            ASSERT(document);
            TrackExceptionState exceptionState;
            RefPtrWillBeRawPtr<XPathResult> result = DocumentXPathEvaluator::evaluate(*document, whitespaceTrimmedQuery, document, nullptr, XPathResult::ORDERED_NODE_SNAPSHOT_TYPE, 0, exceptionState);
            if (exceptionState.hadException() || !result)
                continue;

            unsigned long size = result->snapshotLength(exceptionState);
            for (unsigned long i = 0; !exceptionState.hadException() && i < size; ++i) {
                Node* node = result->snapshotItem(i, exceptionState);
                if (exceptionState.hadException())
                    break;

                if (node->nodeType() == Node::ATTRIBUTE_NODE)
                    node = toAttr(node)->ownerElement();
                resultCollector.add(node);
            }
        }

        // Selector evaluation
        for (Vector<Document*>::iterator it = docs.begin(); it != docs.end(); ++it) {
            Document* document = *it;
            TrackExceptionState exceptionState;
            RefPtrWillBeRawPtr<StaticNodeList> nodeList = document->querySelectorAll(AtomicString(whitespaceTrimmedQuery), exceptionState);
            if (exceptionState.hadException() || !nodeList)
                continue;

            unsigned size = nodeList->length();
            for (unsigned i = 0; i < size; ++i)
                resultCollector.add(nodeList->item(i));
        }
    }

    *searchId = IdentifiersFactory::createIdentifier();
    WillBeHeapVector<RefPtrWillBeMember<Node> >* resultsIt = &m_searchResults.add(*searchId, WillBeHeapVector<RefPtrWillBeMember<Node> >()).storedValue->value;

    for (ListHashSet<Node*>::iterator it = resultCollector.begin(); it != resultCollector.end(); ++it)
        resultsIt->append(*it);

    *resultCount = resultsIt->size();
}

void InspectorDOMAgent::getSearchResults(ErrorString* errorString, const String& searchId, int fromIndex, int toIndex, RefPtr<TypeBuilder::Array<int> >& nodeIds)
{
    SearchResults::iterator it = m_searchResults.find(searchId);
    if (it == m_searchResults.end()) {
        *errorString = "No search session with given id found";
        return;
    }

    int size = it->value.size();
    if (fromIndex < 0 || toIndex > size || fromIndex >= toIndex) {
        *errorString = "Invalid search result range";
        return;
    }

    nodeIds = TypeBuilder::Array<int>::create();
    for (int i = fromIndex; i < toIndex; ++i)
        nodeIds->addItem(pushNodePathToFrontend((it->value)[i].get()));
}

void InspectorDOMAgent::discardSearchResults(ErrorString*, const String& searchId)
{
    m_searchResults.remove(searchId);
}

bool InspectorDOMAgent::handleMousePress()
{
    if (m_searchingForNode == NotSearching)
        return false;

    if (Node* node = m_overlay->highlightedNode()) {
        inspect(node);
        return true;
    }
    return false;
}

bool InspectorDOMAgent::handleGestureEvent(LocalFrame* frame, const PlatformGestureEvent& event)
{
    if (m_searchingForNode == NotSearching || event.type() != PlatformEvent::GestureTap)
        return false;
    Node* node = hoveredNodeForEvent(frame, event, false);
    if (node && m_inspectModeHighlightConfig) {
        m_overlay->highlightNode(node, 0 /* eventTarget */, *m_inspectModeHighlightConfig, false);
        inspect(node);
        return true;
    }
    return false;
}

bool InspectorDOMAgent::handleTouchEvent(LocalFrame* frame, const PlatformTouchEvent& event)
{
    if (m_searchingForNode == NotSearching)
        return false;
    Node* node = hoveredNodeForEvent(frame, event, false);
    if (node && m_inspectModeHighlightConfig) {
        m_overlay->highlightNode(node, 0 /* eventTarget */, *m_inspectModeHighlightConfig, false);
        inspect(node);
        return true;
    }
    return false;
}

void InspectorDOMAgent::inspect(Node* inspectedNode)
{
    if (!m_frontend || !inspectedNode)
        return;

    Node* node = inspectedNode;
    while (node && !node->isElementNode() && !node->isDocumentNode() && !node->isDocumentFragment())
        node = node->parentOrShadowHostNode();

    if (!node)
        return;

    int nodeId = pushNodePathToFrontend(node);
    if (nodeId)
        m_frontend->inspectNodeRequested(nodeId);
}

void InspectorDOMAgent::handleMouseMove(LocalFrame* frame, const PlatformMouseEvent& event)
{
    if (m_searchingForNode == NotSearching)
        return;

    if (!frame->view() || !frame->contentRenderer())
        return;
    Node* node = hoveredNodeForEvent(frame, event, event.shiftKey());

    // Do not highlight within UA shadow root unless requested.
    if (m_searchingForNode != SearchingForUAShadow) {
        ShadowRoot* uaShadowRoot = userAgentShadowRoot(node);
        if (uaShadowRoot)
            node = uaShadowRoot->host();
    }

    // Shadow roots don't have boxes - use host element instead.
    if (node && node->isShadowRoot())
        node = node->parentOrShadowHostNode();

    if (!node)
        return;

    Node* eventTarget = event.shiftKey() ? hoveredNodeForEvent(frame, event, false) : 0;
    if (eventTarget == node)
        eventTarget = 0;

    if (node && m_inspectModeHighlightConfig)
        m_overlay->highlightNode(node, eventTarget, *m_inspectModeHighlightConfig, event.ctrlKey() || event.metaKey());
}

void InspectorDOMAgent::setSearchingForNode(ErrorString* errorString, SearchMode searchMode, JSONObject* highlightInspectorObject)
{
    if (m_searchingForNode == searchMode)
        return;

    m_searchingForNode = searchMode;
    m_overlay->setInspectModeEnabled(searchMode != NotSearching);
    if (searchMode != NotSearching) {
        m_inspectModeHighlightConfig = highlightConfigFromInspectorObject(errorString, highlightInspectorObject);
        if (!m_inspectModeHighlightConfig)
            return;
    } else
        hideHighlight(errorString);
}

PassOwnPtr<HighlightConfig> InspectorDOMAgent::highlightConfigFromInspectorObject(ErrorString* errorString, JSONObject* highlightInspectorObject)
{
    if (!highlightInspectorObject) {
        *errorString = "Internal error: highlight configuration parameter is missing";
        return nullptr;
    }

    OwnPtr<HighlightConfig> highlightConfig = adoptPtr(new HighlightConfig());
    bool showInfo = false; // Default: false (do not show a tooltip).
    highlightInspectorObject->getBoolean("showInfo", &showInfo);
    highlightConfig->showInfo = showInfo;
    bool showRulers = false; // Default: false (do not show rulers).
    highlightInspectorObject->getBoolean("showRulers", &showRulers);
    highlightConfig->showRulers = showRulers;
    highlightConfig->content = parseConfigColor("contentColor", highlightInspectorObject);
    highlightConfig->contentOutline = parseConfigColor("contentOutlineColor", highlightInspectorObject);
    highlightConfig->padding = parseConfigColor("paddingColor", highlightInspectorObject);
    highlightConfig->border = parseConfigColor("borderColor", highlightInspectorObject);
    highlightConfig->margin = parseConfigColor("marginColor", highlightInspectorObject);
    highlightConfig->eventTarget = parseConfigColor("eventTargetColor", highlightInspectorObject);
    return highlightConfig.release();
}

void InspectorDOMAgent::setInspectModeEnabled(ErrorString* errorString, bool enabled, const bool* inspectUAShadowDOM, const RefPtr<JSONObject>* highlightConfig)
{
    if (enabled && !pushDocumentUponHandlelessOperation(errorString))
        return;
    SearchMode searchMode = enabled ? (inspectUAShadowDOM && *inspectUAShadowDOM ? SearchingForUAShadow : SearchingForNormal) : NotSearching;
    setSearchingForNode(errorString, searchMode, highlightConfig ? highlightConfig->get() : 0);
}

void InspectorDOMAgent::highlightRect(ErrorString*, int x, int y, int width, int height, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor)
{
    OwnPtr<FloatQuad> quad = adoptPtr(new FloatQuad(FloatRect(x, y, width, height)));
    innerHighlightQuad(quad.release(), color, outlineColor);
}

void InspectorDOMAgent::highlightQuad(ErrorString* errorString, const RefPtr<JSONArray>& quadArray, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor)
{
    OwnPtr<FloatQuad> quad = adoptPtr(new FloatQuad());
    if (!parseQuad(quadArray, quad.get())) {
        *errorString = "Invalid Quad format";
        return;
    }
    innerHighlightQuad(quad.release(), color, outlineColor);
}

void InspectorDOMAgent::innerHighlightQuad(PassOwnPtr<FloatQuad> quad, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor)
{
    OwnPtr<HighlightConfig> highlightConfig = adoptPtr(new HighlightConfig());
    highlightConfig->content = parseColor(color);
    highlightConfig->contentOutline = parseColor(outlineColor);
    m_overlay->highlightQuad(quad, *highlightConfig);
}

void InspectorDOMAgent::highlightNode(ErrorString* errorString, const RefPtr<JSONObject>& highlightInspectorObject, const int* nodeId, const String* objectId)
{
    Node* node = 0;
    if (nodeId) {
        node = assertNode(errorString, *nodeId);
    } else if (objectId) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*objectId);
        node = injectedScript.nodeForObjectId(*objectId);
        if (!node)
            *errorString = "Node for given objectId not found";
    } else
        *errorString = "Either nodeId or objectId must be specified";

    if (!node)
        return;

    OwnPtr<HighlightConfig> highlightConfig = highlightConfigFromInspectorObject(errorString, highlightInspectorObject.get());
    if (!highlightConfig)
        return;

    m_overlay->highlightNode(node, 0 /* eventTarget */, *highlightConfig, false);
}

void InspectorDOMAgent::highlightFrame(
    ErrorString*,
    const String& frameId,
    const RefPtr<JSONObject>* color,
    const RefPtr<JSONObject>* outlineColor)
{
    LocalFrame* frame = m_pageAgent->frameForId(frameId);
    // FIXME: Inspector doesn't currently work cross process.
    if (frame && frame->deprecatedLocalOwner()) {
        OwnPtr<HighlightConfig> highlightConfig = adoptPtr(new HighlightConfig());
        highlightConfig->showInfo = true; // Always show tooltips for frames.
        highlightConfig->content = parseColor(color);
        highlightConfig->contentOutline = parseColor(outlineColor);
        m_overlay->highlightNode(frame->deprecatedLocalOwner(), 0 /* eventTarget */, *highlightConfig, false);
    }
}

void InspectorDOMAgent::hideHighlight(ErrorString*)
{
    m_overlay->hideHighlight();
}

void InspectorDOMAgent::moveTo(ErrorString* errorString, int nodeId, int targetElementId, const int* const anchorNodeId, int* newNodeId)
{
    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return;

    Element* targetElement = assertEditableElement(errorString, targetElementId);
    if (!targetElement)
        return;

    Node* anchorNode = 0;
    if (anchorNodeId && *anchorNodeId) {
        anchorNode = assertEditableNode(errorString, *anchorNodeId);
        if (!anchorNode)
            return;
        if (anchorNode->parentNode() != targetElement) {
            *errorString = "Anchor node must be child of the target element";
            return;
        }
    }

    if (!m_domEditor->insertBefore(targetElement, node, anchorNode, errorString))
        return;

    *newNodeId = pushNodePathToFrontend(node);
}

void InspectorDOMAgent::undo(ErrorString* errorString)
{
    TrackExceptionState exceptionState;
    m_history->undo(exceptionState);
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorDOMAgent::redo(ErrorString* errorString)
{
    TrackExceptionState exceptionState;
    m_history->redo(exceptionState);
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorDOMAgent::markUndoableState(ErrorString*)
{
    m_history->markUndoableState();
}

void InspectorDOMAgent::focus(ErrorString* errorString, int nodeId)
{
    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return;

    element->document().updateLayoutIgnorePendingStylesheets();
    if (!element->isFocusable()) {
        *errorString = "Element is not focusable";
        return;
    }
    element->focus();
}

void InspectorDOMAgent::setFileInputFiles(ErrorString* errorString, int nodeId, const RefPtr<JSONArray>& files)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return;
    if (!isHTMLInputElement(*node) || !toHTMLInputElement(*node).isFileUpload()) {
        *errorString = "Node is not a file input element";
        return;
    }

    RefPtrWillBeRawPtr<FileList> fileList = FileList::create();
    for (JSONArray::const_iterator iter = files->begin(); iter != files->end(); ++iter) {
        String path;
        if (!(*iter)->asString(&path)) {
            *errorString = "Files must be strings";
            return;
        }
        fileList->append(File::create(path));
    }
    toHTMLInputElement(node)->setFiles(fileList);
}

static RefPtr<TypeBuilder::Array<double> > buildArrayForQuad(const FloatQuad& quad)
{
    RefPtr<TypeBuilder::Array<double> > array = TypeBuilder::Array<double>::create();
    array->addItem(quad.p1().x());
    array->addItem(quad.p1().y());
    array->addItem(quad.p2().x());
    array->addItem(quad.p2().y());
    array->addItem(quad.p3().x());
    array->addItem(quad.p3().y());
    array->addItem(quad.p4().x());
    array->addItem(quad.p4().y());
    return array.release();
}

void InspectorDOMAgent::getBoxModel(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::DOM::BoxModel>& model)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return;

    Vector<FloatQuad> quads;
    bool isInlineOrBox = m_overlay->getBoxModel(node, &quads);
    if (!isInlineOrBox) {
        *errorString = "Could not compute box model.";
        return;
    }

    RenderObject* renderer = node->renderer();
    LocalFrame* frame = node->document().frame();
    FrameView* view = frame->view();

    IntRect boundingBox = pixelSnappedIntRect(view->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
    RefPtr<TypeBuilder::DOM::ShapeOutsideInfo> shapeOutsideInfo = m_overlay->buildObjectForShapeOutside(node);

    model = TypeBuilder::DOM::BoxModel::create()
        .setContent(buildArrayForQuad(quads.at(3)))
        .setPadding(buildArrayForQuad(quads.at(2)))
        .setBorder(buildArrayForQuad(quads.at(1)))
        .setMargin(buildArrayForQuad(quads.at(0)))
        .setWidth(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetWidth(), modelObject) : boundingBox.width())
        .setHeight(modelObject ? adjustForAbsoluteZoom(modelObject->pixelSnappedOffsetHeight(), modelObject) : boundingBox.height());
    if (shapeOutsideInfo)
        model->setShapeOutside(shapeOutsideInfo);
}

void InspectorDOMAgent::getNodeForLocation(ErrorString* errorString, int x, int y, int* nodeId)
{
    if (!pushDocumentUponHandlelessOperation(errorString))
        return;

    Node* node = hoveredNodeForPoint(m_document->frame(), IntPoint(x, y), false);
    if (!node) {
        *errorString = "No node found at given location";
        return;
    }
    *nodeId = pushNodePathToFrontend(node);
}

void InspectorDOMAgent::resolveNode(ErrorString* errorString, int nodeId, const String* const objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& result)
{
    String objectGroupName = objectGroup ? *objectGroup : "";
    Node* node = nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return;
    }
    RefPtr<TypeBuilder::Runtime::RemoteObject> object = resolveNode(node, objectGroupName);
    if (!object) {
        *errorString = "Node with given id does not belong to the document";
        return;
    }
    result = object;
}

void InspectorDOMAgent::getAttributes(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Array<String> >& result)
{
    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return;

    result = buildArrayForElementAttributes(element);
}

void InspectorDOMAgent::requestNode(ErrorString*, const String& objectId, int* nodeId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    Node* node = injectedScript.nodeForObjectId(objectId);
    if (node)
        *nodeId = pushNodePathToFrontend(node);
    else
        *nodeId = 0;
}

// static
String InspectorDOMAgent::documentURLString(Document* document)
{
    if (!document || document->url().isNull())
        return "";
    return document->url().string();
}

static String documentBaseURLString(Document* document)
{
    return document->completeURL("").string();
}

static TypeBuilder::DOM::ShadowRootType::Enum shadowRootType(ShadowRoot* shadowRoot)
{
    switch (shadowRoot->type()) {
    case ShadowRoot::UserAgentShadowRoot:
        return TypeBuilder::DOM::ShadowRootType::User_agent;
    case ShadowRoot::AuthorShadowRoot:
        return TypeBuilder::DOM::ShadowRootType::Author;
    }
    ASSERT_NOT_REACHED();
    return TypeBuilder::DOM::ShadowRootType::User_agent;
}

PassRefPtr<TypeBuilder::DOM::Node> InspectorDOMAgent::buildObjectForNode(Node* node, int depth, NodeToIdMap* nodesMap)
{
    int id = bind(node, nodesMap);
    String nodeName;
    String localName;
    String nodeValue;

    switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::COMMENT_NODE:
    case Node::CDATA_SECTION_NODE:
        nodeValue = node->nodeValue();
        if (nodeValue.length() > maxTextSize)
            nodeValue = nodeValue.left(maxTextSize) + ellipsisUChar;
        break;
    case Node::ATTRIBUTE_NODE:
        localName = node->localName();
        break;
    case Node::DOCUMENT_FRAGMENT_NODE:
    case Node::DOCUMENT_NODE:
    case Node::ELEMENT_NODE:
    default:
        nodeName = node->nodeName();
        localName = node->localName();
        break;
    }

    RefPtr<TypeBuilder::DOM::Node> value = TypeBuilder::DOM::Node::create()
        .setNodeId(id)
        .setNodeType(static_cast<int>(node->nodeType()))
        .setNodeName(nodeName)
        .setLocalName(localName)
        .setNodeValue(nodeValue);

    bool forcePushChildren = false;
    if (node->isElementNode()) {
        Element* element = toElement(node);
        value->setAttributes(buildArrayForElementAttributes(element));

        if (node->isFrameOwnerElement()) {
            HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(node);
            LocalFrame* frame = (frameOwner->contentFrame() && frameOwner->contentFrame()->isLocalFrame()) ? toLocalFrame(frameOwner->contentFrame()) : 0;
            if (frame)
                value->setFrameId(m_pageAgent->frameId(frame));
            if (Document* doc = frameOwner->contentDocument())
                value->setContentDocument(buildObjectForNode(doc, 0, nodesMap));
        }

        ElementShadow* shadow = element->shadow();
        if (shadow) {
            RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > shadowRoots = TypeBuilder::Array<TypeBuilder::DOM::Node>::create();
            for (ShadowRoot* root = shadow->youngestShadowRoot(); root; root = root->olderShadowRoot())
                shadowRoots->addItem(buildObjectForNode(root, 0, nodesMap));
            value->setShadowRoots(shadowRoots);
            forcePushChildren = true;
        }

        if (isHTMLLinkElement(*element)) {
            HTMLLinkElement& linkElement = toHTMLLinkElement(*element);
            if (linkElement.isImport() && linkElement.import() && innerParentNode(linkElement.import()) == linkElement)
                value->setImportedDocument(buildObjectForNode(linkElement.import(), 0, nodesMap));
            forcePushChildren = true;
        }

        if (isHTMLTemplateElement(*element)) {
            value->setTemplateContent(buildObjectForNode(toHTMLTemplateElement(*element).content(), 0, nodesMap));
            forcePushChildren = true;
        }

        switch (element->pseudoId()) {
        case BEFORE:
            value->setPseudoType(TypeBuilder::DOM::PseudoType::Before);
            break;
        case AFTER:
            value->setPseudoType(TypeBuilder::DOM::PseudoType::After);
            break;
        default: {
            RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > pseudoElements = buildArrayForPseudoElements(element, nodesMap);
            if (pseudoElements) {
                value->setPseudoElements(pseudoElements.release());
                forcePushChildren = true;
            }
            break;
        }
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
        RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > children = buildArrayForContainerChildren(node, depth, nodesMap);
        if (children->length() > 0 || depth) // Push children along with shadow in any case.
            value->setChildren(children.release());
    }

    return value.release();
}

PassRefPtr<TypeBuilder::Array<String> > InspectorDOMAgent::buildArrayForElementAttributes(Element* element)
{
    RefPtr<TypeBuilder::Array<String> > attributesValue = TypeBuilder::Array<String>::create();
    // Go through all attributes and serialize them.
    if (!element->hasAttributes())
        return attributesValue.release();
    AttributeCollection attributes = element->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
        // Add attribute pair
        attributesValue->addItem(it->name().toString());
        attributesValue->addItem(it->value());
    }
    return attributesValue.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > InspectorDOMAgent::buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > children = TypeBuilder::Array<TypeBuilder::DOM::Node>::create();
    if (depth == 0) {
        // Special-case the only text child - pretend that container's children have been requested.
        Node* firstChild = container->firstChild();
        if (firstChild && firstChild->nodeType() == Node::TEXT_NODE && !firstChild->nextSibling()) {
            children->addItem(buildObjectForNode(firstChild, 0, nodesMap));
            m_childrenRequested.add(bind(container, nodesMap));
        }
        return children.release();
    }

    Node* child = innerFirstChild(container);
    depth--;
    m_childrenRequested.add(bind(container, nodesMap));

    while (child) {
        children->addItem(buildObjectForNode(child, depth, nodesMap));
        child = innerNextSibling(child);
    }
    return children.release();
}

PassRefPtr<TypeBuilder::DOM::EventListener> InspectorDOMAgent::buildObjectForEventListener(const RegisteredEventListener& registeredEventListener, const AtomicString& eventType, Node* node, const String* objectGroupId)
{
    RefPtr<EventListener> eventListener = registeredEventListener.listener;
    String sourceName;
    String scriptId;
    int lineNumber;
    if (!eventListenerHandlerLocation(&node->document(), eventListener.get(), sourceName, scriptId, lineNumber))
        return nullptr;

    Document& document = node->document();
    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(lineNumber);
    RefPtr<TypeBuilder::DOM::EventListener> value = TypeBuilder::DOM::EventListener::create()
        .setType(eventType)
        .setUseCapture(registeredEventListener.useCapture)
        .setIsAttribute(eventListener->isAttribute())
        .setNodeId(pushNodePathToFrontend(node))
        .setHandlerBody(eventListenerHandlerBody(&document, eventListener.get()))
        .setLocation(location);
    if (objectGroupId) {
        ScriptValue functionValue = eventListenerHandler(&document, eventListener.get());
        if (!functionValue.isEmpty()) {
            LocalFrame* frame = document.frame();
            if (frame) {
                ScriptState* scriptState = eventListenerHandlerScriptState(frame, eventListener.get());
                if (scriptState) {
                    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
                    if (!injectedScript.isEmpty()) {
                        RefPtr<TypeBuilder::Runtime::RemoteObject> valueJson = injectedScript.wrapObject(functionValue, *objectGroupId);
                        value->setHandler(valueJson);
                    }
                }
            }
        }
    }
    if (!sourceName.isEmpty())
        value->setSourceName(sourceName);
    return value.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > InspectorDOMAgent::buildArrayForPseudoElements(Element* element, NodeToIdMap* nodesMap)
{
    if (!element->pseudoElement(BEFORE) && !element->pseudoElement(AFTER))
        return nullptr;

    RefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > pseudoElements = TypeBuilder::Array<TypeBuilder::DOM::Node>::create();
    if (element->pseudoElement(BEFORE))
        pseudoElements->addItem(buildObjectForNode(element->pseudoElement(BEFORE), 0, nodesMap));
    if (element->pseudoElement(AFTER))
        pseudoElements->addItem(buildObjectForNode(element->pseudoElement(AFTER), 0, nodesMap));
    return pseudoElements.release();
}

Node* InspectorDOMAgent::innerFirstChild(Node* node)
{
    node = node->firstChild();
    while (isWhitespace(node))
        node = node->nextSibling();
    return node;
}

Node* InspectorDOMAgent::innerNextSibling(Node* node)
{
    do {
        node = node->nextSibling();
    } while (isWhitespace(node));
    return node;
}

Node* InspectorDOMAgent::innerPreviousSibling(Node* node)
{
    do {
        node = node->previousSibling();
    } while (isWhitespace(node));
    return node;
}

unsigned InspectorDOMAgent::innerChildNodeCount(Node* node)
{
    unsigned count = 0;
    Node* child = innerFirstChild(node);
    while (child) {
        count++;
        child = innerNextSibling(child);
    }
    return count;
}

Node* InspectorDOMAgent::innerParentNode(Node* node)
{
    if (node->isDocumentNode()) {
        Document* document = toDocument(node);
        if (HTMLImportLoader* loader = document->importLoader())
            return loader->firstImport()->link();
        return document->ownerElement();
    }
    return node->parentOrShadowHostNode();
}

bool InspectorDOMAgent::isWhitespace(Node* node)
{
    //TODO: pull ignoreWhitespace setting from the frontend and use here.
    return node && node->nodeType() == Node::TEXT_NODE && node->nodeValue().stripWhiteSpace().length() == 0;
}

void InspectorDOMAgent::domContentLoadedEventFired(LocalFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // Re-push document once it is loaded.
    discardFrontendBindings();
    if (enabled())
        m_frontend->documentUpdated();
}

void InspectorDOMAgent::invalidateFrameOwnerElement(LocalFrame* frame)
{
    Element* frameOwner = frame->document()->ownerElement();
    if (!frameOwner)
        return;

    int frameOwnerId = m_documentNodeToIdMap->get(frameOwner);
    if (!frameOwnerId)
        return;

    // Re-add frame owner element together with its new children.
    int parentId = m_documentNodeToIdMap->get(innerParentNode(frameOwner));
    m_frontend->childNodeRemoved(parentId, frameOwnerId);
    unbind(frameOwner, m_documentNodeToIdMap.get());

    RefPtr<TypeBuilder::DOM::Node> value = buildObjectForNode(frameOwner, 0, m_documentNodeToIdMap.get());
    Node* previousSibling = innerPreviousSibling(frameOwner);
    int prevId = previousSibling ? m_documentNodeToIdMap->get(previousSibling) : 0;
    m_frontend->childNodeInserted(parentId, prevId, value.release());
}

void InspectorDOMAgent::didCommitLoad(LocalFrame* frame, DocumentLoader* loader)
{
    // FIXME: If "frame" is always guarenteed to be in the same Page as loader->frame()
    // then all we need to check here is loader->frame()->isMainFrame()
    // and we don't need "frame" at all.
    if (!frame->page()->mainFrame()->isLocalFrame())
        return;
    LocalFrame* mainFrame = frame->page()->deprecatedLocalMainFrame();
    if (loader->frame() != mainFrame) {
        invalidateFrameOwnerElement(loader->frame());
        return;
    }

    setDocument(mainFrame->document());
}

void InspectorDOMAgent::didInsertDOMNode(Node* node)
{
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
        m_frontend->childNodeCountUpdated(parentId, count);
    } else {
        // Children have been requested -> return value of a new child.
        Node* prevSibling = innerPreviousSibling(node);
        int prevId = prevSibling ? m_documentNodeToIdMap->get(prevSibling) : 0;
        RefPtr<TypeBuilder::DOM::Node> value = buildObjectForNode(node, 0, m_documentNodeToIdMap.get());
        m_frontend->childNodeInserted(parentId, prevId, value.release());
    }
}

void InspectorDOMAgent::willRemoveDOMNode(Node* node)
{
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
        m_frontend->childNodeCountUpdated(parentId, count);
    } else {
        m_frontend->childNodeRemoved(parentId, m_documentNodeToIdMap->get(node));
    }
    unbind(node, m_documentNodeToIdMap.get());
}

void InspectorDOMAgent::willModifyDOMAttr(Element*, const AtomicString& oldValue, const AtomicString& newValue)
{
    m_suppressAttributeModifiedEvent = (oldValue == newValue);
}

void InspectorDOMAgent::didModifyDOMAttr(Element* element, const AtomicString& name, const AtomicString& value)
{
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

    m_frontend->attributeModified(id, name, value);
}

void InspectorDOMAgent::didRemoveDOMAttr(Element* element, const AtomicString& name)
{
    int id = boundNodeId(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
        return;

    if (m_domListener)
        m_domListener->didModifyDOMAttr(element);

    m_frontend->attributeRemoved(id, name);
}

void InspectorDOMAgent::styleAttributeInvalidated(const WillBeHeapVector<RawPtrWillBeMember<Element> >& elements)
{
    RefPtr<TypeBuilder::Array<int> > nodeIds = TypeBuilder::Array<int>::create();
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
    m_frontend->inlineStyleInvalidated(nodeIds.release());
}

void InspectorDOMAgent::characterDataModified(CharacterData* characterData)
{
    int id = m_documentNodeToIdMap->get(characterData);
    if (!id) {
        // Push text node if it is being created.
        didInsertDOMNode(characterData);
        return;
    }
    m_frontend->characterDataModified(id, characterData->data());
}

void InspectorDOMAgent::didInvalidateStyleAttr(Node* node)
{
    int id = m_documentNodeToIdMap->get(node);
    // If node is not mapped yet -> ignore the event.
    if (!id)
        return;

    if (!m_revalidateStyleAttrTask)
        m_revalidateStyleAttrTask = adoptPtr(new RevalidateStyleAttributeTask(this));
    m_revalidateStyleAttrTask->scheduleFor(toElement(node));
}

void InspectorDOMAgent::didPushShadowRoot(Element* host, ShadowRoot* root)
{
    if (!host->ownerDocument())
        return;

    int hostId = m_documentNodeToIdMap->get(host);
    if (!hostId)
        return;

    pushChildNodesToFrontend(hostId, 1);
    m_frontend->shadowRootPushed(hostId, buildObjectForNode(root, 0, m_documentNodeToIdMap.get()));
}

void InspectorDOMAgent::willPopShadowRoot(Element* host, ShadowRoot* root)
{
    if (!host->ownerDocument())
        return;

    int hostId = m_documentNodeToIdMap->get(host);
    int rootId = m_documentNodeToIdMap->get(root);
    if (hostId && rootId)
        m_frontend->shadowRootPopped(hostId, rootId);
}

void InspectorDOMAgent::frameDocumentUpdated(LocalFrame* frame)
{
    Document* document = frame->document();
    if (!document)
        return;

    Page* page = frame->page();
    ASSERT(page);
    if (frame != page->mainFrame())
        return;

    // Only update the main frame document, nested frame document updates are not required
    // (will be handled by invalidateFrameOwnerElement()).
    setDocument(document);
}

void InspectorDOMAgent::pseudoElementCreated(PseudoElement* pseudoElement)
{
    Element* parent = pseudoElement->parentOrShadowHostElement();
    if (!parent)
        return;
    int parentId = m_documentNodeToIdMap->get(parent);
    if (!parentId)
        return;

    pushChildNodesToFrontend(parentId, 1);
    m_frontend->pseudoElementAdded(parentId, buildObjectForNode(pseudoElement, 0, m_documentNodeToIdMap.get()));
}

void InspectorDOMAgent::pseudoElementDestroyed(PseudoElement* pseudoElement)
{
    int pseudoElementId = m_documentNodeToIdMap->get(pseudoElement);
    if (!pseudoElementId)
        return;

    // If a PseudoElement is bound, its parent element must be bound, too.
    Element* parent = pseudoElement->parentOrShadowHostElement();
    ASSERT(parent);
    int parentId = m_documentNodeToIdMap->get(parent);
    ASSERT(parentId);

    unbind(pseudoElement, m_documentNodeToIdMap.get());
    m_frontend->pseudoElementRemoved(parentId, pseudoElementId);
}

static ShadowRoot* shadowRootForNode(Node* node, const String& type)
{
    if (!node->isElementNode())
        return 0;
    if (type == "a")
        return toElement(node)->shadowRoot();
    if (type == "u")
        return toElement(node)->userAgentShadowRoot();
    return 0;
}

Node* InspectorDOMAgent::nodeForPath(const String& path)
{
    // The path is of form "1,HTML,2,BODY,1,DIV" (<index> and <nodeName> interleaved).
    // <index> may also be "a" (author shadow root) or "u" (user-agent shadow root),
    // in which case <nodeName> MUST be "#document-fragment".
    if (!m_document)
        return 0;

    Node* node = m_document.get();
    Vector<String> pathTokens;
    path.split(",", false, pathTokens);
    if (!pathTokens.size())
        return 0;
    for (size_t i = 0; i < pathTokens.size() - 1; i += 2) {
        bool success = true;
        String& indexValue = pathTokens[i];
        unsigned childNumber = indexValue.toUInt(&success);
        Node* child;
        if (!success) {
            child = shadowRootForNode(node, indexValue);
        } else {
            if (childNumber >= innerChildNodeCount(node))
                return 0;

            child = innerFirstChild(node);
        }
        String childName = pathTokens[i + 1];
        for (size_t j = 0; child && j < childNumber; ++j)
            child = innerNextSibling(child);

        if (!child || child->nodeName() != childName)
            return 0;
        node = child;
    }
    return node;
}

void InspectorDOMAgent::pushNodeByPathToFrontend(ErrorString* errorString, const String& path, int* nodeId)
{
    if (Node* node = nodeForPath(path))
        *nodeId = pushNodePathToFrontend(node);
    else
        *errorString = "No node with given path found";
}

void InspectorDOMAgent::pushNodesByBackendIdsToFrontend(ErrorString* errorString, const RefPtr<JSONArray>& backendNodeIds, RefPtr<TypeBuilder::Array<int> >& result)
{
    result = TypeBuilder::Array<int>::create();
    for (JSONArray::const_iterator it = backendNodeIds->begin(); it != backendNodeIds->end(); ++it) {
        int backendNodeId;

        if (!(*it)->asNumber(&backendNodeId)) {
            *errorString = "Invalid argument type";
            return;
        }

        Node* node = InspectorNodeIds::nodeForId(backendNodeId);
        if (node && node->document().page() == m_pageAgent->page())
            result->addItem(pushNodePathToFrontend(node));
        else
            result->addItem(0);
    }
}

void InspectorDOMAgent::getRelayoutBoundary(ErrorString* errorString, int nodeId, int* relayoutBoundaryNodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return;
    RenderObject* renderer = node->renderer();
    if (!renderer) {
        *errorString = "No renderer for node, perhaps orphan or hidden node";
        return;
    }
    while (renderer && !renderer->isDocumentElement() && !renderer->isRelayoutBoundaryForInspector())
        renderer = renderer->container();
    Node* resultNode = renderer ? renderer->generatingNode() : node->ownerDocument();
    *relayoutBoundaryNodeId = pushNodePathToFrontend(resultNode);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InspectorDOMAgent::resolveNode(Node* node, const String& objectGroup)
{
    Document* document = node->isDocumentNode() ? &node->document() : node->ownerDocument();
    LocalFrame* frame = document ? document->frame() : 0;
    if (!frame)
        return nullptr;

    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(ScriptState::forMainWorld(frame));
    if (injectedScript.isEmpty())
        return nullptr;

    return injectedScript.wrapNode(node, objectGroup);
}

bool InspectorDOMAgent::pushDocumentUponHandlelessOperation(ErrorString* errorString)
{
    // FIXME: Oilpan: .get will be unnecessary if m_document is a Member<>.
    if (!m_documentNodeToIdMap->contains(m_document.get())) {
        RefPtr<TypeBuilder::DOM::Node> root;
        getDocument(errorString, root);
        return errorString->isEmpty();
    }
    return true;
}

} // namespace WebCore

