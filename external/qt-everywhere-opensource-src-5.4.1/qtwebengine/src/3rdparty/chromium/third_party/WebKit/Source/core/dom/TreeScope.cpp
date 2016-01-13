/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/TreeScope.h"

#include "core/HTMLNames.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/IdTargetObserverRegistry.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/TreeScopeAdopter.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/EventPath.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMapElement.h"
#include "core/page/DOMSelection.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderView.h"
#include "wtf/Vector.h"

namespace WebCore {

using namespace HTMLNames;

TreeScope::TreeScope(ContainerNode& rootNode, Document& document)
    : m_rootNode(&rootNode)
    , m_document(&document)
    , m_parentTreeScope(&document)
#if !ENABLE(OILPAN)
    , m_guardRefCount(0)
#endif
    , m_idTargetObserverRegistry(IdTargetObserverRegistry::create())
{
    ASSERT(rootNode != document);
#if !ENABLE(OILPAN)
    m_parentTreeScope->guardRef();
#endif
    m_rootNode->setTreeScope(this);
}

TreeScope::TreeScope(Document& document)
    : m_rootNode(document)
    , m_document(&document)
    , m_parentTreeScope(nullptr)
#if !ENABLE(OILPAN)
    , m_guardRefCount(0)
#endif
    , m_idTargetObserverRegistry(IdTargetObserverRegistry::create())
{
    m_rootNode->setTreeScope(this);
}

TreeScope::~TreeScope()
{
#if !ENABLE(OILPAN)
    ASSERT(!m_guardRefCount);
    m_rootNode->setTreeScope(0);

    if (m_selection) {
        m_selection->clearTreeScope();
        m_selection = nullptr;
    }

    if (m_parentTreeScope)
        m_parentTreeScope->guardDeref();
#endif
}

TreeScope* TreeScope::olderShadowRootOrParentTreeScope() const
{
    if (rootNode().isShadowRoot()) {
        if (ShadowRoot* olderShadowRoot = toShadowRoot(rootNode()).olderShadowRoot())
            return olderShadowRoot;
    }
    return parentTreeScope();
}

bool TreeScope::isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(const TreeScope& scope) const
{
    for (const TreeScope* current = &scope; current; current = current->olderShadowRootOrParentTreeScope()) {
        if (current == this)
            return true;
    }
    return false;
}

bool TreeScope::rootNodeHasTreeSharedParent() const
{
    return rootNode().hasTreeSharedParent();
}

#if !ENABLE(OILPAN)
void TreeScope::destroyTreeScopeData()
{
    m_elementsById.clear();
    m_imageMapsByName.clear();
    m_labelsByForAttribute.clear();
}
#endif

void TreeScope::setParentTreeScope(TreeScope& newParentScope)
{
    // A document node cannot be re-parented.
    ASSERT(!rootNode().isDocumentNode());

#if !ENABLE(OILPAN)
    newParentScope.guardRef();
    if (m_parentTreeScope)
        m_parentTreeScope->guardDeref();
#endif
    m_parentTreeScope = &newParentScope;
    setDocument(newParentScope.document());
}

Element* TreeScope::getElementById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return 0;
    if (!m_elementsById)
        return 0;
    return m_elementsById->getElementById(elementId.impl(), this);
}

const Vector<Element*>& TreeScope::getAllElementsById(const AtomicString& elementId) const
{
    DEFINE_STATIC_LOCAL(Vector<Element*>, emptyVector, ());
    if (elementId.isEmpty())
        return emptyVector;
    if (!m_elementsById)
        return emptyVector;
    return m_elementsById->getAllElementsById(elementId.impl(), this);
}

void TreeScope::addElementById(const AtomicString& elementId, Element* element)
{
    if (!m_elementsById)
        m_elementsById = adoptPtr(new DocumentOrderedMap);
    m_elementsById->add(elementId.impl(), element);
    m_idTargetObserverRegistry->notifyObservers(elementId);
}

void TreeScope::removeElementById(const AtomicString& elementId, Element* element)
{
    if (!m_elementsById)
        return;
    m_elementsById->remove(elementId.impl(), element);
    m_idTargetObserverRegistry->notifyObservers(elementId);
}

Node* TreeScope::ancestorInThisScope(Node* node) const
{
    while (node) {
        if (node->treeScope() == this)
            return node;
        if (!node->isInShadowTree())
            return 0;

        node = node->shadowHost();
    }

    return 0;
}

void TreeScope::addImageMap(HTMLMapElement* imageMap)
{
    StringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    if (!m_imageMapsByName)
        m_imageMapsByName = adoptPtr(new DocumentOrderedMap);
    m_imageMapsByName->add(name, imageMap);
}

void TreeScope::removeImageMap(HTMLMapElement* imageMap)
{
    if (!m_imageMapsByName)
        return;
    StringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    m_imageMapsByName->remove(name, imageMap);
}

HTMLMapElement* TreeScope::getImageMap(const String& url) const
{
    if (url.isNull())
        return 0;
    if (!m_imageMapsByName)
        return 0;
    size_t hashPos = url.find('#');
    String name = (hashPos == kNotFound ? url : url.substring(hashPos + 1)).impl();
    if (rootNode().document().isHTMLDocument())
        return toHTMLMapElement(m_imageMapsByName->getElementByLowercasedMapName(AtomicString(name.lower()).impl(), this));
    return toHTMLMapElement(m_imageMapsByName->getElementByMapName(AtomicString(name).impl(), this));
}

HitTestResult hitTestInDocument(const Document* document, int x, int y)
{
    LocalFrame* frame = document->frame();

    if (!frame)
        return HitTestResult();
    FrameView* frameView = frame->view();
    if (!frameView)
        return HitTestResult();

    float scaleFactor = frame->pageZoomFactor();
    IntPoint point = roundedIntPoint(FloatPoint(x * scaleFactor  + frameView->scrollX(), y * scaleFactor + frameView->scrollY()));

    if (!frameView->visibleContentRect().contains(point))
        return HitTestResult();

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
    HitTestResult result(point);
    document->renderView()->hitTest(request, result);
    return result;
}

Element* TreeScope::elementFromPoint(int x, int y) const
{
    HitTestResult result = hitTestInDocument(&rootNode().document(), x, y);
    Node* node = result.innerNode();
    if (!node || node->isDocumentNode())
        return 0;
    if (node->isPseudoElement() || node->isTextNode())
        node = node->parentOrShadowHostNode();
    ASSERT(!node || node->isElementNode() || node->isShadowRoot());
    node = ancestorInThisScope(node);
    if (!node || !node->isElementNode())
        return 0;
    return toElement(node);
}

void TreeScope::addLabel(const AtomicString& forAttributeValue, HTMLLabelElement* element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->add(forAttributeValue.impl(), element);
}

void TreeScope::removeLabel(const AtomicString& forAttributeValue, HTMLLabelElement* element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->remove(forAttributeValue.impl(), element);
}

HTMLLabelElement* TreeScope::labelElementForId(const AtomicString& forAttributeValue)
{
    if (forAttributeValue.isEmpty())
        return 0;

    if (!m_labelsByForAttribute) {
        // Populate the map on first access.
        m_labelsByForAttribute = adoptPtr(new DocumentOrderedMap);
        for (HTMLLabelElement* label = Traversal<HTMLLabelElement>::firstWithin(rootNode()); label; label = Traversal<HTMLLabelElement>::next(*label)) {
            const AtomicString& forValue = label->fastGetAttribute(forAttr);
            if (!forValue.isEmpty())
                addLabel(forValue, label);
        }
    }

    return toHTMLLabelElement(m_labelsByForAttribute->getElementByLabelForAttribute(forAttributeValue.impl(), this));
}

DOMSelection* TreeScope::getSelection() const
{
    if (!rootNode().document().frame())
        return 0;

    if (m_selection)
        return m_selection.get();

    // FIXME: The correct selection in Shadow DOM requires that Position can have a ShadowRoot
    // as a container.
    // See https://bugs.webkit.org/show_bug.cgi?id=82697
    m_selection = DOMSelection::create(this);
    return m_selection.get();
}

Element* TreeScope::findAnchor(const String& name)
{
    if (name.isEmpty())
        return 0;
    if (Element* element = getElementById(AtomicString(name)))
        return element;
    for (HTMLAnchorElement* anchor = Traversal<HTMLAnchorElement>::firstWithin(rootNode()); anchor; anchor = Traversal<HTMLAnchorElement>::next(*anchor)) {
        if (rootNode().document().inQuirksMode()) {
            // Quirks mode, case insensitive comparison of names.
            if (equalIgnoringCase(anchor->name(), name))
                return anchor;
        } else {
            // Strict mode, names need to match exactly.
            if (anchor->name() == name)
                return anchor;
        }
    }
    return 0;
}

bool TreeScope::applyAuthorStyles() const
{
    return rootNode().isDocumentNode();
}

void TreeScope::adoptIfNeeded(Node& node)
{
    ASSERT(this);
    ASSERT(!node.isDocumentNode());
#if !ENABLE(OILPAN)
    ASSERT_WITH_SECURITY_IMPLICATION(!node.m_deletionHasBegun);
#endif
    TreeScopeAdopter adopter(node, *this);
    if (adopter.needsScopeChange())
        adopter.execute();
}

static Element* focusedFrameOwnerElement(Frame* focusedFrame, Frame* currentFrame)
{
    for (; focusedFrame; focusedFrame = focusedFrame->tree().parent()) {
        if (focusedFrame->tree().parent() == currentFrame) {
            // FIXME: This won't work for OOPI.
            return focusedFrame->deprecatedLocalOwner();
        }
    }
    return 0;
}

Element* TreeScope::adjustedFocusedElement() const
{
    Document& document = rootNode().document();
    Element* element = document.focusedElement();
    if (!element && document.page())
        element = focusedFrameOwnerElement(document.page()->focusController().focusedFrame(), document.frame());
    if (!element)
        return 0;

    EventPath eventPath(element);
    for (size_t i = 0; i < eventPath.size(); ++i) {
        if (eventPath[i].node() == rootNode()) {
            // eventPath.at(i).target() is one of the followings:
            // - InsertionPoint
            // - shadow host
            // - Document::focusedElement()
            // So, it's safe to do toElement().
            return toElement(eventPath[i].target()->toNode());
        }
    }
    return 0;
}

unsigned short TreeScope::comparePosition(const TreeScope& otherScope) const
{
    if (otherScope == this)
        return Node::DOCUMENT_POSITION_EQUIVALENT;

    Vector<const TreeScope*, 16> chain1;
    Vector<const TreeScope*, 16> chain2;
    const TreeScope* current;
    for (current = this; current; current = current->parentTreeScope())
        chain1.append(current);
    for (current = &otherScope; current; current = current->parentTreeScope())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();
    if (chain1[index1 - 1] != chain2[index2 - 1])
        return Node::DOCUMENT_POSITION_DISCONNECTED | Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

    for (unsigned i = std::min(index1, index2); i; --i) {
        const TreeScope* child1 = chain1[--index1];
        const TreeScope* child2 = chain2[--index2];
        if (child1 != child2) {
            Node* shadowHost1 = child1->rootNode().parentOrShadowHostNode();
            Node* shadowHost2 = child2->rootNode().parentOrShadowHostNode();
            if (shadowHost1 != shadowHost2)
                return shadowHost1->compareDocumentPositionInternal(shadowHost2, Node::TreatShadowTreesAsDisconnected);

            for (const ShadowRoot* child = toShadowRoot(child2->rootNode()).olderShadowRoot(); child; child = child->olderShadowRoot())
                if (child == child1)
                    return Node::DOCUMENT_POSITION_FOLLOWING;

            return Node::DOCUMENT_POSITION_PRECEDING;
        }
    }

    // There was no difference between the two parent chains, i.e., one was a subset of the other. The shorter
    // chain is the ancestor.
    return index1 < index2 ?
        Node::DOCUMENT_POSITION_FOLLOWING | Node::DOCUMENT_POSITION_CONTAINED_BY :
        Node::DOCUMENT_POSITION_PRECEDING | Node::DOCUMENT_POSITION_CONTAINS;
}

const TreeScope* TreeScope::commonAncestorTreeScope(const TreeScope& other) const
{
    Vector<const TreeScope*, 16> thisChain;
    for (const TreeScope* tree = this; tree; tree = tree->parentTreeScope())
        thisChain.append(tree);

    Vector<const TreeScope*, 16> otherChain;
    for (const TreeScope* tree = &other; tree; tree = tree->parentTreeScope())
        otherChain.append(tree);

    // Keep popping out the last elements of these chains until a mismatched pair is found. If |this| and |other|
    // belong to different documents, null will be returned.
    const TreeScope* lastAncestor = 0;
    while (!thisChain.isEmpty() && !otherChain.isEmpty() && thisChain.last() == otherChain.last()) {
        lastAncestor = thisChain.last();
        thisChain.removeLast();
        otherChain.removeLast();
    }
    return lastAncestor;
}

TreeScope* TreeScope::commonAncestorTreeScope(TreeScope& other)
{
    return const_cast<TreeScope*>(static_cast<const TreeScope&>(*this).commonAncestorTreeScope(other));
}

static void listTreeScopes(Node* node, Vector<TreeScope*, 5>& treeScopes)
{
    while (true) {
        treeScopes.append(&node->treeScope());
        Element* ancestor = node->shadowHost();
        if (!ancestor)
            break;
        node = ancestor;
    }
}

TreeScope* commonTreeScope(Node* nodeA, Node* nodeB)
{
    if (!nodeA || !nodeB)
        return 0;

    if (nodeA->treeScope() == nodeB->treeScope())
        return &nodeA->treeScope();

    Vector<TreeScope*, 5> treeScopesA;
    listTreeScopes(nodeA, treeScopesA);

    Vector<TreeScope*, 5> treeScopesB;
    listTreeScopes(nodeB, treeScopesB);

    size_t indexA = treeScopesA.size();
    size_t indexB = treeScopesB.size();

    for (; indexA > 0 && indexB > 0 && treeScopesA[indexA - 1] == treeScopesB[indexB - 1]; --indexA, --indexB) { }

    return treeScopesA[indexA] == treeScopesB[indexB] ? treeScopesA[indexA] : 0;
}

#if SECURITY_ASSERT_ENABLED && !ENABLE(OILPAN)
bool TreeScope::deletionHasBegun()
{
    return rootNode().m_deletionHasBegun;
}

void TreeScope::beginDeletion()
{
    rootNode().m_deletionHasBegun = true;
}
#endif

#if !ENABLE(OILPAN)
int TreeScope::refCount() const
{
    return rootNode().refCount();
}
#endif

bool TreeScope::isInclusiveAncestorOf(const TreeScope& scope) const
{
    for (const TreeScope* current = &scope; current; current = current->parentTreeScope()) {
        if (current == this)
            return true;
    }
    return false;
}

Element* TreeScope::getElementByAccessKey(const String& key) const
{
    if (key.isEmpty())
        return 0;
    Element* result = 0;
    Node& root = rootNode();
    for (Element* element = ElementTraversal::firstWithin(root); element; element = ElementTraversal::next(*element, &root)) {
        if (equalIgnoringCase(element->fastGetAttribute(accesskeyAttr), key))
            result = element;
        for (ShadowRoot* shadowRoot = element->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
            if (Element* shadowResult = shadowRoot->getElementByAccessKey(key))
                result = shadowResult;
        }
    }
    return result;
}

void TreeScope::setNeedsStyleRecalcForViewportUnits()
{
    for (Element* element = ElementTraversal::firstWithin(rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        for (ShadowRoot* root = element->youngestShadowRoot(); root; root = root->olderShadowRoot())
            root->setNeedsStyleRecalcForViewportUnits();
        RenderStyle* style = element->renderStyle();
        if (style && style->hasViewportUnits())
            element->setNeedsStyleRecalc(LocalStyleChange);
    }
}

void TreeScope::trace(Visitor* visitor)
{
    visitor->trace(m_rootNode);
    visitor->trace(m_document);
    visitor->trace(m_parentTreeScope);
    visitor->trace(m_idTargetObserverRegistry);
    visitor->trace(m_selection);
}

} // namespace WebCore
