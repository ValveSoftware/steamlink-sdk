// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutItem_h
#define LineLayoutItem_h

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutObjectInlines.h"

#include "platform/LayoutUnit.h"
#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"

namespace blink {

class ComputedStyle;
class Document;
class HitTestRequest;
class HitTestLocation;
class LayoutObject;
class LineLayoutBox;
class LineLayoutBoxModel;
class LineLayoutAPIShim;

enum HitTestFilter;

static LayoutObject* const kHashTableDeletedValue = reinterpret_cast<LayoutObject*>(-1);

class LineLayoutItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    explicit LineLayoutItem(LayoutObject* layoutObject)
        : m_layoutObject(layoutObject)
    {
    }

    explicit LineLayoutItem(WTF::HashTableDeletedValueType)
        : m_layoutObject(kHashTableDeletedValue)
    {
    }

    LineLayoutItem(std::nullptr_t)
        : m_layoutObject(0)
    {
    }

    LineLayoutItem() : m_layoutObject(0) { }

    typedef LayoutObject* LineLayoutItem::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_layoutObject ? &LineLayoutItem::m_layoutObject : nullptr; }

    bool isEqual(const LayoutObject* layoutObject) const
    {
        return m_layoutObject == layoutObject;
    }

    bool operator==(const LineLayoutItem& other) const
    {
        return m_layoutObject == other.m_layoutObject;
    }

    bool operator!=(const LineLayoutItem& other) const
    {
        return !(*this == other);
    }

    String debugName() const
    {
        return m_layoutObject->debugName();
    }

    bool needsLayout() const
    {
        return m_layoutObject->needsLayout();
    }

    Node* node() const
    {
        return m_layoutObject->node();
    }

    Node* nonPseudoNode() const
    {
        return m_layoutObject->nonPseudoNode();
    }

    LineLayoutItem parent() const
    {
        return LineLayoutItem(m_layoutObject->parent());
    }

    // Implemented in LineLayoutBox.h
    // Intentionally returns a LineLayoutBox to avoid exposing LayoutBlock
    // to the line layout code.
    LineLayoutBox containingBlock() const;

    // Implemented in LineLayoutBoxModel.h
    // Intentionally returns a LineLayoutBoxModel to avoid exposing LayoutBoxModelObject
    // to the line layout code.
    LineLayoutBoxModel enclosingBoxModelObject() const;

    LineLayoutItem container() const
    {
        return LineLayoutItem(m_layoutObject->container());
    }

    bool isDescendantOf(const LineLayoutItem item) const
    {
        return m_layoutObject->isDescendantOf(item.m_layoutObject);
    }

    void updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
    {
        return m_layoutObject->updateHitTestResult(result, point);
    }

    LineLayoutItem nextSibling() const
    {
        return LineLayoutItem(m_layoutObject->nextSibling());
    }

    LineLayoutItem previousSibling() const
    {
        return LineLayoutItem(m_layoutObject->previousSibling());
    }

    LineLayoutItem slowFirstChild() const
    {
        return LineLayoutItem(m_layoutObject->slowFirstChild());
    }

    LineLayoutItem slowLastChild() const
    {
        return LineLayoutItem(m_layoutObject->slowLastChild());
    }

    // TODO(dgrogan/eae): Collapse these 4 methods to 1. Settle on pointer or
    // ref. Give firstLine a default value.
    const ComputedStyle* style() const
    {
        return m_layoutObject->style();
    }

    const ComputedStyle& styleRef() const
    {
        return m_layoutObject->styleRef();
    }

    const ComputedStyle* style(bool firstLine) const
    {
        return m_layoutObject->style(firstLine);
    }

    const ComputedStyle& styleRef(bool firstLine) const
    {
        return m_layoutObject->styleRef(firstLine);
    }

    Document& document() const
    {
        return m_layoutObject->document();
    }

    // TODO(dgrogan): This is the only caller: move the logic from LayoutObject
    // to here.
    bool preservesNewline() const
    {
        return m_layoutObject->preservesNewline();
    }

    unsigned length() const
    {
        return m_layoutObject->length();
    }

    void dirtyLinesFromChangedChild(LineLayoutItem item, MarkingBehavior markingBehaviour = MarkContainerChain) const
    {
        m_layoutObject->dirtyLinesFromChangedChild(item.layoutObject(), markingBehaviour);
    }

    bool ancestorLineBoxDirty() const
    {
        return m_layoutObject->ancestorLineBoxDirty();
    }

    // TODO(dgrogan/eae): Remove this method and replace every call with an ||.
    bool isFloatingOrOutOfFlowPositioned() const
    {
        return m_layoutObject->isFloatingOrOutOfFlowPositioned();
    }

    bool isFloating() const
    {
        return m_layoutObject->isFloating();
    }

    bool isOutOfFlowPositioned() const
    {
        return m_layoutObject->isOutOfFlowPositioned();
    }

    bool isBox() const
    {
        return m_layoutObject->isBox();
    }

    bool isBoxModelObject() const
    {
        return m_layoutObject->isBoxModelObject();
    }

    bool isBR() const
    {
        return m_layoutObject->isBR();
    }

    bool isCombineText() const
    {
        return m_layoutObject->isCombineText();
    }

    bool isHorizontalWritingMode() const
    {
        return m_layoutObject->isHorizontalWritingMode();
    }

    bool isImage() const
    {
        return m_layoutObject->isImage();
    }

    bool isInline() const
    {
        return m_layoutObject->isInline();
    }

    bool isInlineBlockOrInlineTable() const
    {
        return m_layoutObject->isInlineBlockOrInlineTable();
    }

    bool isInlineElementContinuation() const
    {
        return m_layoutObject->isInlineElementContinuation();
    }

    // TODO(dgrogan/eae): Replace isType with an enum in the API? As it stands
    // we mix isProperty and isType, which is confusing.
    bool isLayoutBlock() const
    {
        return m_layoutObject->isLayoutBlock();
    }

    bool isLayoutBlockFlow() const
    {
        return m_layoutObject->isLayoutBlockFlow();
    }

    bool isLayoutInline() const
    {
        return m_layoutObject->isLayoutInline();
    }

    bool isListMarker() const
    {
        return m_layoutObject->isListMarker();
    }

    bool isAtomicInlineLevel() const
    {
        return m_layoutObject->isAtomicInlineLevel();
    }

    bool isRubyText() const
    {
        return m_layoutObject->isRubyText();
    }

    bool isRubyRun() const
    {
        return m_layoutObject->isRubyRun();
    }

    bool isRubyBase() const
    {
        return m_layoutObject->isRubyBase();
    }

    bool isSVGInline() const
    {
        return m_layoutObject->isSVGInline();
    }

    bool isSVGInlineText() const
    {
        return m_layoutObject->isSVGInlineText();
    }

    bool isSVGText() const
    {
        return m_layoutObject->isSVGText();
    }

    bool isSVGTextPath() const
    {
        return m_layoutObject->isSVGTextPath();
    }

    bool isTableCell() const
    {
        return m_layoutObject->isTableCell();
    }

    bool isText() const
    {
        return m_layoutObject->isText();
    }

    bool hasLayer() const
    {
        return m_layoutObject->hasLayer();
    }

    bool selfNeedsLayout() const
    {
        return m_layoutObject->selfNeedsLayout();
    }

    // TODO(dgrogan/eae): Why does layoutObject need to know if its ancestor
    // line box is dirty at all?
    void setAncestorLineBoxDirty() const
    {
        m_layoutObject->setAncestorLineBoxDirty();
    }

    int caretMinOffset() const
    {
        return m_layoutObject->caretMinOffset();
    }

    int caretMaxOffset() const
    {
        return m_layoutObject->caretMaxOffset();
    }

    bool hasFlippedBlocksWritingMode() const
    {
        return m_layoutObject->hasFlippedBlocksWritingMode();
    }

    bool visibleToHitTestRequest(const HitTestRequest& request) const
    {
        return m_layoutObject->visibleToHitTestRequest(request);
    }

    bool hitTest(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter filter = HitTestAll)
    {
        return m_layoutObject->hitTest(result, locationInContainer, accumulatedOffset, filter);
    }

    SelectionState getSelectionState() const
    {
        return m_layoutObject->getSelectionState();
    }

    // TODO(dgrogan/eae): Can we move this to style?
    Color selectionBackgroundColor() const
    {
        return m_layoutObject->selectionBackgroundColor();
    }

    // TODO(dgrogan/eae): Needed for Color::current. Can we move this somewhere?
    Color resolveColor(const ComputedStyle& styleToUse, int colorProperty)
    {
        return m_layoutObject->resolveColor(styleToUse, colorProperty);
    }

    bool isInFlowPositioned() const
    {
        return m_layoutObject->isInFlowPositioned();
    }

    // TODO(dgrogan/eae): Can we change this to GlobalToLocal and vice versa
    // instead of having 4 methods? See localToAbsoluteQuad below.
    PositionWithAffinity positionForPoint(const LayoutPoint& point)
    {
        return m_layoutObject->positionForPoint(point);
    }

    PositionWithAffinity createPositionWithAffinity(int offset, TextAffinity affinity)
    {
        return m_layoutObject->createPositionWithAffinity(offset, affinity);
    }

    LineLayoutItem previousInPreOrder(const LayoutObject* stayWithin) const
    {
        return LineLayoutItem(m_layoutObject->previousInPreOrder(stayWithin));
    }

    FloatQuad localToAbsoluteQuad(const FloatQuad& quad, MapCoordinatesFlags mode = 0) const
    {
        return m_layoutObject->localToAbsoluteQuad(quad, mode);
    }

    FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(), MapCoordinatesFlags flags = 0) const
    {
        return m_layoutObject->localToAbsolute(localPoint, flags);
    }

    bool hasOverflowClip() const
    {
        return m_layoutObject->hasOverflowClip();
    }

    // TODO(dgrogan/eae): Can we instead add a TearDown method to the API
    // instead of exposing this and other shutdown code to line layout?
    bool documentBeingDestroyed() const
    {
        return m_layoutObject->documentBeingDestroyed();
    }

    LayoutRect visualRect() const
    {
        return m_layoutObject->visualRect();
    }

    bool isHashTableDeletedValue() const
    {
        return m_layoutObject == kHashTableDeletedValue;
    }

    void setShouldDoFullPaintInvalidation()
    {
        m_layoutObject->setShouldDoFullPaintInvalidation();
    }

    void slowSetPaintingLayerNeedsRepaint()
    {
        m_layoutObject->slowSetPaintingLayerNeedsRepaint();
    }

    struct LineLayoutItemHash {
        STATIC_ONLY(LineLayoutItemHash);
        static unsigned hash(const LineLayoutItem& key) { return WTF::PtrHash<LayoutObject>::hash(key.m_layoutObject); }
        static bool equal(const LineLayoutItem& a, const LineLayoutItem& b)
        {
            return WTF::PtrHash<LayoutObject>::equal(a.m_layoutObject, b.m_layoutObject);
        }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };

#ifndef NDEBUG

    const char* name() const
    {
        return m_layoutObject->name();
    }

    // Intentionally returns a void* to avoid exposing LayoutObject* to the line
    // layout code.
    void* debugPointer() const
    {
        return m_layoutObject;
    }

    void showTreeForThis() const
    {
        m_layoutObject->showTreeForThis();
    }

    String decoratedName() const
    {
        return m_layoutObject->decoratedName();
    }

#endif

protected:
    LayoutObject* layoutObject() { return m_layoutObject; }
    const LayoutObject* layoutObject() const { return m_layoutObject; }

private:
    LayoutObject* m_layoutObject;

    friend class LayoutBlockFlow;
    friend class LineLayoutAPIShim;
    friend class LineLayoutBlockFlow;
    friend class LineLayoutBox;
    friend class LineLayoutRubyRun;
};

} // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::LineLayoutItem> {
    using Hash = blink::LineLayoutItem::LineLayoutItemHash;
};

template <>
struct HashTraits<blink::LineLayoutItem> : SimpleClassHashTraits<blink::LineLayoutItem> {
    STATIC_ONLY(HashTraits);
};

} // namespace WTF


#endif // LineLayoutItem_h
