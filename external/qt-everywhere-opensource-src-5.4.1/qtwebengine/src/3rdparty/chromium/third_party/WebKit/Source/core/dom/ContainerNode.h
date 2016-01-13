/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef ContainerNode_h
#define ContainerNode_h

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Node.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ClassCollection;
class ExceptionState;
class FloatPoint;
class HTMLCollection;
class StaticNodeList;
class TagCollection;

namespace Private {
    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer&);
}

enum DynamicRestyleFlags {
    ChildrenOrSiblingsAffectedByFocus = 1 << 0,
    ChildrenOrSiblingsAffectedByHover = 1 << 1,
    ChildrenOrSiblingsAffectedByActive = 1 << 2,
    ChildrenOrSiblingsAffectedByDrag = 1 << 3,
    ChildrenAffectedByFirstChildRules = 1 << 4,
    ChildrenAffectedByLastChildRules = 1 << 5,
    ChildrenAffectedByDirectAdjacentRules = 1 << 6,
    ChildrenAffectedByIndirectAdjacentRules = 1 << 7,
    ChildrenAffectedByForwardPositionalRules = 1 << 8,
    ChildrenAffectedByBackwardPositionalRules = 1 << 9,

    NumberOfDynamicRestyleFlags = 10,
};

// This constant controls how much buffer is initially allocated
// for a Node Vector that is used to store child Nodes of a given Node.
// FIXME: Optimize the value.
const int initialNodeVectorSize = 11;
typedef WillBeHeapVector<RefPtrWillBeMember<Node>, initialNodeVectorSize> NodeVector;

class ContainerNode : public Node {
public:
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    Node* lastChild() const { return m_lastChild; }
    bool hasChildren() const { return m_firstChild; }

    bool hasOneChild() const { return m_firstChild && !m_firstChild->nextSibling(); }
    bool hasOneTextChild() const { return hasOneChild() && m_firstChild->isTextNode(); }
    bool hasChildCount(unsigned) const;

    PassRefPtrWillBeRawPtr<HTMLCollection> children();

    unsigned countChildren() const;
    Node* traverseToChildAt(unsigned index) const;

    PassRefPtrWillBeRawPtr<Element> querySelector(const AtomicString& selectors, ExceptionState&);
    PassRefPtrWillBeRawPtr<StaticNodeList> querySelectorAll(const AtomicString& selectors, ExceptionState&);

    void insertBefore(PassRefPtrWillBeRawPtr<Node> newChild, Node* refChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    void replaceChild(PassRefPtrWillBeRawPtr<Node> newChild, Node* oldChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    void removeChild(Node* child, ExceptionState& = ASSERT_NO_EXCEPTION);
    void appendChild(PassRefPtrWillBeRawPtr<Node> newChild, ExceptionState& = ASSERT_NO_EXCEPTION);

    Element* getElementById(const AtomicString& id) const;
    PassRefPtrWillBeRawPtr<TagCollection> getElementsByTagName(const AtomicString&);
    PassRefPtrWillBeRawPtr<TagCollection> getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName);
    PassRefPtrWillBeRawPtr<NameNodeList> getElementsByName(const AtomicString& elementName);
    PassRefPtrWillBeRawPtr<ClassCollection> getElementsByClassName(const AtomicString& classNames);
    PassRefPtrWillBeRawPtr<RadioNodeList> radioNodeList(const AtomicString&, bool onlyMatchImgElements = false);

    // These methods are only used during parsing.
    // They don't send DOM mutation events or handle reparenting.
    void parserAppendChild(PassRefPtrWillBeRawPtr<Node>);
    void parserRemoveChild(Node&);
    void parserInsertBefore(PassRefPtrWillBeRawPtr<Node> newChild, Node& refChild);
    void parserTakeAllChildrenFrom(ContainerNode&);

    void removeChildren();

    void cloneChildNodes(ContainerNode* clone);

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual LayoutRect boundingBox() const OVERRIDE FINAL;
    virtual void setFocus(bool) OVERRIDE;
    void focusStateChanged();
    virtual void setActive(bool = true) OVERRIDE;
    virtual void setHovered(bool = true) OVERRIDE;

    bool childrenOrSiblingsAffectedByFocus() const { return hasRestyleFlag(ChildrenOrSiblingsAffectedByFocus); }
    void setChildrenOrSiblingsAffectedByFocus() { setRestyleFlag(ChildrenOrSiblingsAffectedByFocus); }

    bool childrenOrSiblingsAffectedByHover() const { return hasRestyleFlag(ChildrenOrSiblingsAffectedByHover); }
    void setChildrenOrSiblingsAffectedByHover() { setRestyleFlag(ChildrenOrSiblingsAffectedByHover); }

    bool childrenOrSiblingsAffectedByActive() const { return hasRestyleFlag(ChildrenOrSiblingsAffectedByActive); }
    void setChildrenOrSiblingsAffectedByActive() { setRestyleFlag(ChildrenOrSiblingsAffectedByActive); }

    bool childrenOrSiblingsAffectedByDrag() const { return hasRestyleFlag(ChildrenOrSiblingsAffectedByDrag); }
    void setChildrenOrSiblingsAffectedByDrag() { setRestyleFlag(ChildrenOrSiblingsAffectedByDrag); }

    bool childrenAffectedByPositionalRules() const { return hasRestyleFlag(ChildrenAffectedByForwardPositionalRules) || hasRestyleFlag(ChildrenAffectedByBackwardPositionalRules); }

    bool childrenAffectedByFirstChildRules() const { return hasRestyleFlag(ChildrenAffectedByFirstChildRules); }
    void setChildrenAffectedByFirstChildRules() { setRestyleFlag(ChildrenAffectedByFirstChildRules); }

    bool childrenAffectedByLastChildRules() const { return hasRestyleFlag(ChildrenAffectedByLastChildRules); }
    void setChildrenAffectedByLastChildRules() { setRestyleFlag(ChildrenAffectedByLastChildRules); }

    bool childrenAffectedByDirectAdjacentRules() const { return hasRestyleFlag(ChildrenAffectedByDirectAdjacentRules); }
    void setChildrenAffectedByDirectAdjacentRules() { setRestyleFlag(ChildrenAffectedByDirectAdjacentRules); }

    bool childrenAffectedByIndirectAdjacentRules() const { return hasRestyleFlag(ChildrenAffectedByIndirectAdjacentRules); }
    void setChildrenAffectedByIndirectAdjacentRules() { setRestyleFlag(ChildrenAffectedByIndirectAdjacentRules); }

    bool childrenAffectedByForwardPositionalRules() const { return hasRestyleFlag(ChildrenAffectedByForwardPositionalRules); }
    void setChildrenAffectedByForwardPositionalRules() { setRestyleFlag(ChildrenAffectedByForwardPositionalRules); }

    bool childrenAffectedByBackwardPositionalRules() const { return hasRestyleFlag(ChildrenAffectedByBackwardPositionalRules); }
    void setChildrenAffectedByBackwardPositionalRules() { setRestyleFlag(ChildrenAffectedByBackwardPositionalRules); }

    // FIXME: These methods should all be renamed to something better than "check",
    // since it's not clear that they alter the style bits of siblings and children.
    void checkForChildrenAdjacentRuleChanges();
    void checkForSiblingStyleChanges(bool finishedParsingCallback, Node* beforeChange, Node* afterChange, int childCountDelta);

    bool childrenSupportStyleSharing() const { return !hasRestyleFlags(); }

    // -----------------------------------------------------------------------------
    // Notification of document structure changes (see core/dom/Node.h for more notification methods)

    // Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
    // node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    void disconnectDescendantFrames();

    virtual void trace(Visitor*) OVERRIDE;

    enum ChildrenChangeSource { ChildrenChangeSourceAPI, ChildrenChangeSourceParser };
    void notifyNodeInserted(Node&, ChildrenChangeSource = ChildrenChangeSourceAPI);
    void notifyNodeRemoved(Node&);

protected:
    ContainerNode(TreeScope*, ConstructionType = CreateContainer);

    template<class GenericNode, class GenericNodeContainer>
    friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer&);

#if !ENABLE(OILPAN)
    void removeDetachedChildren();
#endif

    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }

private:
    void removeBetween(Node* previousChild, Node* nextChild, Node& oldChild);
    void insertBeforeCommon(Node& nextChild, Node& oldChild);
    void appendChildCommon(Node& child);
    void updateTreeAfterInsertion(Node& child);
    void willRemoveChildren();
    void willRemoveChild(Node& child);

    void notifyNodeInsertedInternal(Node&, NodeVector& postInsertionNotificationTargets);

    bool hasRestyleFlag(DynamicRestyleFlags mask) const { return hasRareData() && hasRestyleFlagInternal(mask); }
    bool hasRestyleFlags() const { return hasRareData() && hasRestyleFlagsInternal(); }
    void setRestyleFlag(DynamicRestyleFlags);
    bool hasRestyleFlagInternal(DynamicRestyleFlags) const;
    bool hasRestyleFlagsInternal() const;

    inline bool checkAcceptChildGuaranteedNodeTypes(const Node& newChild, ExceptionState&) const;
    inline bool checkAcceptChild(const Node* newChild, const Node* oldChild, ExceptionState&) const;
    inline bool containsConsideringHostElements(const Node&) const;
    inline bool isChildTypeAllowed(const Node& child) const;

    void attachChildren(const AttachContext& = AttachContext());
    void detachChildren(const AttachContext& = AttachContext());

    bool getUpperLeftCorner(FloatPoint&) const;
    bool getLowerRightCorner(FloatPoint&) const;

    RawPtrWillBeMember<Node> m_firstChild;
    RawPtrWillBeMember<Node> m_lastChild;
};

#ifndef NDEBUG
bool childAttachedAllowedWhenAttachingChildren(ContainerNode*);
#endif

DEFINE_NODE_TYPE_CASTS(ContainerNode, isContainerNode());

inline bool ContainerNode::hasChildCount(unsigned count) const
{
    Node* child = m_firstChild;
    while (count && child) {
        child = child->nextSibling();
        --count;
    }
    return !count && !child;
}

inline ContainerNode::ContainerNode(TreeScope* treeScope, ConstructionType type)
    : Node(treeScope, type)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
{
}

inline void ContainerNode::attachChildren(const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        ASSERT(child->needsAttach() || childAttachedAllowedWhenAttachingChildren(this));
        if (child->needsAttach())
            child->attach(childrenContext);
    }
}

inline void ContainerNode::detachChildren(const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling())
        child->detach(childrenContext);
}

inline unsigned Node::countChildren() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->countChildren();
}

inline Node* Node::traverseToChildAt(unsigned index) const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->traverseToChildAt(index);
}

inline Node* Node::firstChild() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->firstChild();
}

inline Node* Node::lastChild() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->lastChild();
}

inline Node& Node::highestAncestorOrSelf() const
{
    Node* node = const_cast<Node*>(this);
    Node* highest = node;
    for (; node; node = node->parentNode())
        highest = node;
    return *highest;
}

inline ContainerNode* Node::parentElementOrShadowRoot() const
{
    ContainerNode* parent = parentNode();
    return parent && (parent->isElementNode() || parent->isShadowRoot()) ? parent : 0;
}

inline ContainerNode* Node::parentElementOrDocumentFragment() const
{
    ContainerNode* parent = parentNode();
    return parent && (parent->isElementNode() || parent->isDocumentFragment()) ? parent : 0;
}

inline void getChildNodes(Node& node, NodeVector& nodes)
{
    ASSERT(!nodes.size());
    for (Node* child = node.firstChild(); child; child = child->nextSibling())
        nodes.append(child);
}

} // namespace WebCore

#endif // ContainerNode_h
