/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Position_h
#define Position_h

#include "core/CoreExport.h"
#include "core/dom/ContainerNode.h"
#include "core/editing/EditingBoundary.h"
#include "core/editing/EditingStrategy.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class Element;
class Node;
class LayoutObject;
class Text;
enum class TextAffinity;
class TreeScope;

enum class PositionAnchorType : unsigned {
    OffsetInAnchor,
    BeforeAnchor,
    AfterAnchor,
    BeforeChildren,
    AfterChildren,
};

// Instances of |PositionTemplate<Strategy>| are immutable.
template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT PositionTemplate {
    DISALLOW_NEW();
public:

    PositionTemplate()
        : m_offset(0)
        , m_anchorType(PositionAnchorType::OffsetInAnchor)
    {
    }

    static const TreeScope* commonAncestorTreeScope(const PositionTemplate<Strategy>&, const PositionTemplate<Strategy>& b);
    static PositionTemplate<Strategy> editingPositionOf(Node* anchorNode, int offset);

    // For creating before/after positions:
    PositionTemplate(Node* anchorNode, PositionAnchorType);

    // For creating offset positions:
    // FIXME: This constructor should eventually go away. See bug 63040.
    PositionTemplate(Node* anchorNode, int offset);

    PositionTemplate(const PositionTemplate&);

    PositionAnchorType anchorType() const { return m_anchorType; }
    bool isAfterAnchor() const { return m_anchorType == PositionAnchorType::AfterAnchor; }
    bool isAfterChildren() const { return m_anchorType == PositionAnchorType::AfterChildren; }
    bool isBeforeAnchor() const { return m_anchorType == PositionAnchorType::BeforeAnchor; }
    bool isBeforeChildren() const { return m_anchorType == PositionAnchorType::BeforeChildren; }
    bool isOffsetInAnchor() const { return m_anchorType == PositionAnchorType::OffsetInAnchor; }

    // These are always DOM compliant values.  Editing positions like [img, 0] (aka [img, before])
    // will return img->parentNode() and img->nodeIndex() from these functions.
    Node* computeContainerNode() const; // null for a before/after position anchored to a node with no parent

    int computeOffsetInContainerNode() const; // O(n) for before/after-anchored positions, O(1) for parent-anchored positions
    PositionTemplate<Strategy> parentAnchoredEquivalent() const; // Convenience method for DOM positions that also fixes up some positions for editing

    // Returns |PositionIsAnchor| type |Position| which is compatible with
    // |RangeBoundaryPoint| as safe to pass |Range| constructor. Return value
    // of this function is different from |parentAnchoredEquivalent()| which
    // returns editing specific position.
    PositionTemplate<Strategy> toOffsetInAnchor() const;

    // Inline O(1) access for Positions which callers know to be parent-anchored
    int offsetInContainerNode() const
    {
        DCHECK(isOffsetInAnchor());
        return m_offset;
    }

    // Returns an offset for editing based on anchor type for using with
    // |anchorNode()| function:
    //   - OffsetInAnchor  m_offset
    //   - BeforeChildren  0
    //   - BeforeAnchor    0
    //   - AfterChildren   last editing offset in anchor node
    //   - AfterAnchor     last editing offset in anchor node
    // Editing operations will change in anchor node rather than nodes around
    // anchor node.
    int computeEditingOffset() const;

    // These are convenience methods which are smart about whether the position is neighbor anchored or parent anchored
    Node* computeNodeBeforePosition() const;
    Node* computeNodeAfterPosition() const;

    // Returns node as |Range::firstNode()|. This position must be a
    // |PositionAnchorType::OffsetInAhcor| to behave as |Range| boundary point.
    Node* nodeAsRangeFirstNode() const;

    // Similar to |nodeAsRangeLastNode()|, but returns a node in a range.
    Node* nodeAsRangeLastNode() const;

    // Returns a node as past last as same as |Range::pastLastNode()|. This
    // function is supposed to used in HTML serialization and plain text
    // iterator. This position must be a |PositionAnchorType::OffsetInAhcor| to
    // behave as |Range| boundary point.
    Node* nodeAsRangePastLastNode() const;

    Node* commonAncestorContainer(const PositionTemplate<Strategy>&) const;

    Node* anchorNode() const { return m_anchorNode.get(); }

    Document* document() const { return m_anchorNode ? &m_anchorNode->document() : 0; }
    bool inShadowIncludingDocument() const { return m_anchorNode && m_anchorNode->inShadowIncludingDocument(); }

    bool isNull() const { return !m_anchorNode; }
    bool isNotNull() const { return m_anchorNode; }
    bool isOrphan() const { return m_anchorNode && !m_anchorNode->inShadowIncludingDocument(); }

    // Note: Comparison of positions require both parameters are non-null. You
    // should check null-position before comparing them.
    // TODO(yosin): We should use |Position::operator<()| instead of
    // |Position::comapreTo()| to utilize |DHCECK_XX()|.
    int compareTo(const PositionTemplate<Strategy>&) const;
    bool operator<(const PositionTemplate<Strategy>&) const;
    bool operator<=(const PositionTemplate<Strategy>&) const;
    bool operator>(const PositionTemplate<Strategy>&) const;
    bool operator>=(const PositionTemplate<Strategy>&) const;

    // These can be either inside or just before/after the node, depending on
    // if the node is ignored by editing or not.
    // FIXME: These should go away. They only make sense for legacy positions.
    bool atFirstEditingPositionForNode() const;
    bool atLastEditingPositionForNode() const;

    bool atStartOfTree() const;
    bool atEndOfTree() const;

    static PositionTemplate<Strategy> beforeNode(Node* anchorNode);
    static PositionTemplate<Strategy> afterNode(Node* anchorNode);
    static PositionTemplate<Strategy> inParentBeforeNode(const Node& anchorNode);
    static PositionTemplate<Strategy> inParentAfterNode(const Node& anchorNode);
    static int lastOffsetInNode(Node* anchorNode);
    static PositionTemplate<Strategy> firstPositionInNode(Node* anchorNode);
    static PositionTemplate<Strategy> lastPositionInNode(Node* anchorNode);
    static int minOffsetForNode(Node* anchorNode, int offset);
    static PositionTemplate<Strategy> firstPositionInOrBeforeNode(Node* anchorNode);
    static PositionTemplate<Strategy> lastPositionInOrAfterNode(Node* anchorNode);

    void debugPosition(const char* msg = "") const;

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showAnchorTypeAndOffset() const;
    void showTreeForThis() const;
    void showTreeForThisInFlatTree() const;
#endif

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_anchorNode);
    }

private:
    bool isAfterAnchorOrAfterChildren() const
    {
        return isAfterAnchor() || isAfterChildren();
    }

    Member<Node> m_anchorNode;
    // m_offset can be the offset inside m_anchorNode, or if editingIgnoresContent(m_anchorNode)
    // returns true, then other places in editing will treat m_offset == 0 as "before the anchor"
    // and m_offset > 0 as "after the anchor node".  See parentAnchoredEquivalent for more info.
    int m_offset;
    PositionAnchorType m_anchorType;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT PositionTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT PositionTemplate<EditingInFlatTreeStrategy>;

using Position = PositionTemplate<EditingStrategy>;
using PositionInFlatTree = PositionTemplate<EditingInFlatTreeStrategy>;

template <typename Strategy>
bool operator==(const PositionTemplate<Strategy>& a, const PositionTemplate<Strategy>& b)
{
    if (a.isNull())
        return b.isNull();

    if (a.anchorNode() != b.anchorNode() || a.anchorType() != b.anchorType())
        return false;

    if (!a.isOffsetInAnchor()) {
        // Note: |m_offset| only has meaning when |PositionAnchorType::OffsetInAnchor|.
        return true;
    }

    // FIXME: In <div><img></div> [div, 0] != [img, 0] even though most of the
    // editing code will treat them as identical.
    return a.offsetInContainerNode() == b.offsetInContainerNode();
}

template <typename Strategy>
bool operator!=(const PositionTemplate<Strategy>& a, const PositionTemplate<Strategy>& b)
{
    return !(a == b);
}

CORE_EXPORT PositionInFlatTree toPositionInFlatTree(const Position&);
CORE_EXPORT Position toPositionInDOMTree(const Position&);
CORE_EXPORT Position toPositionInDOMTree(const PositionInFlatTree&);

template <typename Strategy>
PositionTemplate<Strategy> fromPositionInDOMTree(const Position&);

template <>
inline Position fromPositionInDOMTree<EditingStrategy>(const Position& position)
{
    return position;
}

template <>
inline PositionInFlatTree fromPositionInDOMTree<EditingInFlatTreeStrategy>(const Position& position)
{
    return toPositionInFlatTree(position);
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, PositionAnchorType);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const Position&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const PositionInFlatTree&);

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::Position&);
void showTree(const blink::Position*);
#endif

#endif // Position_h
