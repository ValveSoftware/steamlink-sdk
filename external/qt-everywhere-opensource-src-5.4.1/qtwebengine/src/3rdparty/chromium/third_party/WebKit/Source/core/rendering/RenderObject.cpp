/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "config.h"
#include "core/rendering/RenderObject.h"

#include "core/HTMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/animation/ActiveAnimations.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingBoundary.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/htmlediting.h"
#include "core/fetch/ResourceLoadPriorityOptimizer.h"
#include "core/fetch/ResourceLoader.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/page/AutoscrollController.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderCounter.h"
#include "core/rendering/RenderDeprecatedFlexibleBox.h"
#include "core/rendering/RenderFlexibleBox.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderGrid.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderImageResourceStyleImage.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderMarquee.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderTableCaption.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTableCol.h"
#include "core/rendering/RenderTableRow.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/ShadowList.h"
#include "platform/JSONValues.h"
#include "platform/Partitions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsContext.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include <algorithm>
#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace WebCore {

namespace {

static bool gModifyRenderTreeStructureAnyState = false;

} // namespace

using namespace HTMLNames;

#ifndef NDEBUG

RenderObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(RenderObject& renderObject)
    : m_renderObject(renderObject)
    , m_preexistingForbidden(m_renderObject.isSetNeedsLayoutForbidden())
{
    m_renderObject.setNeedsLayoutIsForbidden(true);
}

RenderObject::SetLayoutNeededForbiddenScope::~SetLayoutNeededForbiddenScope()
{
    m_renderObject.setNeedsLayoutIsForbidden(m_preexistingForbidden);
}
#endif

struct SameSizeAsRenderObject {
    virtual ~SameSizeAsRenderObject() { } // Allocate vtable pointer.
    void* pointers[5];
#ifndef NDEBUG
    unsigned m_debugBitfields : 2;
#endif
    unsigned m_bitfields;
    unsigned m_bitfields2;
    LayoutRect rect; // Stores the previous paint invalidation rect.
    LayoutPoint position; // Stores the previous position from the paint invalidation container.
};

COMPILE_ASSERT(sizeof(RenderObject) == sizeof(SameSizeAsRenderObject), RenderObject_should_stay_small);

bool RenderObject::s_affectsParentBlock = false;

void* RenderObject::operator new(size_t sz)
{
    ASSERT(isMainThread());
    return partitionAlloc(Partitions::getRenderingPartition(), sz);
}

void RenderObject::operator delete(void* ptr)
{
    ASSERT(isMainThread());
    partitionFree(ptr);
}

RenderObject* RenderObject::createObject(Element* element, RenderStyle* style)
{
    ASSERT(isAllowedToModifyRenderTreeStructure(element->document()));

    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    const ContentData* contentData = style->contentData();
    if (contentData && !contentData->next() && contentData->isImage() && !element->isPseudoElement()) {
        RenderImage* image = new RenderImage(element);
        // RenderImageResourceStyleImage requires a style being present on the image but we don't want to
        // trigger a style change now as the node is not fully attached. Moving this code to style change
        // doesn't make sense as it should be run once at renderer creation.
        image->setStyleInternal(style);
        if (const StyleImage* styleImage = static_cast<const ImageContentData*>(contentData)->image()) {
            image->setImageResource(RenderImageResourceStyleImage::create(const_cast<StyleImage*>(styleImage)));
            image->setIsGeneratedContent();
        } else
            image->setImageResource(RenderImageResource::create());
        image->setStyleInternal(nullptr);
        return image;
    }

    switch (style->display()) {
    case NONE:
        return 0;
    case INLINE:
        return new RenderInline(element);
    case BLOCK:
    case INLINE_BLOCK:
        return new RenderBlockFlow(element);
    case LIST_ITEM:
        return new RenderListItem(element);
    case TABLE:
    case INLINE_TABLE:
        return new RenderTable(element);
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        return new RenderTableSection(element);
    case TABLE_ROW:
        return new RenderTableRow(element);
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        return new RenderTableCol(element);
    case TABLE_CELL:
        return new RenderTableCell(element);
    case TABLE_CAPTION:
        return new RenderTableCaption(element);
    case BOX:
    case INLINE_BOX:
        return new RenderDeprecatedFlexibleBox(element);
    case FLEX:
    case INLINE_FLEX:
        return new RenderFlexibleBox(element);
    case GRID:
    case INLINE_GRID:
        return new RenderGrid(element);
    }

    return 0;
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, renderObjectCounter, ("RenderObject"));

RenderObject::RenderObject(Node* node)
    : ImageResourceClient()
    , m_style(nullptr)
    , m_node(node)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
#ifndef NDEBUG
    , m_hasAXObject(false)
    , m_setNeedsLayoutForbidden(false)
#endif
    , m_bitfields(node)
{
#ifndef NDEBUG
    renderObjectCounter.increment();
#endif
}

RenderObject::~RenderObject()
{
    ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->removeRenderObject(this);
#ifndef NDEBUG
    ASSERT(!m_hasAXObject);
    renderObjectCounter.decrement();
#endif
}

String RenderObject::debugName() const
{
    StringBuilder name;
    name.append(renderName());

    if (Node* node = this->node()) {
        name.append(' ');
        name.append(node->debugName());
    }

    return name.toString();
}

bool RenderObject::isDescendantOf(const RenderObject* obj) const
{
    for (const RenderObject* r = this; r; r = r->m_parent) {
        if (r == obj)
            return true;
    }
    return false;
}

bool RenderObject::isHR() const
{
    return isHTMLHRElement(node());
}

bool RenderObject::isLegend() const
{
    return isHTMLLegendElement(node());
}

void RenderObject::setFlowThreadStateIncludingDescendants(FlowThreadState state)
{
    for (RenderObject *object = this; object; object = object->nextInPreOrder(this)) {
        // If object is a fragmentation context it already updated the descendants flag accordingly.
        if (object->isRenderFlowThread())
            continue;
        ASSERT(state != object->flowThreadState());
        object->setFlowThreadState(state);
    }
}

bool RenderObject::requiresAnonymousTableWrappers(const RenderObject* newChild) const
{
    // Check should agree with:
    // CSS 2.1 Tables: 17.2.1 Anonymous table objects
    // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
    if (newChild->isRenderTableCol()) {
        const RenderTableCol* newTableColumn = toRenderTableCol(newChild);
        bool isColumnInColumnGroup = newTableColumn->isTableColumn() && isRenderTableCol();
        return !isTable() && !isColumnInColumnGroup;
    } else if (newChild->isTableCaption())
        return !isTable();
    else if (newChild->isTableSection())
        return !isTable();
    else if (newChild->isTableRow())
        return !isTableSection();
    else if (newChild->isTableCell())
        return !isTableRow();
    return false;
}

void RenderObject::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    ASSERT(isAllowedToModifyRenderTreeStructure(document()));

    RenderObjectChildList* children = virtualChildren();
    ASSERT(children);
    if (!children)
        return;

    if (requiresAnonymousTableWrappers(newChild)) {
        // Generate an anonymous table or reuse existing one from previous child
        // Per: 17.2.1 Anonymous table objects 3. Generate missing parents
        // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : children->lastChild();
        if (afterChild && afterChild->isAnonymous() && afterChild->isTable() && !afterChild->isBeforeContent())
            table = toRenderTable(afterChild);
        else {
            table = RenderTable::createAnonymousWithParentRenderer(this);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else
        children->insertChildNode(this, newChild, beforeChild);

    if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE)
        toRenderText(newChild)->transformText();

    // SVG creates renderers for <g display="none">, as SVG requires children of hidden
    // <g>s to have renderers - at least that's how our implementation works. Consider:
    // <g display="none"><foreignObject><body style="position: relative">FOO...
    // - layerTypeRequired() would return true for the <body>, creating a new RenderLayer
    // - when the document is painted, both layers are painted. The <body> layer doesn't
    //   know that it's inside a "hidden SVG subtree", and thus paints, even if it shouldn't.
    // To avoid the problem alltogether, detect early if we're inside a hidden SVG subtree
    // and stop creating layers at all for these cases - they're not used anyways.
    if (newChild->hasLayer() && !layerCreationAllowedForSubtree())
        toRenderLayerModelObject(newChild)->layer()->removeOnlyThisLayer();
}

void RenderObject::removeChild(RenderObject* oldChild)
{
    ASSERT(isAllowedToModifyRenderTreeStructure(document()));

    RenderObjectChildList* children = virtualChildren();
    ASSERT(children);
    if (!children)
        return;

    children->removeChildNode(this, oldChild);
}

RenderObject* RenderObject::nextInPreOrder() const
{
    if (RenderObject* o = slowFirstChild())
        return o;

    return nextInPreOrderAfterChildren();
}

RenderObject* RenderObject::nextInPreOrderAfterChildren() const
{
    RenderObject* o;
    if (!(o = nextSibling())) {
        o = parent();
        while (o && !o->nextSibling())
            o = o->parent();
        if (o)
            o = o->nextSibling();
    }

    return o;
}

RenderObject* RenderObject::nextInPreOrder(const RenderObject* stayWithin) const
{
    if (RenderObject* o = slowFirstChild())
        return o;

    return nextInPreOrderAfterChildren(stayWithin);
}

RenderObject* RenderObject::nextInPreOrderAfterChildren(const RenderObject* stayWithin) const
{
    if (this == stayWithin)
        return 0;

    const RenderObject* current = this;
    RenderObject* next;
    while (!(next = current->nextSibling())) {
        current = current->parent();
        if (!current || current == stayWithin)
            return 0;
    }
    return next;
}

RenderObject* RenderObject::previousInPreOrder() const
{
    if (RenderObject* o = previousSibling()) {
        while (RenderObject* lastChild = o->slowLastChild())
            o = lastChild;
        return o;
    }

    return parent();
}

RenderObject* RenderObject::previousInPreOrder(const RenderObject* stayWithin) const
{
    if (this == stayWithin)
        return 0;

    return previousInPreOrder();
}

RenderObject* RenderObject::childAt(unsigned index) const
{
    RenderObject* child = slowFirstChild();
    for (unsigned i = 0; child && i < index; i++)
        child = child->nextSibling();
    return child;
}

RenderObject* RenderObject::lastLeafChild() const
{
    RenderObject* r = slowLastChild();
    while (r) {
        RenderObject* n = 0;
        n = r->slowLastChild();
        if (!n)
            break;
        r = n;
    }
    return r;
}

static void addLayers(RenderObject* obj, RenderLayer* parentLayer, RenderObject*& newObject,
                      RenderLayer*& beforeChild)
{
    if (obj->hasLayer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject. We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = 0;
        }
        parentLayer->addChild(toRenderLayerModelObject(obj)->layer(), beforeChild);
        return;
    }

    for (RenderObject* curr = obj->slowFirstChild(); curr; curr = curr->nextSibling())
        addLayers(curr, parentLayer, newObject, beforeChild);
}

void RenderObject::addLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    RenderObject* object = this;
    RenderLayer* beforeChild = 0;
    WebCore::addLayers(this, parentLayer, object, beforeChild);
}

void RenderObject::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(toRenderLayerModelObject(this)->layer());
        return;
    }

    for (RenderObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
        curr->removeLayers(parentLayer);
}

void RenderObject::moveLayers(RenderLayer* oldParent, RenderLayer* newParent)
{
    if (!newParent)
        return;

    if (hasLayer()) {
        RenderLayer* layer = toRenderLayerModelObject(this)->layer();
        ASSERT(oldParent == layer->parent());
        if (oldParent)
            oldParent->removeChild(layer);
        newParent->addChild(layer);
        return;
    }

    for (RenderObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(oldParent, newParent);
}

RenderLayer* RenderObject::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint,
                                         bool checkParent)
{
    // Error check the parent layer passed in. If it's null, we can't find anything.
    if (!parentLayer)
        return 0;

    // Step 1: If our layer is a child of the desired parent, then return our layer.
    RenderLayer* ourLayer = hasLayer() ? toRenderLayerModelObject(this)->layer() : 0;
    if (ourLayer && ourLayer->parent() == parentLayer)
        return ourLayer;

    // Step 2: If we don't have a layer, or our layer is the desired parent, then descend
    // into our siblings trying to find the next layer whose parent is the desired parent.
    if (!ourLayer || ourLayer == parentLayer) {
        for (RenderObject* curr = startPoint ? startPoint->nextSibling() : slowFirstChild();
             curr; curr = curr->nextSibling()) {
            RenderLayer* nextLayer = curr->findNextLayer(parentLayer, 0, false);
            if (nextLayer)
                return nextLayer;
        }
    }

    // Step 3: If our layer is the desired parent layer, then we're finished. We didn't
    // find anything.
    if (parentLayer == ourLayer)
        return 0;

    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && parent())
        return parent()->findNextLayer(parentLayer, this, true);

    return 0;
}

RenderLayer* RenderObject::enclosingLayer() const
{
    for (const RenderObject* current = this; current; current = current->parent()) {
        if (current->hasLayer())
            return toRenderLayerModelObject(current)->layer();
    }
    // FIXME: We should remove the one caller that triggers this case and make
    // this function return a reference.
    ASSERT(!m_parent && !isRenderView());
    return 0;
}

bool RenderObject::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderBox* enclosingBox = this->enclosingBox();
    if (!enclosingBox)
        return false;

    enclosingBox->scrollRectToVisible(rect, alignX, alignY);
    return true;
}

RenderBox* RenderObject::enclosingBox() const
{
    RenderObject* curr = const_cast<RenderObject*>(this);
    while (curr) {
        if (curr->isBox())
            return toRenderBox(curr);
        curr = curr->parent();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

RenderBoxModelObject* RenderObject::enclosingBoxModelObject() const
{
    RenderObject* curr = const_cast<RenderObject*>(this);
    while (curr) {
        if (curr->isBoxModelObject())
            return toRenderBoxModelObject(curr);
        curr = curr->parent();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

RenderBox* RenderObject::enclosingScrollableBox() const
{
    for (RenderObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (!ancestor->isBox())
            continue;

        RenderBox* ancestorBox = toRenderBox(ancestor);
        if (ancestorBox->canBeScrolledAndHasScrollableArea())
            return ancestorBox;
    }

    return 0;
}

RenderFlowThread* RenderObject::locateFlowThreadContainingBlock() const
{
    ASSERT(flowThreadState() != NotInsideFlowThread);

    // See if we have the thread cached because we're in the middle of layout.
    RenderFlowThread* flowThread = view()->flowThreadController()->currentRenderFlowThread();
    if (flowThread)
        return flowThread;

    // Not in the middle of layout so have to find the thread the slow way.
    RenderObject* curr = const_cast<RenderObject*>(this);
    while (curr) {
        if (curr->isRenderFlowThread())
            return toRenderFlowThread(curr);
        curr = curr->containingBlock();
    }
    return 0;
}

RenderBlock* RenderObject::firstLineBlock() const
{
    return 0;
}

static inline bool objectIsRelayoutBoundary(const RenderObject* object)
{
    // FIXME: In future it may be possible to broaden these conditions in order to improve performance.
    if (object->isTextControl())
        return true;

    if (object->isSVGRoot())
        return true;

    if (!object->hasOverflowClip())
        return false;

    if (object->style()->width().isIntrinsicOrAuto() || object->style()->height().isIntrinsicOrAuto() || object->style()->height().isPercent())
        return false;

    // Table parts can't be relayout roots since the table is responsible for layouting all the parts.
    if (object->isTablePart())
        return false;

    return true;
}

void RenderObject::markContainingBlocksForLayout(bool scheduleRelayout, RenderObject* newRoot, SubtreeLayoutScope* layouter)
{
    ASSERT(!scheduleRelayout || !newRoot);
    ASSERT(!isSetNeedsLayoutForbidden());
    ASSERT(!layouter || this != layouter->root());

    RenderObject* object = container();
    RenderObject* last = this;

    bool simplifiedNormalFlowLayout = needsSimplifiedNormalFlowLayout() && !selfNeedsLayout() && !normalChildNeedsLayout();

    while (object) {
        if (object->selfNeedsLayout())
            return;

        // Don't mark the outermost object of an unrooted subtree. That object will be
        // marked when the subtree is added to the document.
        RenderObject* container = object->container();
        if (!container && !object->isRenderView())
            return;
        if (!last->isText() && last->style()->hasOutOfFlowPosition()) {
            bool willSkipRelativelyPositionedInlines = !object->isRenderBlock() || object->isAnonymousBlock();
            // Skip relatively positioned inlines and anonymous blocks to get to the enclosing RenderBlock.
            while (object && (!object->isRenderBlock() || object->isAnonymousBlock()))
                object = object->container();
            if (!object || object->posChildNeedsLayout())
                return;
            if (willSkipRelativelyPositionedInlines)
                container = object->container();
            object->setPosChildNeedsLayout(true);
            simplifiedNormalFlowLayout = true;
            ASSERT(!object->isSetNeedsLayoutForbidden());
        } else if (simplifiedNormalFlowLayout) {
            if (object->needsSimplifiedNormalFlowLayout())
                return;
            object->setNeedsSimplifiedNormalFlowLayout(true);
            ASSERT(!object->isSetNeedsLayoutForbidden());
        } else {
            if (object->normalChildNeedsLayout())
                return;
            object->setNormalChildNeedsLayout(true);
            ASSERT(!object->isSetNeedsLayoutForbidden());
        }

        if (layouter) {
            layouter->addRendererToLayout(object);
            if (object == layouter->root())
                return;
        }

        if (object == newRoot)
            return;

        last = object;
        if (scheduleRelayout && objectIsRelayoutBoundary(last))
            break;
        object = container;
    }

    if (scheduleRelayout)
        last->scheduleRelayout();
}

#ifndef NDEBUG
void RenderObject::checkBlockPositionedObjectsNeedLayout()
{
    ASSERT(!needsLayout());

    if (isRenderBlock())
        toRenderBlock(this)->checkPositionedObjectsNeedLayout();
}
#endif

void RenderObject::setPreferredLogicalWidthsDirty(MarkingBehavior markParents)
{
    m_bitfields.setPreferredLogicalWidthsDirty(true);
    if (markParents == MarkContainingBlockChain && (isText() || !style()->hasOutOfFlowPosition()))
        invalidateContainerPreferredLogicalWidths();
}

void RenderObject::clearPreferredLogicalWidthsDirty()
{
    m_bitfields.setPreferredLogicalWidthsDirty(false);
}

void RenderObject::invalidateContainerPreferredLogicalWidths()
{
    // In order to avoid pathological behavior when inlines are deeply nested, we do include them
    // in the chain that we mark dirty (even though they're kind of irrelevant).
    RenderObject* o = isTableCell() ? containingBlock() : container();
    while (o && !o->preferredLogicalWidthsDirty()) {
        // Don't invalidate the outermost object of an unrooted subtree. That object will be
        // invalidated when the subtree is added to the document.
        RenderObject* container = o->isTableCell() ? o->containingBlock() : o->container();
        if (!container && !o->isRenderView())
            break;

        o->m_bitfields.setPreferredLogicalWidthsDirty(true);
        if (o->style()->hasOutOfFlowPosition())
            // A positioned object has no effect on the min/max width of its containing block ever.
            // We can optimize this case and not go up any further.
            break;
        o = container;
    }
}

void RenderObject::setLayerNeedsFullPaintInvalidationForPositionedMovementLayout()
{
    ASSERT(hasLayer());
    toRenderLayerModelObject(this)->layer()->repainter().setRepaintStatus(NeedsFullRepaintForPositionedMovementLayout);
}

RenderBlock* RenderObject::containerForFixedPosition(const RenderLayerModelObject* paintInvalidationContainer, bool* paintInvalidationContainerSkipped) const
{
    ASSERT(!paintInvalidationContainerSkipped || !*paintInvalidationContainerSkipped);
    ASSERT(!isText());
    ASSERT(style()->position() == FixedPosition);

    RenderObject* ancestor = parent();
    for (; ancestor && !ancestor->canContainFixedPositionObjects(); ancestor = ancestor->parent()) {
        if (paintInvalidationContainerSkipped && ancestor == paintInvalidationContainer)
            *paintInvalidationContainerSkipped = true;
    }

    ASSERT(!ancestor || !ancestor->isAnonymousBlock());
    return toRenderBlock(ancestor);
}

RenderBlock* RenderObject::containingBlock() const
{
    RenderObject* o = parent();
    if (!o && isRenderScrollbarPart())
        o = toRenderScrollbarPart(this)->rendererOwningScrollbar();
    if (!isText() && m_style->position() == FixedPosition) {
        return containerForFixedPosition();
    } else if (!isText() && m_style->position() == AbsolutePosition) {
        while (o) {
            // For relpositioned inlines, we return the nearest non-anonymous enclosing block. We don't try
            // to return the inline itself.  This allows us to avoid having a positioned objects
            // list in all RenderInlines and lets us return a strongly-typed RenderBlock* result
            // from this method.  The container() method can actually be used to obtain the
            // inline directly.
            if (o->style()->position() != StaticPosition && (!o->isInline() || o->isReplaced()))
                break;

            if (o->canContainFixedPositionObjects())
                break;

            if (o->style()->hasInFlowPosition() && o->isInline() && !o->isReplaced()) {
                o = o->containingBlock();
                break;
            }

            o = o->parent();
        }

        if (o && !o->isRenderBlock())
            o = o->containingBlock();

        while (o && o->isAnonymousBlock())
            o = o->containingBlock();
    } else {
        while (o && ((o->isInline() && !o->isReplaced()) || !o->isRenderBlock()))
            o = o->parent();
    }

    if (!o || !o->isRenderBlock())
        return 0; // This can still happen in case of an orphaned tree

    return toRenderBlock(o);
}

RenderObject* RenderObject::clippingContainer() const
{
    RenderObject* container = const_cast<RenderObject*>(this);
    while (container) {
        if (container->style()->position() == FixedPosition) {
            for (container = container->parent(); container && !container->canContainFixedPositionObjects(); container = container->parent()) {
                // CSS clip applies to fixed position elements even for ancestors that are not what the
                // fixed element is positioned with respect to.
                if (container->hasClip())
                    return container;
            }
        } else {
            container = container->containingBlock();
        }

        if (!container)
            return 0;
        if (container->hasClipOrOverflowClip())
            return container;
    }
    return 0;
}

bool RenderObject::canRenderBorderImage() const
{
    ASSERT(style()->hasBorder());

    StyleImage* borderImage = style()->borderImage().image();
    return borderImage && borderImage->canRender(*this, style()->effectiveZoom()) && borderImage->isLoaded();
}

bool RenderObject::mustInvalidateFillLayersPaintOnWidthChange(const FillLayer& layer) const
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer.next())
        return true;

    // Make sure we have a valid image.
    StyleImage* img = layer.image();
    if (!img || !img->canRender(*this, style()->effectiveZoom()))
        return false;

    if (layer.repeatX() != RepeatFill && layer.repeatX() != NoRepeatFill)
        return true;

    if (layer.xPosition().isPercent() && !layer.xPosition().isZero())
        return true;

    if (layer.backgroundXOrigin() != LeftEdge)
        return true;

    EFillSizeType sizeType = layer.sizeType();

    if (sizeType == Contain || sizeType == Cover)
        return true;

    if (sizeType == SizeLength) {
        if (layer.sizeLength().width().isPercent() && !layer.sizeLength().width().isZero())
            return true;
        if (img->isGeneratedImage() && layer.sizeLength().width().isAuto())
            return true;
    } else if (img->usesImageContainerSize()) {
        return true;
    }

    return false;
}

bool RenderObject::mustInvalidateFillLayersPaintOnHeightChange(const FillLayer& layer) const
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer.next())
        return true;

    // Make sure we have a valid image.
    StyleImage* img = layer.image();
    if (!img || !img->canRender(*this, style()->effectiveZoom()))
        return false;

    if (layer.repeatY() != RepeatFill && layer.repeatY() != NoRepeatFill)
        return true;

    if (layer.yPosition().isPercent() && !layer.yPosition().isZero())
        return true;

    if (layer.backgroundYOrigin() != TopEdge)
        return true;

    EFillSizeType sizeType = layer.sizeType();

    if (sizeType == Contain || sizeType == Cover)
        return true;

    if (sizeType == SizeLength) {
        if (layer.sizeLength().height().isPercent() && !layer.sizeLength().height().isZero())
            return true;
        if (img->isGeneratedImage() && layer.sizeLength().height().isAuto())
            return true;
    } else if (img->usesImageContainerSize()) {
        return true;
    }

    return false;
}

bool RenderObject::mustInvalidateBackgroundOrBorderPaintOnWidthChange() const
{
    if (hasMask() && mustInvalidateFillLayersPaintOnWidthChange(*style()->maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!hasBoxDecorations())
        return false;

    if (mustInvalidateFillLayersPaintOnWidthChange(*style()->backgroundLayers()))
        return true;

    // Our fill layers are ok. Let's check border.
    if (style()->hasBorder() && canRenderBorderImage())
        return true;

    return false;
}

bool RenderObject::mustInvalidateBackgroundOrBorderPaintOnHeightChange() const
{
    if (hasMask() && mustInvalidateFillLayersPaintOnHeightChange(*style()->maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!hasBoxDecorations())
        return false;

    if (mustInvalidateFillLayersPaintOnHeightChange(*style()->backgroundLayers()))
        return true;

    // Our fill layers are ok.  Let's check border.
    if (style()->hasBorder() && canRenderBorderImage())
        return true;

    return false;
}

void RenderObject::drawLineForBoxSide(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
                                      BoxSide side, Color color, EBorderStyle style,
                                      int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    int thickness;
    int length;
    if (side == BSTop || side == BSBottom) {
        thickness = y2 - y1;
        length = x2 - x1;
    } else {
        thickness = x2 - x1;
        length = y2 - y1;
    }

    // FIXME: We really would like this check to be an ASSERT as we don't want to draw empty borders. However
    // nothing guarantees that the following recursive calls to drawLineForBoxSide will have non-null dimensions.
    if (!thickness || !length)
        return;

    if (style == DOUBLE && thickness < 3)
        style = SOLID;

    switch (style) {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
    case DASHED:
        drawDashedOrDottedBoxSide(graphicsContext, x1, y1, x2, y2, side,
            color, thickness, style, antialias);
        break;
    case DOUBLE:
        drawDoubleBoxSide(graphicsContext, x1, y1, x2, y2, length, side, color,
            thickness, adjacentWidth1, adjacentWidth2, antialias);
        break;
    case RIDGE:
    case GROOVE:
        drawRidgeOrGrooveBoxSide(graphicsContext, x1, y1, x2, y2, side, color,
            style, adjacentWidth1, adjacentWidth2, antialias);
        break;
    case INSET:
        // FIXME: Maybe we should lighten the colors on one side like Firefox.
        // https://bugs.webkit.org/show_bug.cgi?id=58608
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        // fall through
    case OUTSET:
        if (style == OUTSET && (side == BSBottom || side == BSRight))
            color = color.dark();
        // fall through
    case SOLID:
        drawSolidBoxSide(graphicsContext, x1, y1, x2, y2, side, color, adjacentWidth1, adjacentWidth2, antialias);
        break;
    }
}

void RenderObject::drawDashedOrDottedBoxSide(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, int thickness, EBorderStyle style, bool antialias)
{
    if (thickness <= 0)
        return;

    bool wasAntialiased = graphicsContext->shouldAntialias();
    StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
    graphicsContext->setShouldAntialias(antialias);
    graphicsContext->setStrokeColor(color);
    graphicsContext->setStrokeThickness(thickness);
    graphicsContext->setStrokeStyle(style == DASHED ? DashedStroke : DottedStroke);

    switch (side) {
    case BSBottom:
    case BSTop:
        graphicsContext->drawLine(IntPoint(x1, (y1 + y2) / 2), IntPoint(x2, (y1 + y2) / 2));
        break;
    case BSRight:
    case BSLeft:
        graphicsContext->drawLine(IntPoint((x1 + x2) / 2, y1), IntPoint((x1 + x2) / 2, y2));
        break;
    }
    graphicsContext->setShouldAntialias(wasAntialiased);
    graphicsContext->setStrokeStyle(oldStrokeStyle);
}

void RenderObject::drawDoubleBoxSide(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
    int length, BoxSide side, Color color, int thickness, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    int thirdOfThickness = (thickness + 1) / 3;
    ASSERT(thirdOfThickness);

    if (!adjacentWidth1 && !adjacentWidth2) {
        StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
        graphicsContext->setStrokeStyle(NoStroke);
        graphicsContext->setFillColor(color);

        bool wasAntialiased = graphicsContext->shouldAntialias();
        graphicsContext->setShouldAntialias(antialias);

        switch (side) {
        case BSTop:
        case BSBottom:
            graphicsContext->drawRect(IntRect(x1, y1, length, thirdOfThickness));
            graphicsContext->drawRect(IntRect(x1, y2 - thirdOfThickness, length, thirdOfThickness));
            break;
        case BSLeft:
        case BSRight:
            // FIXME: Why do we offset the border by 1 in this case but not the other one?
            if (length > 1) {
                graphicsContext->drawRect(IntRect(x1, y1 + 1, thirdOfThickness, length - 1));
                graphicsContext->drawRect(IntRect(x2 - thirdOfThickness, y1 + 1, thirdOfThickness, length - 1));
            }
            break;
        }

        graphicsContext->setShouldAntialias(wasAntialiased);
        graphicsContext->setStrokeStyle(oldStrokeStyle);
        return;
    }

    int adjacent1BigThird = ((adjacentWidth1 > 0) ? adjacentWidth1 + 1 : adjacentWidth1 - 1) / 3;
    int adjacent2BigThird = ((adjacentWidth2 > 0) ? adjacentWidth2 + 1 : adjacentWidth2 - 1) / 3;

    switch (side) {
    case BSTop:
        drawLineForBoxSide(graphicsContext, x1 + max((-adjacentWidth1 * 2 + 1) / 3, 0),
            y1, x2 - max((-adjacentWidth2 * 2 + 1) / 3, 0), y1 + thirdOfThickness,
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x1 + max((adjacentWidth1 * 2 + 1) / 3, 0),
            y2 - thirdOfThickness, x2 - max((adjacentWidth2 * 2 + 1) / 3, 0), y2,
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSLeft:
        drawLineForBoxSide(graphicsContext, x1, y1 + max((-adjacentWidth1 * 2 + 1) / 3, 0),
            x1 + thirdOfThickness, y2 - max((-adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x2 - thirdOfThickness, y1 + max((adjacentWidth1 * 2 + 1) / 3, 0),
            x2, y2 - max((adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSBottom:
        drawLineForBoxSide(graphicsContext, x1 + max((adjacentWidth1 * 2 + 1) / 3, 0),
            y1, x2 - max((adjacentWidth2 * 2 + 1) / 3, 0), y1 + thirdOfThickness,
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x1 + max((-adjacentWidth1 * 2 + 1) / 3, 0),
            y2 - thirdOfThickness, x2 - max((-adjacentWidth2 * 2 + 1) / 3, 0), y2,
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    case BSRight:
        drawLineForBoxSide(graphicsContext, x1, y1 + max((adjacentWidth1 * 2 + 1) / 3, 0),
            x1 + thirdOfThickness, y2 - max((adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        drawLineForBoxSide(graphicsContext, x2 - thirdOfThickness, y1 + max((-adjacentWidth1 * 2 + 1) / 3, 0),
            x2, y2 - max((-adjacentWidth2 * 2 + 1) / 3, 0),
            side, color, SOLID, adjacent1BigThird, adjacent2BigThird, antialias);
        break;
    default:
        break;
    }
}

void RenderObject::drawRidgeOrGrooveBoxSide(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, EBorderStyle style, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    EBorderStyle s1;
    EBorderStyle s2;
    if (style == GROOVE) {
        s1 = INSET;
        s2 = OUTSET;
    } else {
        s1 = OUTSET;
        s2 = INSET;
    }

    int adjacent1BigHalf = ((adjacentWidth1 > 0) ? adjacentWidth1 + 1 : adjacentWidth1 - 1) / 2;
    int adjacent2BigHalf = ((adjacentWidth2 > 0) ? adjacentWidth2 + 1 : adjacentWidth2 - 1) / 2;

    switch (side) {
    case BSTop:
        drawLineForBoxSide(graphicsContext, x1 + max(-adjacentWidth1, 0) / 2, y1, x2 - max(-adjacentWidth2, 0) / 2, (y1 + y2 + 1) / 2,
            side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, x1 + max(adjacentWidth1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - max(adjacentWidth2 + 1, 0) / 2, y2,
            side, color, s2, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSLeft:
        drawLineForBoxSide(graphicsContext, x1, y1 + max(-adjacentWidth1, 0) / 2, (x1 + x2 + 1) / 2, y2 - max(-adjacentWidth2, 0) / 2,
            side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, (x1 + x2 + 1) / 2, y1 + max(adjacentWidth1 + 1, 0) / 2, x2, y2 - max(adjacentWidth2 + 1, 0) / 2,
            side, color, s2, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSBottom:
        drawLineForBoxSide(graphicsContext, x1 + max(adjacentWidth1, 0) / 2, y1, x2 - max(adjacentWidth2, 0) / 2, (y1 + y2 + 1) / 2,
            side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, x1 + max(-adjacentWidth1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - max(-adjacentWidth2 + 1, 0) / 2, y2,
            side, color, s1, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    case BSRight:
        drawLineForBoxSide(graphicsContext, x1, y1 + max(adjacentWidth1, 0) / 2, (x1 + x2 + 1) / 2, y2 - max(adjacentWidth2, 0) / 2,
            side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
        drawLineForBoxSide(graphicsContext, (x1 + x2 + 1) / 2, y1 + max(-adjacentWidth1 + 1, 0) / 2, x2, y2 - max(-adjacentWidth2 + 1, 0) / 2,
            side, color, s1, adjacentWidth1 / 2, adjacentWidth2 / 2, antialias);
        break;
    }
}

void RenderObject::drawSolidBoxSide(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
    BoxSide side, Color color, int adjacentWidth1, int adjacentWidth2, bool antialias)
{
    StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
    graphicsContext->setStrokeStyle(NoStroke);
    graphicsContext->setFillColor(color);
    ASSERT(x2 >= x1);
    ASSERT(y2 >= y1);
    if (!adjacentWidth1 && !adjacentWidth2) {
        // Turn off antialiasing to match the behavior of drawConvexPolygon();
        // this matters for rects in transformed contexts.
        bool wasAntialiased = graphicsContext->shouldAntialias();
        graphicsContext->setShouldAntialias(antialias);
        graphicsContext->drawRect(IntRect(x1, y1, x2 - x1, y2 - y1));
        graphicsContext->setShouldAntialias(wasAntialiased);
        graphicsContext->setStrokeStyle(oldStrokeStyle);
        return;
    }
    FloatPoint quad[4];
    switch (side) {
    case BSTop:
        quad[0] = FloatPoint(x1 + max(-adjacentWidth1, 0), y1);
        quad[1] = FloatPoint(x1 + max(adjacentWidth1, 0), y2);
        quad[2] = FloatPoint(x2 - max(adjacentWidth2, 0), y2);
        quad[3] = FloatPoint(x2 - max(-adjacentWidth2, 0), y1);
        break;
    case BSBottom:
        quad[0] = FloatPoint(x1 + max(adjacentWidth1, 0), y1);
        quad[1] = FloatPoint(x1 + max(-adjacentWidth1, 0), y2);
        quad[2] = FloatPoint(x2 - max(-adjacentWidth2, 0), y2);
        quad[3] = FloatPoint(x2 - max(adjacentWidth2, 0), y1);
        break;
    case BSLeft:
        quad[0] = FloatPoint(x1, y1 + max(-adjacentWidth1, 0));
        quad[1] = FloatPoint(x1, y2 - max(-adjacentWidth2, 0));
        quad[2] = FloatPoint(x2, y2 - max(adjacentWidth2, 0));
        quad[3] = FloatPoint(x2, y1 + max(adjacentWidth1, 0));
        break;
    case BSRight:
        quad[0] = FloatPoint(x1, y1 + max(adjacentWidth1, 0));
        quad[1] = FloatPoint(x1, y2 - max(adjacentWidth2, 0));
        quad[2] = FloatPoint(x2, y2 - max(-adjacentWidth2, 0));
        quad[3] = FloatPoint(x2, y1 + max(-adjacentWidth1, 0));
        break;
    }

    graphicsContext->drawConvexPolygon(4, quad, antialias);
    graphicsContext->setStrokeStyle(oldStrokeStyle);
}

void RenderObject::paintFocusRing(PaintInfo& paintInfo, const LayoutPoint& paintOffset, RenderStyle* style)
{
    Vector<IntRect> focusRingRects;
    addFocusRingRects(focusRingRects, paintOffset, paintInfo.paintContainer());
    if (style->outlineStyleIsAuto())
        paintInfo.context->drawFocusRing(focusRingRects, style->outlineWidth(), style->outlineOffset(), resolveColor(style, CSSPropertyOutlineColor));
    else
        addPDFURLRect(paintInfo.context, unionRect(focusRingRects));
}

void RenderObject::addPDFURLRect(GraphicsContext* context, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;
    Node* n = node();
    if (!n || !n->isLink() || !n->isElementNode())
        return;
    const AtomicString& href = toElement(n)->getAttribute(hrefAttr);
    if (href.isNull())
        return;
    KURL url = n->document().completeURL(href);
    if (!url.isValid())
        return;
    if (context->supportsURLFragments() && url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, n->document().baseURL())) {
        String name = url.fragmentIdentifier();
        if (document().findAnchor(name))
            context->setURLFragmentForRect(name, pixelSnappedIntRect(rect));
        return;
    }
    context->setURLForRect(url, pixelSnappedIntRect(rect));
}

void RenderObject::paintOutline(PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    if (!hasOutline())
        return;

    RenderStyle* styleToUse = style();
    LayoutUnit outlineWidth = styleToUse->outlineWidth();

    int outlineOffset = styleToUse->outlineOffset();

    if (styleToUse->outlineStyleIsAuto() || hasOutlineAnnotation()) {
        if (RenderTheme::theme().shouldDrawDefaultFocusRing(this)) {
            // Only paint the focus ring by hand if the theme isn't able to draw the focus ring.
            paintFocusRing(paintInfo, paintRect.location(), styleToUse);
        }
    }

    if (styleToUse->outlineStyleIsAuto() || styleToUse->outlineStyle() == BNONE)
        return;

    IntRect inner = pixelSnappedIntRect(paintRect);
    inner.inflate(outlineOffset);

    IntRect outer = pixelSnappedIntRect(inner);
    outer.inflate(outlineWidth);

    // FIXME: This prevents outlines from painting inside the object. See bug 12042
    if (outer.isEmpty())
        return;

    EBorderStyle outlineStyle = styleToUse->outlineStyle();
    Color outlineColor = resolveColor(styleToUse, CSSPropertyOutlineColor);

    GraphicsContext* graphicsContext = paintInfo.context;
    bool useTransparencyLayer = outlineColor.hasAlpha();
    if (useTransparencyLayer) {
        if (outlineStyle == SOLID) {
            Path path;
            path.addRect(outer);
            path.addRect(inner);
            graphicsContext->setFillRule(RULE_EVENODD);
            graphicsContext->setFillColor(outlineColor);
            graphicsContext->fillPath(path);
            return;
        }
        graphicsContext->beginTransparencyLayer(static_cast<float>(outlineColor.alpha()) / 255);
        outlineColor = Color(outlineColor.red(), outlineColor.green(), outlineColor.blue());
    }

    int leftOuter = outer.x();
    int leftInner = inner.x();
    int rightOuter = outer.maxX();
    int rightInner = inner.maxX();
    int topOuter = outer.y();
    int topInner = inner.y();
    int bottomOuter = outer.maxY();
    int bottomInner = inner.maxY();

    drawLineForBoxSide(graphicsContext, leftOuter, topOuter, leftInner, bottomOuter, BSLeft, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, leftOuter, topOuter, rightOuter, topInner, BSTop, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, rightInner, topOuter, rightOuter, bottomOuter, BSRight, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, leftOuter, bottomInner, rightOuter, bottomOuter, BSBottom, outlineColor, outlineStyle, outlineWidth, outlineWidth);

    if (useTransparencyLayer)
        graphicsContext->endLayer();
}

void RenderObject::addChildFocusRingRects(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    for (RenderObject* current = slowFirstChild(); current; current = current->nextSibling()) {
        if (current->isText() || current->isListMarker())
            continue;

        if (current->isBox()) {
            RenderBox* box = toRenderBox(current);
            if (box->hasLayer()) {
                Vector<IntRect> layerFocusRingRects;
                box->addFocusRingRects(layerFocusRingRects, LayoutPoint(), box);
                for (size_t i = 0; i < layerFocusRingRects.size(); ++i) {
                    FloatQuad quadInBox = box->localToContainerQuad(FloatRect(layerFocusRingRects[i]), paintContainer);
                    rects.append(pixelSnappedIntRect(LayoutRect(quadInBox.boundingBox())));
                }
            } else {
                FloatPoint pos(additionalOffset);
                pos.move(box->locationOffset()); // FIXME: Snap offsets? crbug.com/350474
                box->addFocusRingRects(rects, flooredIntPoint(pos), paintContainer);
            }
        } else {
            current->addFocusRingRects(rects, additionalOffset, paintContainer);
        }
    }
}

// FIXME: In repaint-after-layout, we should be able to change the logic to remove the need for this function. See crbug.com/368416.
LayoutPoint RenderObject::positionFromPaintInvalidationContainer(const RenderLayerModelObject* paintInvalidationContainer) const
{
    // FIXME: This assert should be re-enabled when we move paint invalidation to after compositing update. crbug.com/360286
    // ASSERT(containerForPaintInvalidation() == paintInvalidationContainer);

    LayoutPoint offset = isBox() ? toRenderBox(this)->location() : LayoutPoint();
    if (paintInvalidationContainer == this)
        return offset;

    return roundedIntPoint(localToContainerPoint(offset, paintInvalidationContainer));
}

IntRect RenderObject::absoluteBoundingBoxRect() const
{
    Vector<FloatQuad> quads;
    absoluteQuads(quads);

    size_t n = quads.size();
    if (!n)
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < n; ++i)
        result.unite(quads[i].enclosingBoundingBox());
    return result;
}

IntRect RenderObject::absoluteBoundingBoxRectIgnoringTransforms() const
{
    FloatPoint absPos = localToAbsolute();
    Vector<IntRect> rects;
    absoluteRects(rects, flooredLayoutPoint(absPos));

    size_t n = rects.size();
    if (!n)
        return IntRect();

    LayoutRect result = rects[0];
    for (size_t i = 1; i < n; ++i)
        result.unite(rects[i]);
    return pixelSnappedIntRect(result);
}

void RenderObject::absoluteFocusRingQuads(Vector<FloatQuad>& quads)
{
    Vector<IntRect> rects;
    const RenderLayerModelObject* container = containerForPaintInvalidation();
    addFocusRingRects(rects, LayoutPoint(localToContainerPoint(FloatPoint(), container)), container);
    size_t count = rects.size();
    for (size_t i = 0; i < count; ++i)
        quads.append(container->localToAbsoluteQuad(FloatQuad(rects[i])));
}

FloatRect RenderObject::absoluteBoundingBoxRectForRange(const Range* range)
{
    if (!range || !range->startContainer())
        return FloatRect();

    range->ownerDocument().updateLayout();

    Vector<FloatQuad> quads;
    range->textQuads(quads);

    FloatRect result;
    for (size_t i = 0; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    return result;
}

void RenderObject::addAbsoluteRectForLayer(LayoutRect& result)
{
    if (hasLayer())
        result.unite(absoluteBoundingBoxRect());
    for (RenderObject* current = slowFirstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
}

LayoutRect RenderObject::paintingRootRect(LayoutRect& topLevelRect)
{
    LayoutRect result = absoluteBoundingBoxRect();
    topLevelRect = result;
    for (RenderObject* current = slowFirstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
    return result;
}

void RenderObject::paint(PaintInfo&, const LayoutPoint&)
{
}

const RenderLayerModelObject* RenderObject::containerForPaintInvalidation() const
{
    if (!isRooted())
        return 0;

    return adjustCompositedContainerForSpecialAncestors(enclosingCompositedContainer());
}

const RenderLayerModelObject* RenderObject::enclosingCompositedContainer() const
{
    RenderLayerModelObject* container = 0;
    if (view()->usesCompositing()) {
        // FIXME: CompositingState is not necessarily up to date for many callers of this function.
        DisableCompositingQueryAsserts disabler;

        if (RenderLayer* compositingLayer = enclosingLayer()->enclosingCompositingLayerForRepaint())
            container = compositingLayer->renderer();
    }
    return container;
}

const RenderLayerModelObject* RenderObject::adjustCompositedContainerForSpecialAncestors(const RenderLayerModelObject* paintInvalidationContainer) const
{

    if (document().view()->hasSoftwareFilters()) {
        if (RenderLayer* enclosingFilterLayer = enclosingLayer()->enclosingFilterLayer())
            return enclosingFilterLayer->renderer();
    }

    // If we have a flow thread, then we need to do individual paint invalidations within the RenderRegions instead.
    // Return the flow thread as a paint invalidation container in order to create a chokepoint that allows us to change
    // paint invalidation to do individual region paint invalidations.
    if (RenderFlowThread* parentRenderFlowThread = flowThreadContainingBlock()) {
        // If we have already found a paint invalidation container then we will invalidate paints in that container only if it is part of the same
        // flow thread. Otherwise we will need to catch the paint invalidation call and send it to the flow thread.
        if (!paintInvalidationContainer || paintInvalidationContainer->flowThreadContainingBlock() != parentRenderFlowThread)
            paintInvalidationContainer = parentRenderFlowThread;
    }
    return paintInvalidationContainer ? paintInvalidationContainer : view();
}

bool RenderObject::isPaintInvalidationContainer() const
{
    return hasLayer() && toRenderLayerModelObject(this)->layer()->isRepaintContainer();
}

template<typename T> PassRefPtr<JSONValue> jsonObjectForRect(const T& rect)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", rect.x());
    object->setNumber("y", rect.y());
    object->setNumber("width", rect.width());
    object->setNumber("height", rect.height());
    return object.release();
}

static PassRefPtr<JSONValue> jsonObjectForPaintInvalidationInfo(const IntRect& rect, const String& invalidationReason)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setValue("rect", jsonObjectForRect(rect));
    object->setString("invalidation_reason", invalidationReason);
    return object.release();
}

LayoutRect RenderObject::computePaintInvalidationRect(const RenderLayerModelObject* paintInvalidationContainer) const
{
    return clippedOverflowRectForPaintInvalidation(paintInvalidationContainer);
}

void RenderObject::invalidatePaintUsingContainer(const RenderLayerModelObject* paintInvalidationContainer, const IntRect& r, InvalidationReason invalidationReason) const
{
    if (r.isEmpty())
        return;

    // FIXME: This should be an assert, but editing/selection can trigger this case to invalidate
    // the selection. crbug.com/368140.
    if (!isRooted())
        return;

    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), "RenderObject::invalidatePaintUsingContainer()",
        "object", this->debugName().ascii(),
        "info", TracedValue::fromJSONValue(jsonObjectForPaintInvalidationInfo(r, invalidationReasonToString(invalidationReason))));

    // For querying RenderLayer::compositingState()
    DisableCompositingQueryAsserts disabler;

    if (paintInvalidationContainer->isRenderFlowThread()) {
        toRenderFlowThread(paintInvalidationContainer)->repaintRectangleInRegions(r);
        return;
    }

    if (paintInvalidationContainer->hasFilter() && paintInvalidationContainer->layer()->requiresFullLayerImageForFilters()) {
        paintInvalidationContainer->layer()->repainter().setFilterBackendNeedsRepaintingInRect(r);
        return;
    }

    RenderView* v = view();
    if (paintInvalidationContainer->isRenderView()) {
        ASSERT(paintInvalidationContainer == v);
        v->repaintViewRectangle(r);
        return;
    }

    if (v->usesCompositing()) {
        ASSERT(paintInvalidationContainer->hasLayer() && (paintInvalidationContainer->layer()->compositingState() == PaintsIntoOwnBacking || paintInvalidationContainer->layer()->compositingState() == PaintsIntoGroupedBacking));
        paintInvalidationContainer->layer()->repainter().setBackingNeedsRepaintInRect(r);
    }
}

void RenderObject::paintInvalidationForWholeRenderer() const
{
    if (!isRooted())
        return;

    if (view()->document().printing())
        return; // Don't invalidate paints if we're printing.

    // FIXME: really, we're in the paint invalidation phase here, and the following queries are legal.
    // Until those states are fully fledged, I'll just disable the ASSERTS.
    DisableCompositingQueryAsserts disabler;
    const RenderLayerModelObject* paintInvalidationContainer = containerForPaintInvalidation();
    LayoutRect paintInvalidationRect = boundsRectForPaintInvalidation(paintInvalidationContainer);
    invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(paintInvalidationRect), InvalidationPaint);
}

LayoutRect RenderObject::boundsRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const
{
    if (!paintInvalidationContainer)
        return computePaintInvalidationRect(paintInvalidationContainer);
    return RenderLayer::computeRepaintRect(this, paintInvalidationContainer->layer());
}

void RenderObject::invalidatePaintRectangle(const LayoutRect& r) const
{
    if (!isRooted())
        return;

    if (view()->document().printing())
        return; // Don't invalidate paints if we're printing.

    LayoutRect dirtyRect(r);

    if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled()) {
        // FIXME: layoutDelta needs to be applied in parts before/after transforms and
        // paint invalidation containers. https://bugs.webkit.org/show_bug.cgi?id=23308
        dirtyRect.move(view()->layoutDelta());
    }

    const RenderLayerModelObject* paintInvalidationContainer = containerForPaintInvalidation();
    RenderLayer::mapRectToRepaintBacking(this, paintInvalidationContainer, dirtyRect);
    invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(dirtyRect), InvalidationPaintRectangle);
}

IntRect RenderObject::pixelSnappedAbsoluteClippedOverflowRect() const
{
    return pixelSnappedIntRect(absoluteClippedOverflowRect());
}

const char* RenderObject::invalidationReasonToString(InvalidationReason reason) const
{
    switch (reason) {
    case InvalidationIncremental:
        return "incremental";
    case InvalidationSelfLayout:
        return "self layout";
    case InvalidationBorderFitLines:
        return "border fit lines";
    case InvalidationBorderRadius:
        return "border radius";
    case InvalidationBoundsChangeWithBackground:
        return "bounds change with background";
    case InvalidationBoundsChange:
        return "bounds change";
    case InvalidationLocationChange:
        return "location change";
    case InvalidationScroll:
        return "scroll";
    case InvalidationSelection:
        return "selection";
    case InvalidationLayer:
        return "layer";
    case InvalidationPaint:
        return "invalidate paint";
    case InvalidationPaintRectangle:
        return "invalidate paint rectangle";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void RenderObject::invalidateTreeAfterLayout(const RenderLayerModelObject& paintInvalidationContainer)
{
    // If we didn't need paint invalidation then our children don't need as well.
    // Skip walking down the tree as everything should be fine below us.
    if (!shouldCheckForPaintInvalidationAfterLayout())
        return;

    clearPaintInvalidationState();

    for (RenderObject* child = slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->isOutOfFlowPositioned())
            child->invalidateTreeAfterLayout(paintInvalidationContainer);
    }
}

static PassRefPtr<JSONValue> jsonObjectForOldAndNewRects(const LayoutRect& oldRect, const LayoutRect& newRect)
{
    RefPtr<JSONObject> object = JSONObject::create();

    object->setValue("old", jsonObjectForRect(oldRect));
    object->setValue("new", jsonObjectForRect(newRect));
    return object.release();
}

bool RenderObject::invalidatePaintAfterLayoutIfNeeded(const RenderLayerModelObject* paintInvalidationContainer, bool wasSelfLayout,
    const LayoutRect& oldBounds, const LayoutPoint& oldLocation, const LayoutRect* newBoundsPtr, const LayoutPoint* newLocationPtr)
{
    RenderView* v = view();
    if (v->document().printing())
        return false; // Don't invalidate paints if we're printing.

    // This ASSERT fails due to animations.  See https://bugs.webkit.org/show_bug.cgi?id=37048
    // ASSERT(!newBoundsPtr || *newBoundsPtr == clippedOverflowRectForPaintInvalidation(paintInvalidationContainer));
    LayoutRect newBounds = newBoundsPtr ? *newBoundsPtr : computePaintInvalidationRect();
    LayoutPoint newLocation = newLocationPtr ? (*newLocationPtr) : RenderLayer::positionFromPaintInvalidationContainer(this, paintInvalidationContainer);

    // FIXME: This should use a ConvertableToTraceFormat when they are available in Blink.
    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), "RenderObject::invalidatePaintAfterLayoutIfNeeded()",
        "object", this->debugName().ascii(),
        "info", TracedValue::fromJSONValue(jsonObjectForOldAndNewRects(oldBounds, newBounds)));

    InvalidationReason invalidationReason = wasSelfLayout ? InvalidationSelfLayout : InvalidationIncremental;

    // Presumably a background or a border exists if border-fit:lines was specified.
    if (invalidationReason == InvalidationIncremental && style()->borderFit() == BorderFitLines)
        invalidationReason = InvalidationBorderFitLines;

    if (invalidationReason == InvalidationIncremental && style()->hasBorderRadius()) {
        // If a border-radius exists and width/height is smaller than
        // radius width/height, we cannot use delta-paint-invalidation.
        RoundedRect oldRoundedRect = style()->getRoundedBorderFor(oldBounds);
        RoundedRect newRoundedRect = style()->getRoundedBorderFor(newBounds);
        if (oldRoundedRect.radii() != newRoundedRect.radii())
            invalidationReason = InvalidationBorderRadius;
    }

    if (invalidationReason == InvalidationIncremental && compositingState() != PaintsIntoOwnBacking && newLocation != oldLocation)
        invalidationReason = InvalidationLocationChange;

    // If the bounds are the same then we know that none of the statements below
    // can match, so we can early out since we will not need to do any
    // invalidation.
    if (invalidationReason == InvalidationIncremental && oldBounds == newBounds)
        return false;

    if (invalidationReason == InvalidationIncremental) {
        if (oldBounds.width() != newBounds.width() && mustInvalidateBackgroundOrBorderPaintOnWidthChange())
            invalidationReason = InvalidationBoundsChangeWithBackground;
        else if (oldBounds.height() != newBounds.height() && mustInvalidateBackgroundOrBorderPaintOnHeightChange())
            invalidationReason = InvalidationBoundsChangeWithBackground;
    }

    // If we shifted, we don't know the exact reason so we are conservative and trigger a full invalidation. Shifting could
    // be caused by some layout property (left / top) or some in-flow renderer inserted / removed before us in the tree.
    if (invalidationReason == InvalidationIncremental && newBounds.location() != oldBounds.location())
        invalidationReason = InvalidationBoundsChange;

    // If the size is zero on one of our bounds then we know we're going to have
    // to do a full invalidation of either old bounds or new bounds. If we fall
    // into the incremental invalidation we'll issue two invalidations instead
    // of one.
    if (invalidationReason == InvalidationIncremental && (oldBounds.size().isZero() || newBounds.size().isZero()))
        invalidationReason = InvalidationBoundsChange;

    ASSERT(paintInvalidationContainer);

    if (invalidationReason != InvalidationIncremental) {
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(oldBounds), invalidationReason);
        if (newBounds != oldBounds)
            invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(newBounds), invalidationReason);
        return true;
    }

    LayoutUnit deltaLeft = newBounds.x() - oldBounds.x();
    if (deltaLeft > 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(oldBounds.x(), oldBounds.y(), deltaLeft, oldBounds.height()), invalidationReason);
    else if (deltaLeft < 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(newBounds.x(), newBounds.y(), -deltaLeft, newBounds.height()), invalidationReason);

    LayoutUnit deltaRight = newBounds.maxX() - oldBounds.maxX();
    if (deltaRight > 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(oldBounds.maxX(), newBounds.y(), deltaRight, newBounds.height()), invalidationReason);
    else if (deltaRight < 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(newBounds.maxX(), oldBounds.y(), -deltaRight, oldBounds.height()), invalidationReason);

    LayoutUnit deltaTop = newBounds.y() - oldBounds.y();
    if (deltaTop > 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(oldBounds.x(), oldBounds.y(), oldBounds.width(), deltaTop), invalidationReason);
    else if (deltaTop < 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(newBounds.x(), newBounds.y(), newBounds.width(), -deltaTop), invalidationReason);

    LayoutUnit deltaBottom = newBounds.maxY() - oldBounds.maxY();
    if (deltaBottom > 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(newBounds.x(), oldBounds.maxY(), newBounds.width(), deltaBottom), invalidationReason);
    else if (deltaBottom < 0)
        invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(oldBounds.x(), newBounds.maxY(), oldBounds.width(), -deltaBottom), invalidationReason);

    // FIXME: This is a limitation of our visual overflow being a single rectangle.
    if (!style()->boxShadow() && !style()->hasBorderImageOutsets() && !style()->hasOutline())
        return false;

    // We didn't move, but we did change size. Invalidate the delta, which will consist of possibly
    // two rectangles (but typically only one).
    RenderStyle* outlineStyle = outlineStyleForPaintInvalidation();
    LayoutUnit outlineWidth = outlineStyle->outlineSize();
    LayoutBoxExtent insetShadowExtent = style()->getBoxShadowInsetExtent();
    LayoutUnit width = absoluteValue(newBounds.width() - oldBounds.width());
    if (width) {
        LayoutUnit shadowLeft;
        LayoutUnit shadowRight;
        style()->getBoxShadowHorizontalExtent(shadowLeft, shadowRight);
        int borderRight = isBox() ? toRenderBox(this)->borderRight() : 0;
        LayoutUnit boxWidth = isBox() ? toRenderBox(this)->width() : LayoutUnit();
        LayoutUnit minInsetRightShadowExtent = min<LayoutUnit>(-insetShadowExtent.right(), min<LayoutUnit>(newBounds.width(), oldBounds.width()));
        LayoutUnit borderWidth = max<LayoutUnit>(borderRight, max<LayoutUnit>(valueForLength(style()->borderTopRightRadius().width(), boxWidth), valueForLength(style()->borderBottomRightRadius().width(), boxWidth)));
        LayoutUnit decorationsLeftWidth = max<LayoutUnit>(-outlineStyle->outlineOffset(), borderWidth + minInsetRightShadowExtent) + max<LayoutUnit>(outlineWidth, -shadowLeft);
        LayoutUnit decorationsRightWidth = max<LayoutUnit>(-outlineStyle->outlineOffset(), borderWidth + minInsetRightShadowExtent) + max<LayoutUnit>(outlineWidth, shadowRight);
        LayoutRect rightRect(newBounds.x() + min(newBounds.width(), oldBounds.width()) - decorationsLeftWidth,
            newBounds.y(),
            width + decorationsLeftWidth + decorationsRightWidth,
            max(newBounds.height(), oldBounds.height()));
        LayoutUnit right = min<LayoutUnit>(newBounds.maxX(), oldBounds.maxX());
        if (rightRect.x() < right) {
            rightRect.setWidth(min(rightRect.width(), right - rightRect.x()));
            invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(rightRect), invalidationReason);
        }
    }
    LayoutUnit height = absoluteValue(newBounds.height() - oldBounds.height());
    if (height) {
        LayoutUnit shadowTop;
        LayoutUnit shadowBottom;
        style()->getBoxShadowVerticalExtent(shadowTop, shadowBottom);
        int borderBottom = isBox() ? toRenderBox(this)->borderBottom() : 0;
        LayoutUnit boxHeight = isBox() ? toRenderBox(this)->height() : LayoutUnit();
        LayoutUnit minInsetBottomShadowExtent = min<LayoutUnit>(-insetShadowExtent.bottom(), min<LayoutUnit>(newBounds.height(), oldBounds.height()));
        LayoutUnit borderHeight = max<LayoutUnit>(borderBottom, max<LayoutUnit>(valueForLength(style()->borderBottomLeftRadius().height(), boxHeight), valueForLength(style()->borderBottomRightRadius().height(), boxHeight)));
        LayoutUnit decorationsTopHeight = max<LayoutUnit>(-outlineStyle->outlineOffset(), borderHeight + minInsetBottomShadowExtent) + max<LayoutUnit>(outlineWidth, -shadowTop);
        LayoutUnit decorationsBottomHeight = max<LayoutUnit>(-outlineStyle->outlineOffset(), borderHeight + minInsetBottomShadowExtent) + max<LayoutUnit>(outlineWidth, shadowBottom);
        LayoutRect bottomRect(newBounds.x(),
            min(newBounds.maxY(), oldBounds.maxY()) - decorationsTopHeight,
            max(newBounds.width(), oldBounds.width()),
            height + decorationsTopHeight + decorationsBottomHeight);
        LayoutUnit bottom = min(newBounds.maxY(), oldBounds.maxY());
        if (bottomRect.y() < bottom) {
            bottomRect.setHeight(min(bottomRect.height(), bottom - bottomRect.y()));
            invalidatePaintUsingContainer(paintInvalidationContainer, pixelSnappedIntRect(bottomRect), invalidationReason);
        }
    }
    return false;
}

void RenderObject::invalidatePaintForOverflow()
{
}

void RenderObject::invalidatePaintForOverflowIfNeeded()
{
    if (shouldInvalidateOverflowForPaint())
        invalidatePaintForOverflow();
}

bool RenderObject::checkForPaintInvalidation() const
{
    return !document().view()->needsFullPaintInvalidation() && everHadLayout();
}

bool RenderObject::checkForPaintInvalidationDuringLayout() const
{
    return !RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && checkForPaintInvalidation();
}

LayoutRect RenderObject::rectWithOutlineForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(clippedOverflowRectForPaintInvalidation(paintInvalidationContainer));
    r.inflate(outlineWidth);
    return r;
}

LayoutRect RenderObject::clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject*) const
{
    ASSERT_NOT_REACHED();
    return LayoutRect();
}

void RenderObject::mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer, LayoutRect& rect, bool fixed) const
{
    if (paintInvalidationContainer == this)
        return;

    if (RenderObject* o = parent()) {
        if (o->isRenderBlockFlow()) {
            RenderBlock* cb = toRenderBlock(o);
            if (cb->hasColumns())
                cb->adjustRectForColumns(rect);
        }

        if (o->hasOverflowClip()) {
            RenderBox* boxParent = toRenderBox(o);
            boxParent->applyCachedClipAndScrollOffsetForRepaint(rect);
            if (rect.isEmpty())
                return;
        }

        o->mapRectToPaintInvalidationBacking(paintInvalidationContainer, rect, fixed);
    }
}

void RenderObject::computeFloatRectForPaintInvalidation(const RenderLayerModelObject*, FloatRect&, bool) const
{
    ASSERT_NOT_REACHED();
}

void RenderObject::dirtyLinesFromChangedChild(RenderObject*)
{
}

#ifndef NDEBUG

void RenderObject::showTreeForThis() const
{
    if (node())
        node()->showTreeForThis();
}

void RenderObject::showRenderTreeForThis() const
{
    showRenderTree(this, 0);
}

void RenderObject::showLineTreeForThis() const
{
    if (containingBlock())
        containingBlock()->showLineTreeAndMark(0, 0, 0, 0, this);
}

void RenderObject::showRenderObject() const
{
    showRenderObject(0);
}

void RenderObject::showRenderObject(int printedCharacters) const
{
    // As this function is intended to be used when debugging, the
    // this pointer may be 0.
    if (!this) {
        fputs("(null)\n", stderr);
        return;
    }

    printedCharacters += fprintf(stderr, "%s %p", renderName(), this);

    if (node()) {
        if (printedCharacters)
            for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
                fputc(' ', stderr);
        fputc('\t', stderr);
        node()->showNode();
    } else
        fputc('\n', stderr);
}

void RenderObject::showRenderTreeAndMark(const RenderObject* markedObject1, const char* markedLabel1, const RenderObject* markedObject2, const char* markedLabel2, int depth) const
{
    int printedCharacters = 0;
    if (markedObject1 == this && markedLabel1)
        printedCharacters += fprintf(stderr, "%s", markedLabel1);
    if (markedObject2 == this && markedLabel2)
        printedCharacters += fprintf(stderr, "%s", markedLabel2);
    for (; printedCharacters < depth * 2; printedCharacters++)
        fputc(' ', stderr);

    showRenderObject(printedCharacters);
    if (!this)
        return;

    for (const RenderObject* child = slowFirstChild(); child; child = child->nextSibling())
        child->showRenderTreeAndMark(markedObject1, markedLabel1, markedObject2, markedLabel2, depth + 1);
}

#endif // NDEBUG

bool RenderObject::isSelectable() const
{
    return !isInert() && !(style()->userSelect() == SELECT_NONE && style()->userModify() == READ_ONLY);
}

Color RenderObject::selectionBackgroundColor() const
{
    if (!isSelectable())
        return Color::transparent;

    if (RefPtr<RenderStyle> pseudoStyle = getUncachedPseudoStyleFromParentOrShadowHost())
        return resolveColor(pseudoStyle.get(), CSSPropertyBackgroundColor).blendWithWhite();
    return frame()->selection().isFocusedAndActive() ?
        RenderTheme::theme().activeSelectionBackgroundColor() :
        RenderTheme::theme().inactiveSelectionBackgroundColor();
}

Color RenderObject::selectionColor(int colorProperty) const
{
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if (!isSelectable() || (frame()->view()->paintBehavior() & PaintBehaviorSelectionOnly))
        return resolveColor(colorProperty);

    if (RefPtr<RenderStyle> pseudoStyle = getUncachedPseudoStyleFromParentOrShadowHost())
        return resolveColor(pseudoStyle.get(), colorProperty);
    if (!RenderTheme::theme().supportsSelectionForegroundColors())
        return resolveColor(colorProperty);
    return frame()->selection().isFocusedAndActive() ?
        RenderTheme::theme().activeSelectionForegroundColor() :
        RenderTheme::theme().inactiveSelectionForegroundColor();
}

Color RenderObject::selectionForegroundColor() const
{
    return selectionColor(CSSPropertyWebkitTextFillColor);
}

Color RenderObject::selectionEmphasisMarkColor() const
{
    return selectionColor(CSSPropertyWebkitTextEmphasisColor);
}

void RenderObject::selectionStartEnd(int& spos, int& epos) const
{
    view()->selectionStartEnd(spos, epos);
}

void RenderObject::handleDynamicFloatPositionChange()
{
    // We have gone from not affecting the inline status of the parent flow to suddenly
    // having an impact.  See if there is a mismatch between the parent flow's
    // childrenInline() state and our state.
    setInline(style()->isDisplayInlineType());
    if (isInline() != parent()->childrenInline()) {
        if (!isInline())
            toRenderBoxModelObject(parent())->childBecameNonInline(this);
        else {
            // An anonymous block must be made to wrap this inline.
            RenderBlock* block = toRenderBlock(parent())->createAnonymousBlock();
            RenderObjectChildList* childlist = parent()->virtualChildren();
            childlist->insertChildNode(parent(), block, this);
            block->children()->appendChildNode(block, childlist->removeChildNode(parent(), this));
        }
    }
}

StyleDifference RenderObject::adjustStyleDifference(StyleDifference diff, unsigned contextSensitiveProperties) const
{
    if (contextSensitiveProperties & ContextSensitivePropertyTransform && isSVG())
        diff.setNeedsFullLayout();

    // If transform changed, and the layer does not paint into its own separate backing, then we need to invalidate paints.
    if (contextSensitiveProperties & ContextSensitivePropertyTransform) {
        // Text nodes share style with their parents but transforms don't apply to them,
        // hence the !isText() check.
        if (!isText() && (!hasLayer() || !toRenderLayerModelObject(this)->layer()->styleDeterminedCompositingReasons()))
            diff.setNeedsRepaintLayer();
        else
            diff.setNeedsRecompositeLayer();
    }

    // If opacity or zIndex changed, and the layer does not paint into its own separate backing, then we need to invalidate paints (also
    // ignoring text nodes)
    if (contextSensitiveProperties & (ContextSensitivePropertyOpacity | ContextSensitivePropertyZIndex)) {
        if (!isText() && (!hasLayer() || !toRenderLayerModelObject(this)->layer()->styleDeterminedCompositingReasons()))
            diff.setNeedsRepaintLayer();
        else
            diff.setNeedsRecompositeLayer();
    }

    // If filter changed, and the layer does not paint into its own separate backing or it paints with filters, then we need to invalidate paints.
    if ((contextSensitiveProperties & ContextSensitivePropertyFilter) && hasLayer()) {
        RenderLayer* layer = toRenderLayerModelObject(this)->layer();
        if (!layer->styleDeterminedCompositingReasons() || layer->paintsWithFilters())
            diff.setNeedsRepaintLayer();
        else
            diff.setNeedsRecompositeLayer();
    }

    if ((contextSensitiveProperties & ContextSensitivePropertyTextOrColor) && !diff.needsRepaint()
        && hasImmediateNonWhitespaceTextChildOrPropertiesDependentOnColor())
        diff.setNeedsRepaintObject();

    // The answer to layerTypeRequired() for plugins, iframes, and canvas can change without the actual
    // style changing, since it depends on whether we decide to composite these elements. When the
    // layer status of one of these elements changes, we need to force a layout.
    if (!diff.needsFullLayout() && style() && isLayerModelObject()) {
        bool requiresLayer = toRenderLayerModelObject(this)->layerTypeRequired() != NoLayer;
        if (hasLayer() != requiresLayer)
            diff.setNeedsFullLayout();
    }

    // If we have no layer(), just treat a RepaintLayer hint as a normal paint invalidation.
    if (diff.needsRepaintLayer() && !hasLayer()) {
        diff.clearNeedsRepaint();
        diff.setNeedsRepaintObject();
    }

    return diff;
}

void RenderObject::setPseudoStyle(PassRefPtr<RenderStyle> pseudoStyle)
{
    ASSERT(pseudoStyle->styleType() == BEFORE || pseudoStyle->styleType() == AFTER);

    // FIXME: We should consider just making all pseudo items use an inherited style.

    // Images are special and must inherit the pseudoStyle so the width and height of
    // the pseudo element doesn't change the size of the image. In all other cases we
    // can just share the style.
    //
    // Quotes are also RenderInline, so we need to create an inherited style to avoid
    // getting an inline with positioning or an invalid display.
    //
    if (isImage() || isQuote()) {
        RefPtr<RenderStyle> style = RenderStyle::create();
        style->inheritFrom(pseudoStyle.get());
        setStyle(style.release());
        return;
    }

    setStyle(pseudoStyle);
}

inline bool RenderObject::hasImmediateNonWhitespaceTextChildOrPropertiesDependentOnColor() const
{
    if (style()->hasBorder() || style()->hasOutline())
        return true;
    for (const RenderObject* r = slowFirstChild(); r; r = r->nextSibling()) {
        if (r->isText() && !toRenderText(r)->isAllCollapsibleWhitespace())
            return true;
        if (r->style()->hasOutline() || r->style()->hasBorder())
            return true;
    }
    return false;
}

void RenderObject::markContainingBlocksForOverflowRecalc()
{
    for (RenderBlock* container = containingBlock(); container && !container->childNeedsOverflowRecalcAfterStyleChange(); container = container->containingBlock())
        container->setChildNeedsOverflowRecalcAfterStyleChange(true);
}

void RenderObject::setNeedsOverflowRecalcAfterStyleChange()
{
    bool neededRecalc = needsOverflowRecalcAfterStyleChange();
    setSelfNeedsOverflowRecalcAfterStyleChange(true);
    if (!neededRecalc)
        markContainingBlocksForOverflowRecalc();
}

void RenderObject::setStyle(PassRefPtr<RenderStyle> style)
{
    ASSERT(style);

    if (m_style == style) {
        // We need to run through adjustStyleDifference() for iframes, plugins, and canvas so
        // style sharing is disabled for them. That should ensure that we never hit this code path.
        ASSERT(!isRenderIFrame() && !isEmbeddedObject() && !isCanvas());
        return;
    }

    StyleDifference diff;
    unsigned contextSensitiveProperties = ContextSensitivePropertyNone;
    if (m_style)
        diff = m_style->visualInvalidationDiff(*style, contextSensitiveProperties);

    diff = adjustStyleDifference(diff, contextSensitiveProperties);

    styleWillChange(diff, *style);

    RefPtr<RenderStyle> oldStyle = m_style.release();
    setStyleInternal(style);

    updateFillImages(oldStyle ? oldStyle->backgroundLayers() : 0, m_style ? m_style->backgroundLayers() : 0);
    updateFillImages(oldStyle ? oldStyle->maskLayers() : 0, m_style ? m_style->maskLayers() : 0);

    updateImage(oldStyle ? oldStyle->borderImage().image() : 0, m_style ? m_style->borderImage().image() : 0);
    updateImage(oldStyle ? oldStyle->maskBoxImage().image() : 0, m_style ? m_style->maskBoxImage().image() : 0);

    updateShapeImage(oldStyle ? oldStyle->shapeOutside() : 0, m_style ? m_style->shapeOutside() : 0);

    bool doesNotNeedLayout = !m_parent || isText();

    styleDidChange(diff, oldStyle.get());

    // FIXME: |this| might be destroyed here. This can currently happen for a RenderTextFragment when
    // its first-letter block gets an update in RenderTextFragment::styleDidChange. For RenderTextFragment(s),
    // we will safely bail out with the doesNotNeedLayout flag. We might want to broaden this condition
    // in the future as we move renderer changes out of layout and into style changes.
    if (doesNotNeedLayout)
        return;

    // Now that the layer (if any) has been updated, we need to adjust the diff again,
    // check whether we should layout now, and decide if we need to invalidate paints.
    StyleDifference updatedDiff = adjustStyleDifference(diff, contextSensitiveProperties);

    if (!diff.needsFullLayout()) {
        if (updatedDiff.needsFullLayout())
            setNeedsLayoutAndPrefWidthsRecalc();
        else if (updatedDiff.needsPositionedMovementLayout())
            setNeedsPositionedMovementLayout();
    }

    if (contextSensitiveProperties & ContextSensitivePropertyTransform && !needsLayout()) {
        if (RenderBlock* container = containingBlock())
            container->setNeedsOverflowRecalcAfterStyleChange();
    }

    if (updatedDiff.needsRepaintLayer()) {
        toRenderLayerModelObject(this)->layer()->repainter().repaintIncludingNonCompositingDescendants();
    } else if (updatedDiff.needsRepaint()) {
        // Invalidate paints with the new style, e.g., for example if we go from not having
        // an outline to having an outline.
        if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && needsLayout())
            setShouldDoFullPaintInvalidationAfterLayout(true);
        else if (!selfNeedsLayout())
            paintInvalidationForWholeRenderer();
    }
}

static inline bool rendererHasBackground(const RenderObject* renderer)
{
    return renderer && renderer->hasBackground();
}

void RenderObject::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    if (m_style) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        bool visibilityChanged = m_style->visibility() != newStyle.visibility()
            || m_style->zIndex() != newStyle.zIndex()
            || m_style->hasAutoZIndex() != newStyle.hasAutoZIndex();
        if (visibilityChanged) {
            document().setAnnotatedRegionsDirty(true);
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->childrenChanged(parent());
        }

        // Keep layer hierarchy visibility bits up to date if visibility changes.
        if (m_style->visibility() != newStyle.visibility()) {
            // We might not have an enclosing layer yet because we might not be in the tree.
            if (RenderLayer* layer = enclosingLayer()) {
                if (newStyle.visibility() == VISIBLE) {
                    layer->setHasVisibleContent();
                } else if (layer->hasVisibleContent() && (this == layer->renderer() || layer->renderer()->style()->visibility() != VISIBLE)) {
                    layer->dirtyVisibleContentStatus();
                    if (diff.needsLayout())
                        paintInvalidationForWholeRenderer();
                }
            }
        }

        if (m_parent && diff.needsRepaintObject()) {
            if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && (diff.needsLayout() || needsLayout()))
                setShouldDoFullPaintInvalidationAfterLayout(true);
            else if (!diff.needsFullLayout() && !selfNeedsLayout())
                paintInvalidationForWholeRenderer();
        }

        if (isFloating() && (m_style->floating() != newStyle.floating()))
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();
        else if (isOutOfFlowPositioned() && (m_style->position() != newStyle.position()))
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();

        s_affectsParentBlock = isFloatingOrOutOfFlowPositioned()
            && (!newStyle.isFloating() && !newStyle.hasOutOfFlowPosition())
            && parent() && (parent()->isRenderBlockFlow() || parent()->isRenderInline());

        // Clearing these bits is required to avoid leaving stale renderers.
        // FIXME: We shouldn't need that hack if our logic was totally correct.
        if (diff.needsLayout()) {
            setFloating(false);
            clearPositionedState();
        }
    } else {
        s_affectsParentBlock = false;
    }

    if (view()->frameView()) {
        bool shouldBlitOnFixedBackgroundImage = false;
        if (RuntimeEnabledFeatures::fastMobileScrollingEnabled()) {
            // On low-powered/mobile devices, preventing blitting on a scroll can cause noticeable delays
            // when scrolling a page with a fixed background image. As an optimization, assuming there are
            // no fixed positoned elements on the page, we can acclerate scrolling (via blitting) if we
            // ignore the CSS property "background-attachment: fixed".
            shouldBlitOnFixedBackgroundImage = true;
        }
        bool newStyleSlowScroll = !shouldBlitOnFixedBackgroundImage && newStyle.hasFixedBackgroundImage();
        bool oldStyleSlowScroll = m_style && !shouldBlitOnFixedBackgroundImage && m_style->hasFixedBackgroundImage();

        bool drawsRootBackground = isDocumentElement() || (isBody() && !rendererHasBackground(document().documentElement()->renderer()));
        if (drawsRootBackground && !shouldBlitOnFixedBackgroundImage) {
            if (view()->compositor()->supportsFixedRootBackgroundCompositing()) {
                if (newStyleSlowScroll && newStyle.hasEntirelyFixedBackground())
                    newStyleSlowScroll = false;

                if (oldStyleSlowScroll && m_style->hasEntirelyFixedBackground())
                    oldStyleSlowScroll = false;
            }
        }

        if (oldStyleSlowScroll != newStyleSlowScroll) {
            if (oldStyleSlowScroll)
                view()->frameView()->removeSlowRepaintObject();
            if (newStyleSlowScroll)
                view()->frameView()->addSlowRepaintObject();
        }
    }

    // Elements with non-auto touch-action will send a SetTouchAction message
    // on touchstart in EventHandler::handleTouchEvent, and so effectively have
    // a touchstart handler that must be reported.
    //
    // Since a CSS property cannot be applied directly to a text node, a
    // handler will have already been added for its parent so ignore it.
    TouchAction oldTouchAction = m_style ? m_style->touchAction() : TouchActionAuto;
    if (node() && !node()->isTextNode() && (oldTouchAction == TouchActionAuto) != (newStyle.touchAction() == TouchActionAuto)) {
        if (newStyle.touchAction() != TouchActionAuto)
            document().didAddTouchEventHandler(node());
        else
            document().didRemoveTouchEventHandler(node());
    }
}

static bool areNonIdenticalCursorListsEqual(const RenderStyle* a, const RenderStyle* b)
{
    ASSERT(a->cursors() != b->cursors());
    return a->cursors() && b->cursors() && *a->cursors() == *b->cursors();
}

static inline bool areCursorsEqual(const RenderStyle* a, const RenderStyle* b)
{
    return a->cursor() == b->cursor() && (a->cursors() == b->cursors() || areNonIdenticalCursorListsEqual(a, b));
}

void RenderObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (s_affectsParentBlock)
        handleDynamicFloatPositionChange();

    if (!m_parent)
        return;

    if (diff.needsFullLayout()) {
        RenderCounter::rendererStyleChanged(*this, oldStyle, m_style.get());

        // If the object already needs layout, then setNeedsLayout won't do
        // any work. But if the containing block has changed, then we may need
        // to mark the new containing blocks for layout. The change that can
        // directly affect the containing block of this object is a change to
        // the position style.
        if (needsLayout() && oldStyle->position() != m_style->position())
            markContainingBlocksForLayout();

        // Ditto.
        if (needsOverflowRecalcAfterStyleChange() && oldStyle->position() != m_style->position())
            markContainingBlocksForOverflowRecalc();

        if (diff.needsFullLayout())
            setNeedsLayoutAndPrefWidthsRecalc();
    } else if (diff.needsPositionedMovementLayout())
        setNeedsPositionedMovementLayout();

    // Don't check for paint invalidation here; we need to wait until the layer has been
    // updated by subclasses before we know if we have to invalidate paints (in setStyle()).

    if (oldStyle && !areCursorsEqual(oldStyle, style())) {
        if (LocalFrame* frame = this->frame())
            frame->eventHandler().scheduleCursorUpdate();
    }
}

void RenderObject::propagateStyleToAnonymousChildren(bool blockChildrenOnly)
{
    // FIXME: We could save this call when the change only affected non-inherited properties.
    for (RenderObject* child = slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->isAnonymous() || child->style()->styleType() != NOPSEUDO)
            continue;

        if (blockChildrenOnly && !child->isRenderBlock())
            continue;

        if (child->isRenderFullScreen() || child->isRenderFullScreenPlaceholder())
            continue;

        RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), child->style()->display());
        if (style()->specifiesColumns()) {
            if (child->style()->specifiesColumns())
                newStyle->inheritColumnPropertiesFrom(style());
            if (child->style()->columnSpan())
                newStyle->setColumnSpan(ColumnSpanAll);
        }

        // Preserve the position style of anonymous block continuations as they can have relative or sticky position when
        // they contain block descendants of relative or sticky positioned inlines.
        if (child->isInFlowPositioned() && toRenderBlock(child)->isAnonymousBlockContinuation())
            newStyle->setPosition(child->style()->position());

        child->setStyle(newStyle.release());
    }
}

void RenderObject::updateFillImages(const FillLayer* oldLayers, const FillLayer* newLayers)
{
    // Optimize the common case
    if (oldLayers && !oldLayers->next() && newLayers && !newLayers->next() && (oldLayers->image() == newLayers->image()))
        return;

    // Go through the new layers and addClients first, to avoid removing all clients of an image.
    for (const FillLayer* currNew = newLayers; currNew; currNew = currNew->next()) {
        if (currNew->image())
            currNew->image()->addClient(this);
    }

    for (const FillLayer* currOld = oldLayers; currOld; currOld = currOld->next()) {
        if (currOld->image())
            currOld->image()->removeClient(this);
    }
}

void RenderObject::updateImage(StyleImage* oldImage, StyleImage* newImage)
{
    if (oldImage != newImage) {
        if (oldImage)
            oldImage->removeClient(this);
        if (newImage)
            newImage->addClient(this);
    }
}

void RenderObject::updateShapeImage(const ShapeValue* oldShapeValue, const ShapeValue* newShapeValue)
{
    if (oldShapeValue || newShapeValue)
        updateImage(oldShapeValue ? oldShapeValue->image() : 0, newShapeValue ? newShapeValue->image() : 0);
}

LayoutRect RenderObject::viewRect() const
{
    return view()->viewRect();
}

FloatPoint RenderObject::localToAbsolute(const FloatPoint& localPoint, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(0, transformState, mode | ApplyContainerFlip);
    transformState.flatten();

    return transformState.lastPlanarPoint();
}

FloatPoint RenderObject::absoluteToLocal(const FloatPoint& containerPoint, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, containerPoint);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();

    return transformState.lastPlanarPoint();
}

FloatQuad RenderObject::absoluteToLocalQuad(const FloatQuad& quad, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, quad.boundingBox().center(), quad);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

void RenderObject::mapLocalToContainer(const RenderLayerModelObject* paintInvalidationContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (paintInvalidationContainer == this)
        return;

    RenderObject* o = parent();
    if (!o)
        return;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutPoint centerPoint = roundedLayoutPoint(transformState.mappedPoint());
    if (mode & ApplyContainerFlip && o->isBox()) {
        if (o->style()->isFlippedBlocksWritingMode())
            transformState.move(toRenderBox(o)->flipForWritingModeIncludingColumns(roundedLayoutPoint(transformState.mappedPoint())) - centerPoint);
        mode &= ~ApplyContainerFlip;
    }

    transformState.move(o->columnOffset(roundedLayoutPoint(transformState.mappedPoint())));

    if (o->hasOverflowClip())
        transformState.move(-toRenderBox(o)->scrolledContentOffset());

    o->mapLocalToContainer(paintInvalidationContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != this);

    RenderObject* container = parent();
    if (!container)
        return 0;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutSize offset;
    if (container->hasOverflowClip())
        offset = -toRenderBox(container)->scrolledContentOffset();

    geometryMap.push(this, offset, hasColumns());

    return container;
}

void RenderObject::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    RenderObject* o = parent();
    if (o) {
        o->mapAbsoluteToLocalPoint(mode, transformState);
        if (o->hasOverflowClip())
            transformState.move(toRenderBox(o)->scrolledContentOffset());
    }
}

bool RenderObject::shouldUseTransformFromContainer(const RenderObject* containerObject) const
{
    // hasTransform() indicates whether the object has transform, transform-style or perspective. We just care about transform,
    // so check the layer's transform directly.
    return (hasLayer() && toRenderLayerModelObject(this)->layer()->transform()) || (containerObject && containerObject->style()->hasPerspective());
}

void RenderObject::getTransformFromContainer(const RenderObject* containerObject, const LayoutSize& offsetInContainer, TransformationMatrix& transform) const
{
    transform.makeIdentity();
    transform.translate(offsetInContainer.width().toFloat(), offsetInContainer.height().toFloat());
    RenderLayer* layer;
    if (hasLayer() && (layer = toRenderLayerModelObject(this)->layer()) && layer->transform())
        transform.multiply(layer->currentTransform());

    if (containerObject && containerObject->hasLayer() && containerObject->style()->hasPerspective()) {
        // Perpsective on the container affects us, so we have to factor it in here.
        ASSERT(containerObject->hasLayer());
        FloatPoint perspectiveOrigin = toRenderLayerModelObject(containerObject)->layer()->perspectiveOrigin();

        TransformationMatrix perspectiveMatrix;
        perspectiveMatrix.applyPerspective(containerObject->style()->perspective());

        transform.translateRight3d(-perspectiveOrigin.x(), -perspectiveOrigin.y(), 0);
        transform = perspectiveMatrix * transform;
        transform.translateRight3d(perspectiveOrigin.x(), perspectiveOrigin.y(), 0);
    }
}

FloatQuad RenderObject::localToContainerQuad(const FloatQuad& localQuad, const RenderLayerModelObject* paintInvalidationContainer, MapCoordinatesFlags mode, bool* wasFixed) const
{
    // Track the point at the center of the quad's bounding box. As mapLocalToContainer() calls offsetFromContainer(),
    // it will use that point as the reference point to decide which column's transform to apply in multiple-column blocks.
    TransformState transformState(TransformState::ApplyTransformDirection, localQuad.boundingBox().center(), localQuad);
    mapLocalToContainer(paintInvalidationContainer, transformState, mode | ApplyContainerFlip | UseTransforms, wasFixed);
    transformState.flatten();

    return transformState.lastPlanarQuad();
}

FloatPoint RenderObject::localToContainerPoint(const FloatPoint& localPoint, const RenderLayerModelObject* paintInvalidationContainer, MapCoordinatesFlags mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(paintInvalidationContainer, transformState, mode | ApplyContainerFlip | UseTransforms, wasFixed);
    transformState.flatten();

    return transformState.lastPlanarPoint();
}

LayoutSize RenderObject::offsetFromContainer(const RenderObject* o, const LayoutPoint& point, bool* offsetDependsOnPoint) const
{
    ASSERT(o == container());

    LayoutSize offset = o->columnOffset(point);

    if (o->hasOverflowClip())
        offset -= toRenderBox(o)->scrolledContentOffset();

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = hasColumns() || o->isRenderFlowThread();

    return offset;
}

LayoutSize RenderObject::offsetFromAncestorContainer(const RenderObject* container) const
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    const RenderObject* currContainer = this;
    do {
        const RenderObject* nextContainer = currContainer->container();
        ASSERT(nextContainer);  // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        ASSERT(!currContainer->hasTransform());
        LayoutSize currentOffset = currContainer->offsetFromContainer(nextContainer, referencePoint);
        offset += currentOffset;
        referencePoint.move(currentOffset);
        currContainer = nextContainer;
    } while (currContainer != container);

    return offset;
}

LayoutRect RenderObject::localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine)
{
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = 0;

    return LayoutRect();
}

void RenderObject::computeLayerHitTestRects(LayerHitTestRects& layerRects) const
{
    // Figure out what layer our container is in. Any offset (or new layer) for this
    // renderer within it's container will be applied in addLayerHitTestRects.
    LayoutPoint layerOffset;
    const RenderLayer* currentLayer = 0;

    if (!hasLayer()) {
        RenderObject* container = this->container();
        currentLayer = container->enclosingLayer();
        if (container && currentLayer->renderer() != container) {
            layerOffset.move(container->offsetFromAncestorContainer(currentLayer->renderer()));
            // If the layer itself is scrolled, we have to undo the subtraction of its scroll
            // offset since we want the offset relative to the scrolling content, not the
            // element itself.
            if (currentLayer->renderer()->hasOverflowClip())
                layerOffset.move(currentLayer->renderBox()->scrolledContentOffset());
        }
    }

    this->addLayerHitTestRects(layerRects, currentLayer, layerOffset, LayoutRect());
}

void RenderObject::addLayerHitTestRects(LayerHitTestRects& layerRects, const RenderLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    ASSERT(currentLayer);
    ASSERT(currentLayer == this->enclosingLayer());

    // Compute the rects for this renderer only and add them to the results.
    // Note that we could avoid passing the offset and instead adjust each result, but this
    // seems slightly simpler.
    Vector<LayoutRect> ownRects;
    LayoutRect newContainerRect;
    computeSelfHitTestRects(ownRects, layerOffset);

    // When we get to have a lot of rects on a layer, the performance cost of tracking those
    // rects outweighs the benefit of doing compositor thread hit testing.
    // FIXME: This limit needs to be low due to the O(n^2) algorithm in
    // WebLayer::setTouchEventHandlerRegion - crbug.com/300282.
    const size_t maxRectsPerLayer = 100;

    LayerHitTestRects::iterator iter = layerRects.find(currentLayer);
    Vector<WebCore::LayoutRect>* iterValue;
    if (iter == layerRects.end())
        iterValue = &layerRects.add(currentLayer, Vector<LayoutRect>()).storedValue->value;
    else
        iterValue = &iter->value;
    for (size_t i = 0; i < ownRects.size(); i++) {
        if (!containerRect.contains(ownRects[i])) {
            iterValue->append(ownRects[i]);
            if (iterValue->size() > maxRectsPerLayer) {
                // Just mark the entire layer instead, and switch to walking the layer
                // tree instead of the render tree.
                layerRects.remove(currentLayer);
                currentLayer->addLayerHitTestRects(layerRects);
                return;
            }
            if (newContainerRect.isEmpty())
                newContainerRect = ownRects[i];
        }
    }
    if (newContainerRect.isEmpty())
        newContainerRect = containerRect;

    // If it's possible for children to have rects outside our bounds, then we need to descend into
    // the children and compute them.
    // Ideally there would be other cases where we could detect that children couldn't have rects
    // outside our bounds and prune the tree walk.
    // Note that we don't use Region here because Union is O(N) - better to just keep a list of
    // partially redundant rectangles. If we find examples where this is expensive, then we could
    // rewrite Region to be more efficient. See https://bugs.webkit.org/show_bug.cgi?id=100814.
    if (!isRenderView()) {
        for (RenderObject* curr = slowFirstChild(); curr; curr = curr->nextSibling()) {
            curr->addLayerHitTestRects(layerRects, currentLayer,  layerOffset, newContainerRect);
        }
    }
}

bool RenderObject::isRooted() const
{
    const RenderObject* object = this;
    while (object->parent() && !object->hasLayer())
        object = object->parent();
    if (object->hasLayer())
        return toRenderLayerModelObject(object)->layer()->root()->isRootLayer();
    return false;
}

RenderObject* RenderObject::rendererForRootBackground()
{
    ASSERT(isDocumentElement());
    if (!hasBackground() && isHTMLHtmlElement(node())) {
        // Locate the <body> element using the DOM. This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc. We can locate the <body>
        // render object very easily via the DOM.
        HTMLElement* body = document().body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (bodyObject)
            return bodyObject;
    }

    return this;
}

RespectImageOrientationEnum RenderObject::shouldRespectImageOrientation() const
{
    // Respect the image's orientation if it's being used as a full-page image or it's
    // an <img> and the setting to respect it everywhere is set.
    return document().isImageDocument()
        || (document().settings() && document().settings()->shouldRespectImageOrientation() && isHTMLImageElement(node())) ? RespectImageOrientation : DoNotRespectImageOrientation;
}

bool RenderObject::hasOutlineAnnotation() const
{
    return node() && node()->isLink() && document().printing();
}

bool RenderObject::hasEntirelyFixedBackground() const
{
    return m_style->hasEntirelyFixedBackground();
}

RenderObject* RenderObject::container(const RenderLayerModelObject* paintInvalidationContainer, bool* paintInvalidationContainerSkipped) const
{
    if (paintInvalidationContainerSkipped)
        *paintInvalidationContainerSkipped = false;

    // This method is extremely similar to containingBlock(), but with a few notable
    // exceptions.
    // (1) It can be used on orphaned subtrees, i.e., it can be called safely even when
    // the object is not part of the primary document subtree yet.
    // (2) For normal flow elements, it just returns the parent.
    // (3) For absolute positioned elements, it will return a relative positioned inline.
    // containingBlock() simply skips relpositioned inlines and lets an enclosing block handle
    // the layout of the positioned object.  This does mean that computePositionedLogicalWidth and
    // computePositionedLogicalHeight have to use container().
    RenderObject* o = parent();

    if (isText())
        return o;

    EPosition pos = m_style->position();
    if (pos == FixedPosition) {
        return containerForFixedPosition(paintInvalidationContainer, paintInvalidationContainerSkipped);
    } else if (pos == AbsolutePosition) {
        // We technically just want our containing block, but
        // we may not have one if we're part of an uninstalled
        // subtree. We'll climb as high as we can though.
        while (o) {
            if (o->style()->position() != StaticPosition)
                break;

            if (o->canContainFixedPositionObjects())
                break;

            if (paintInvalidationContainerSkipped && o == paintInvalidationContainer)
                *paintInvalidationContainerSkipped = true;

            o = o->parent();
        }
    }

    return o;
}

bool RenderObject::isSelectionBorder() const
{
    SelectionState st = selectionState();
    return st == SelectionStart || st == SelectionEnd || st == SelectionBoth;
}

inline void RenderObject::clearLayoutRootIfNeeded() const
{
    if (frame()) {
        if (FrameView* view = frame()->view()) {
            if (view->layoutRoot() == this) {
                if (!documentBeingDestroyed())
                    ASSERT_NOT_REACHED();
                // This indicates a failure to layout the child, which is why
                // the layout root is still set to |this|. Make sure to clear it
                // since we are getting destroyed.
                view->clearLayoutSubtreeRoot();
            }
        }
    }
}

void RenderObject::willBeDestroyed()
{
    // Destroy any leftover anonymous children.
    RenderObjectChildList* children = virtualChildren();
    if (children)
        children->destroyLeftoverChildren();

    // If this renderer is being autoscrolled, stop the autoscrolling.
    if (LocalFrame* frame = this->frame()) {
        if (frame->page())
            frame->page()->autoscrollController().stopAutoscrollIfNeeded(this);
    }

    // For accessibility management, notify the parent of the imminent change to its child set.
    // We do it now, before remove(), while the parent pointer is still available.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this->parent());

    remove();

    // The remove() call above may invoke axObjectCache()->childrenChanged() on the parent, which may require the AX render
    // object for this renderer. So we remove the AX render object now, after the renderer is removed.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->remove(this);

    // If this renderer had a parent, remove should have destroyed any counters
    // attached to this renderer and marked the affected other counters for
    // reevaluation. This apparently redundant check is here for the case when
    // this renderer had no parent at the time remove() was called.

    if (hasCounterNodeMap())
        RenderCounter::destroyCounterNodes(*this);

    // Remove the handler if node had touch-action set. Don't call when
    // document is being destroyed as all handlers will have been cleared
    // previously. Handlers are not added for text nodes so don't try removing
    // for one too. Need to check if m_style is null in cases of partial construction.
    if (!documentBeingDestroyed() && node() && !node()->isTextNode() && m_style && m_style->touchAction() != TouchActionAuto)
        document().didRemoveTouchEventHandler(node());

    setAncestorLineBoxDirty(false);

    clearLayoutRootIfNeeded();
}

void RenderObject::insertedIntoTree()
{
    // FIXME: We should ASSERT(isRooted()) here but generated content makes some out-of-order insertion.

    // Keep our layer hierarchy updated. Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    RenderLayer* layer = 0;
    if (slowFirstChild() || hasLayer()) {
        layer = parent()->enclosingLayer();
        addLayers(layer);
    }

    // If |this| is visible but this object was not, tell the layer it has some visible content
    // that needs to be drawn and layer visibility optimization can't be used
    if (parent()->style()->visibility() != VISIBLE && style()->visibility() == VISIBLE && !hasLayer()) {
        if (!layer)
            layer = parent()->enclosingLayer();
        if (layer)
            layer->setHasVisibleContent();
    }

    if (!isFloating() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(this);
}

void RenderObject::willBeRemovedFromTree()
{
    // FIXME: We should ASSERT(isRooted()) but we have some out-of-order removals which would need to be fixed first.

    // If we remove a visible child from an invisible parent, we don't know the layer visibility any more.
    RenderLayer* layer = 0;
    if (parent()->style()->visibility() != VISIBLE && style()->visibility() == VISIBLE && !hasLayer()) {
        layer = parent()->enclosingLayer();
        if (layer)
            layer->dirtyVisibleContentStatus();
    }

    // Keep our layer hierarchy updated.
    if (slowFirstChild() || hasLayer()) {
        if (!layer)
            layer = parent()->enclosingLayer();
        removeLayers(layer);
    }

    if (isOutOfFlowPositioned() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(this);

    removeFromRenderFlowThread();

    // Update cached boundaries in SVG renderers if a child is removed.
    if (parent()->isSVG())
        parent()->setNeedsBoundariesUpdate();
}

void RenderObject::removeFromRenderFlowThread()
{
    if (flowThreadState() == NotInsideFlowThread)
        return;

    // Sometimes we remove the element from the flow, but it's not destroyed at that time.
    // It's only until later when we actually destroy it and remove all the children from it.
    // Currently, that happens for firstLetter elements and list markers.
    // Pass in the flow thread so that we don't have to look it up for all the children.
    removeFromRenderFlowThreadRecursive(flowThreadContainingBlock());
}

void RenderObject::removeFromRenderFlowThreadRecursive(RenderFlowThread* renderFlowThread)
{
    if (const RenderObjectChildList* children = virtualChildren()) {
        for (RenderObject* child = children->firstChild(); child; child = child->nextSibling())
            child->removeFromRenderFlowThreadRecursive(renderFlowThread);
    }

    setFlowThreadState(NotInsideFlowThread);
}

void RenderObject::destroyAndCleanupAnonymousWrappers()
{
    // If the tree is destroyed, there is no need for a clean-up phase.
    if (documentBeingDestroyed()) {
        destroy();
        return;
    }

    RenderObject* destroyRoot = this;
    for (RenderObject* destroyRootParent = destroyRoot->parent(); destroyRootParent && destroyRootParent->isAnonymous(); destroyRoot = destroyRootParent, destroyRootParent = destroyRootParent->parent()) {
        // Anonymous block continuations are tracked and destroyed elsewhere (see the bottom of RenderBlock::removeChild)
        if (destroyRootParent->isRenderBlock() && toRenderBlock(destroyRootParent)->isAnonymousBlockContinuation())
            break;
        // Render flow threads are tracked by the FlowThreadController, so we can't destroy them here.
        // Column spans are tracked elsewhere.
        if (destroyRootParent->isRenderFlowThread() || destroyRootParent->isAnonymousColumnSpanBlock())
            break;

        if (destroyRootParent->slowFirstChild() != this || destroyRootParent->slowLastChild() != this)
            break;
    }

    destroyRoot->destroy();

    // WARNING: |this| is deleted here.
}

void RenderObject::destroy()
{
    willBeDestroyed();
    postDestroy();
}

void RenderObject::removeShapeImageClient(ShapeValue* shapeValue)
{
    if (!shapeValue)
        return;
    if (StyleImage* shapeImage = shapeValue->image())
        shapeImage->removeClient(this);
}

void RenderObject::postDestroy()
{
    // It seems ugly that this is not in willBeDestroyed().
    if (m_style) {
        for (const FillLayer* bgLayer = m_style->backgroundLayers(); bgLayer; bgLayer = bgLayer->next()) {
            if (StyleImage* backgroundImage = bgLayer->image())
                backgroundImage->removeClient(this);
        }

        for (const FillLayer* maskLayer = m_style->maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
            if (StyleImage* maskImage = maskLayer->image())
                maskImage->removeClient(this);
        }

        if (StyleImage* borderImage = m_style->borderImage().image())
            borderImage->removeClient(this);

        if (StyleImage* maskBoxImage = m_style->maskBoxImage().image())
            maskBoxImage->removeClient(this);

        removeShapeImageClient(m_style->shapeOutside());
    }

    delete this;
}

PositionWithAffinity RenderObject::positionForPoint(const LayoutPoint&)
{
    return createPositionWithAffinity(caretMinOffset(), DOWNSTREAM);
}

void RenderObject::updateDragState(bool dragOn)
{
    bool valueChanged = (dragOn != isDragging());
    setIsDragging(dragOn);
    if (valueChanged && node()) {
        if (node()->isElementNode() && toElement(node())->childrenOrSiblingsAffectedByDrag())
            node()->setNeedsStyleRecalc(SubtreeStyleChange);
        else if (style()->affectedByDrag())
            node()->setNeedsStyleRecalc(LocalStyleChange);
    }
    for (RenderObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
        curr->updateDragState(dragOn);
}

CompositingState RenderObject::compositingState() const
{
    return hasLayer() ? toRenderLayerModelObject(this)->layer()->compositingState() : NotComposited;
}

CompositingReasons RenderObject::additionalCompositingReasons(CompositingTriggerFlags) const
{
    return CompositingReasonNone;
}

bool RenderObject::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter hitTestFilter)
{
    bool inside = false;
    if (hitTestFilter != HitTestSelf) {
        // First test the foreground layer (lines and inlines).
        inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestForeground);

        // Test floats next.
        if (!inside)
            inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestFloat);

        // Finally test to see if the mouse is in the background (within a child block's background).
        if (!inside)
            inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestChildBlockBackgrounds);
    }

    // See if the mouse is inside us but not any of our descendants
    if (hitTestFilter != HitTestDescendants && !inside)
        inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestBlockBackground);

    return inside;
}

void RenderObject::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = this->node();

    // If we hit the anonymous renderers inside generated content we should
    // actually hit the generated content so walk up to the PseudoElement.
    if (!node && parent() && parent()->isBeforeOrAfterContent()) {
        for (RenderObject* renderer = parent(); renderer && !node; renderer = renderer->parent())
            node = renderer->node();
    }

    if (node) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(point);
    }
}

bool RenderObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& /*locationInContainer*/, const LayoutPoint& /*accumulatedOffset*/, HitTestAction)
{
    return false;
}

void RenderObject::scheduleRelayout()
{
    if (isRenderView()) {
        FrameView* view = toRenderView(this)->frameView();
        if (view)
            view->scheduleRelayout();
    } else {
        if (isRooted()) {
            if (RenderView* renderView = view()) {
                if (FrameView* frameView = renderView->frameView())
                    frameView->scheduleRelayoutOfSubtree(this);
            }
        }
    }
}

void RenderObject::forceLayout()
{
    setSelfNeedsLayout(true);
    setShouldDoFullPaintInvalidationAfterLayout(true);
    layout();
}

// FIXME: Does this do anything different than forceLayout given that we don't walk
// the containing block chain. If not, we should change all callers to use forceLayout.
void RenderObject::forceChildLayout()
{
    setNormalChildNeedsLayout(true);
    layout();
}

enum StyleCacheState {
    Cached,
    Uncached
};

static PassRefPtr<RenderStyle> firstLineStyleForCachedUncachedType(StyleCacheState type, const RenderObject* renderer, RenderStyle* style)
{
    const RenderObject* rendererForFirstLineStyle = renderer;
    if (renderer->isBeforeOrAfterContent())
        rendererForFirstLineStyle = renderer->parent();

    if (rendererForFirstLineStyle->isRenderBlockFlow() || rendererForFirstLineStyle->isRenderButton()) {
        if (RenderBlock* firstLineBlock = rendererForFirstLineStyle->firstLineBlock()) {
            if (type == Cached)
                return firstLineBlock->getCachedPseudoStyle(FIRST_LINE, style);
            return firstLineBlock->getUncachedPseudoStyle(PseudoStyleRequest(FIRST_LINE), style, firstLineBlock == renderer ? style : 0);
        }
    } else if (!rendererForFirstLineStyle->isAnonymous() && rendererForFirstLineStyle->isRenderInline()) {
        RenderStyle* parentStyle = rendererForFirstLineStyle->parent()->firstLineStyle();
        if (parentStyle != rendererForFirstLineStyle->parent()->style()) {
            if (type == Cached) {
                // A first-line style is in effect. Cache a first-line style for ourselves.
                rendererForFirstLineStyle->style()->setHasPseudoStyle(FIRST_LINE_INHERITED);
                return rendererForFirstLineStyle->getCachedPseudoStyle(FIRST_LINE_INHERITED, parentStyle);
            }
            return rendererForFirstLineStyle->getUncachedPseudoStyle(PseudoStyleRequest(FIRST_LINE_INHERITED), parentStyle, style);
        }
    }
    return nullptr;
}

PassRefPtr<RenderStyle> RenderObject::uncachedFirstLineStyle(RenderStyle* style) const
{
    if (!document().styleEngine()->usesFirstLineRules())
        return nullptr;

    ASSERT(!isText());

    return firstLineStyleForCachedUncachedType(Uncached, this, style);
}

RenderStyle* RenderObject::cachedFirstLineStyle() const
{
    ASSERT(document().styleEngine()->usesFirstLineRules());

    if (RefPtr<RenderStyle> style = firstLineStyleForCachedUncachedType(Cached, isText() ? parent() : this, m_style.get()))
        return style.get();

    return m_style.get();
}

RenderStyle* RenderObject::getCachedPseudoStyle(PseudoId pseudo, RenderStyle* parentStyle) const
{
    if (pseudo < FIRST_INTERNAL_PSEUDOID && !style()->hasPseudoStyle(pseudo))
        return 0;

    RenderStyle* cachedStyle = style()->getCachedPseudoStyle(pseudo);
    if (cachedStyle)
        return cachedStyle;

    RefPtr<RenderStyle> result = getUncachedPseudoStyle(PseudoStyleRequest(pseudo), parentStyle);
    if (result)
        return style()->addCachedPseudoStyle(result.release());
    return 0;
}

PassRefPtr<RenderStyle> RenderObject::getUncachedPseudoStyle(const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle, RenderStyle* ownStyle) const
{
    if (pseudoStyleRequest.pseudoId < FIRST_INTERNAL_PSEUDOID && !ownStyle && !style()->hasPseudoStyle(pseudoStyleRequest.pseudoId))
        return nullptr;

    if (!parentStyle) {
        ASSERT(!ownStyle);
        parentStyle = style();
    }

    if (!node())
        return nullptr;

    Element* element = Traversal<Element>::firstAncestorOrSelf(*node());
    if (!element)
        return nullptr;

    if (pseudoStyleRequest.pseudoId == FIRST_LINE_INHERITED) {
        RefPtr<RenderStyle> result = document().ensureStyleResolver().styleForElement(element, parentStyle, DisallowStyleSharing);
        result->setStyleType(FIRST_LINE_INHERITED);
        return result.release();
    }

    return document().ensureStyleResolver().pseudoStyleForElement(element, pseudoStyleRequest, parentStyle);
}

PassRefPtr<RenderStyle> RenderObject::getUncachedPseudoStyleFromParentOrShadowHost() const
{
    if (!node())
        return nullptr;

    if (ShadowRoot* root = node()->containingShadowRoot()) {
        if (root->type() == ShadowRoot::UserAgentShadowRoot) {
            if (Element* shadowHost = node()->shadowHost()) {
                return shadowHost->renderer()->getUncachedPseudoStyle(PseudoStyleRequest(SELECTION));
            }
        }
    }

    return getUncachedPseudoStyle(PseudoStyleRequest(SELECTION));
}

bool RenderObject::hasBlendMode() const
{
    return RuntimeEnabledFeatures::cssCompositingEnabled() && style() && style()->hasBlendMode();
}

void RenderObject::getTextDecorations(unsigned decorations, AppliedTextDecoration& underline, AppliedTextDecoration& overline, AppliedTextDecoration& linethrough, bool quirksMode, bool firstlineStyle)
{
    RenderObject* curr = this;
    RenderStyle* styleToUse = 0;
    unsigned currDecs = TextDecorationNone;
    Color resultColor;
    TextDecorationStyle resultStyle;
    do {
        styleToUse = curr->style(firstlineStyle);
        currDecs = styleToUse->textDecoration();
        currDecs &= decorations;
        resultColor = styleToUse->visitedDependentDecorationColor();
        resultStyle = styleToUse->textDecorationStyle();
        // Parameter 'decorations' is cast as an int to enable the bitwise operations below.
        if (currDecs) {
            if (currDecs & TextDecorationUnderline) {
                decorations &= ~TextDecorationUnderline;
                underline.color = resultColor;
                underline.style = resultStyle;
            }
            if (currDecs & TextDecorationOverline) {
                decorations &= ~TextDecorationOverline;
                overline.color = resultColor;
                overline.style = resultStyle;
            }
            if (currDecs & TextDecorationLineThrough) {
                decorations &= ~TextDecorationLineThrough;
                linethrough.color = resultColor;
                linethrough.style = resultStyle;
            }
        }
        if (curr->isRubyText())
            return;
        curr = curr->parent();
        if (curr && curr->isAnonymousBlock() && toRenderBlock(curr)->continuation())
            curr = toRenderBlock(curr)->continuation();
    } while (curr && decorations && (!quirksMode || !curr->node() || (!isHTMLAnchorElement(*curr->node()) && !isHTMLFontElement(*curr->node()))));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (decorations && curr) {
        styleToUse = curr->style(firstlineStyle);
        resultColor = styleToUse->visitedDependentDecorationColor();
        if (decorations & TextDecorationUnderline) {
            underline.color = resultColor;
            underline.style = resultStyle;
        }
        if (decorations & TextDecorationOverline) {
            overline.color = resultColor;
            overline.style = resultStyle;
        }
        if (decorations & TextDecorationLineThrough) {
            linethrough.color = resultColor;
            linethrough.style = resultStyle;
        }
    }
}

void RenderObject::addAnnotatedRegions(Vector<AnnotatedRegionValue>& regions)
{
    // Convert the style regions to absolute coordinates.
    if (style()->visibility() != VISIBLE || !isBox())
        return;

    if (style()->getDraggableRegionMode() == DraggableRegionNone)
        return;

    RenderBox* box = toRenderBox(this);
    FloatRect localBounds(FloatPoint(), FloatSize(box->width().toFloat(), box->height().toFloat()));
    FloatRect absBounds = localToAbsoluteQuad(localBounds).boundingBox();

    AnnotatedRegionValue region;
    region.draggable = style()->getDraggableRegionMode() == DraggableRegionDrag;
    region.bounds = LayoutRect(absBounds);
    regions.append(region);
}

void RenderObject::collectAnnotatedRegions(Vector<AnnotatedRegionValue>& regions)
{
    // RenderTexts don't have their own style, they just use their parent's style,
    // so we don't want to include them.
    if (isText())
        return;

    addAnnotatedRegions(regions);
    for (RenderObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
        curr->collectAnnotatedRegions(regions);
}

bool RenderObject::willRenderImage(ImageResource*)
{
    // Without visibility we won't render (and therefore don't care about animation).
    if (style()->visibility() != VISIBLE)
        return false;

    // We will not render a new image when Active DOM is suspended
    if (document().activeDOMObjectsAreSuspended())
        return false;

    // If we're not in a window (i.e., we're dormant from being in a background tab)
    // then we don't want to render either.
    return document().view()->isVisible();
}

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    if (isReplaced())
        return node() ? max(1U, node()->countChildren()) : 1;
    if (isHR())
        return 1;
    return 0;
}

int RenderObject::previousOffset(int current) const
{
    return current - 1;
}

int RenderObject::previousOffsetForBackwardDeletion(int current) const
{
    return current - 1;
}

int RenderObject::nextOffset(int current) const
{
    return current + 1;
}

bool RenderObject::isInert() const
{
    const RenderObject* renderer = this;
    while (!renderer->node())
        renderer = renderer->parent();
    return renderer->node()->isInert();
}

// touch-action applies to all elements with both width AND height properties.
// According to the CSS Box Model Spec (http://dev.w3.org/csswg/css-box/#the-width-and-height-properties)
// width applies to all elements but non-replaced inline elements, table rows, and row groups and
// height applies to all elements but non-replaced inline elements, table columns, and column groups.
bool RenderObject::supportsTouchAction() const
{
    if (isInline() && !isReplaced())
        return false;
    if (isTableRow() || isRenderTableCol())
        return false;

    return true;
}

void RenderObject::imageChanged(ImageResource* image, const IntRect* rect)
{
    imageChanged(static_cast<WrappedImagePtr>(image), rect);
}

Element* RenderObject::offsetParent() const
{
    if (isDocumentElement() || isBody())
        return 0;

    if (isOutOfFlowPositioned() && style()->position() == FixedPosition)
        return 0;

    // If A is an area HTML element which has a map HTML element somewhere in the ancestor
    // chain return the nearest ancestor map HTML element and stop this algorithm.
    // FIXME: Implement!

    float effectiveZoom = style()->effectiveZoom();
    Node* node = 0;
    for (RenderObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        // Spec: http://www.w3.org/TR/cssom-view/#offset-attributes

        node = ancestor->node();

        if (!node)
            continue;

        if (ancestor->isPositioned())
            break;

        if (isHTMLBodyElement(*node))
            break;

        if (!isPositioned() && (isHTMLTableElement(*node) || isHTMLTableCellElement(*node)))
            break;

        // Webkit specific extension where offsetParent stops at zoom level changes.
        if (effectiveZoom != ancestor->style()->effectiveZoom())
            break;
    }

    return node && node->isElementNode() ? toElement(node) : 0;
}

PositionWithAffinity RenderObject::createPositionWithAffinity(int offset, EAffinity affinity)
{
    // If this is a non-anonymous renderer in an editable area, then it's simple.
    if (Node* node = nonPseudoNode()) {
        if (!node->rendererIsEditable()) {
            // If it can be found, we prefer a visually equivalent position that is editable.
            Position position = createLegacyEditingPosition(node, offset);
            Position candidate = position.downstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->rendererIsEditable())
                return PositionWithAffinity(candidate, affinity);
            candidate = position.upstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->rendererIsEditable())
                return PositionWithAffinity(candidate, affinity);
        }
        // FIXME: Eliminate legacy editing positions
        return PositionWithAffinity(createLegacyEditingPosition(node, offset), affinity);
    }

    // We don't want to cross the boundary between editable and non-editable
    // regions of the document, but that is either impossible or at least
    // extremely unlikely in any normal case because we stop as soon as we
    // find a single non-anonymous renderer.

    // Find a nearby non-anonymous renderer.
    RenderObject* child = this;
    while (RenderObject* parent = child->parent()) {
        // Find non-anonymous content after.
        RenderObject* renderer = child;
        while ((renderer = renderer->nextInPreOrder(parent))) {
            if (Node* node = renderer->nonPseudoNode())
                return PositionWithAffinity(firstPositionInOrBeforeNode(node), DOWNSTREAM);
        }

        // Find non-anonymous content before.
        renderer = child;
        while ((renderer = renderer->previousInPreOrder())) {
            if (renderer == parent)
                break;
            if (Node* node = renderer->nonPseudoNode())
                return PositionWithAffinity(lastPositionInOrAfterNode(node), DOWNSTREAM);
        }

        // Use the parent itself unless it too is anonymous.
        if (Node* node = parent->nonPseudoNode())
            return PositionWithAffinity(firstPositionInOrBeforeNode(node), DOWNSTREAM);

        // Repeat at the next level up.
        child = parent;
    }

    // Everything was anonymous. Give up.
    return PositionWithAffinity();
}

PositionWithAffinity RenderObject::createPositionWithAffinity(const Position& position)
{
    if (position.isNotNull())
        return PositionWithAffinity(position);

    ASSERT(!node());
    return createPositionWithAffinity(0, DOWNSTREAM);
}

CursorDirective RenderObject::getCursor(const LayoutPoint&, Cursor&) const
{
    return SetCursorBasedOnStyle;
}

bool RenderObject::canUpdateSelectionOnRootLineBoxes()
{
    if (needsLayout())
        return false;

    RenderBlock* containingBlock = this->containingBlock();
    return containingBlock ? !containingBlock->needsLayout() : true;
}

// We only create "generated" child renderers like one for first-letter if:
// - the firstLetterBlock can have children in the DOM and
// - the block doesn't have any special assumption on its text children.
// This correctly prevents form controls from having such renderers.
bool RenderObject::canHaveGeneratedChildren() const
{
    return canHaveChildren();
}

void RenderObject::setNeedsBoundariesUpdate()
{
    if (RenderObject* renderer = parent())
        renderer->setNeedsBoundariesUpdate();
}

FloatRect RenderObject::objectBoundingBox() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

FloatRect RenderObject::strokeBoundingBox() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

// Returns the smallest rectangle enclosing all of the painted content
// respecting clipping, masking, filters, opacity, stroke-width and markers
FloatRect RenderObject::paintInvalidationRectInLocalCoordinates() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

AffineTransform RenderObject::localTransform() const
{
    static const AffineTransform identity;
    return identity;
}

const AffineTransform& RenderObject::localToParentTransform() const
{
    static const AffineTransform identity;
    return identity;
}

bool RenderObject::nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderObject::isRelayoutBoundaryForInspector() const
{
    return objectIsRelayoutBoundary(this);
}

void RenderObject::clearPaintInvalidationState()
{
    setShouldDoFullPaintInvalidationAfterLayout(false);
    setShouldDoFullPaintInvalidationIfSelfPaintingLayer(false);
    setOnlyNeededPositionedMovementLayout(false);
    setShouldInvalidateOverflowForPaint(false);
    setLayoutDidGetCalled(false);
    setMayNeedPaintInvalidation(false);
}

bool RenderObject::isAllowedToModifyRenderTreeStructure(Document& document)
{
    return DeprecatedDisableModifyRenderTreeStructureAsserts::canModifyRenderTreeStateInAnyState()
        || document.lifecycle().stateAllowsRenderTreeMutations();
}

DeprecatedDisableModifyRenderTreeStructureAsserts::DeprecatedDisableModifyRenderTreeStructureAsserts()
    : m_disabler(gModifyRenderTreeStructureAnyState, true)
{
}

bool DeprecatedDisableModifyRenderTreeStructureAsserts::canModifyRenderTreeStateInAnyState()
{
    return gModifyRenderTreeStructureAnyState;
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::RenderObject* object)
{
    if (object)
        object->showTreeForThis();
}

void showLineTree(const WebCore::RenderObject* object)
{
    if (object)
        object->showLineTreeForThis();
}

void showRenderTree(const WebCore::RenderObject* object1)
{
    showRenderTree(object1, 0);
}

void showRenderTree(const WebCore::RenderObject* object1, const WebCore::RenderObject* object2)
{
    if (object1) {
        const WebCore::RenderObject* root = object1;
        while (root->parent())
            root = root->parent();
        root->showRenderTreeAndMark(object1, "*", object2, "-", 0);
    }
}

#endif
