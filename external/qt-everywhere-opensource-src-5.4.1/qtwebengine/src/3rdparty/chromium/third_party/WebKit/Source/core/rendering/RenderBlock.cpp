/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "config.h"
#include "core/rendering/RenderBlock.h"

#include "core/HTMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/events/OverflowEvent.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/fetch/ResourceLoadPriorityOptimizer.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/rendering/FastTextAutosizer.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/HitTestLocation.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/LayoutRepainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/RenderDeprecatedFlexibleBox.h"
#include "core/rendering/RenderFlexibleBox.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGrid.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderMarquee.h"
#include "core/rendering/RenderRegion.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTextControl.h"
#include "core/rendering/RenderTextFragment.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/shapes/ShapeOutsideInfo.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsContextCullSaver.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsRenderBlock : public RenderBox {
    void* pointers[1];
    RenderObjectChildList children;
    RenderLineBoxList lineBoxes;
    uint32_t bitfields;
};

struct SameSizeAsRenderBlockRareData {
    int paginationStrut;
    int pageLogicalOffset;
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(RenderBlock) == sizeof(SameSizeAsRenderBlock), RenderBlock_should_stay_small);
COMPILE_ASSERT(sizeof(RenderBlock::RenderBlockRareData) == sizeof(SameSizeAsRenderBlockRareData), RenderBlockRareData_should_stay_small);

typedef WTF::HashMap<const RenderBox*, OwnPtr<ColumnInfo> > ColumnInfoMap;
static ColumnInfoMap* gColumnInfoMap = 0;

static TrackedDescendantsMap* gPositionedDescendantsMap = 0;
static TrackedDescendantsMap* gPercentHeightDescendantsMap = 0;

static TrackedContainerMap* gPositionedContainerMap = 0;
static TrackedContainerMap* gPercentHeightContainerMap = 0;

typedef WTF::HashMap<RenderBlock*, OwnPtr<ListHashSet<RenderInline*> > > ContinuationOutlineTableMap;

typedef WTF::HashSet<RenderBlock*> DelayedUpdateScrollInfoSet;
static int gDelayUpdateScrollInfo = 0;
static DelayedUpdateScrollInfoSet* gDelayedUpdateScrollInfoSet = 0;

static bool gColumnFlowSplitEnabled = true;

// This class helps dispatching the 'overflow' event on layout change. overflow can be set on RenderBoxes, yet the existing code
// only works on RenderBlocks. If this changes, this class should be shared with other RenderBoxes.
class OverflowEventDispatcher {
    WTF_MAKE_NONCOPYABLE(OverflowEventDispatcher);
public:
    OverflowEventDispatcher(const RenderBlock* block)
        : m_block(block)
        , m_hadHorizontalLayoutOverflow(false)
        , m_hadVerticalLayoutOverflow(false)
    {
        m_shouldDispatchEvent = !m_block->isAnonymous() && m_block->hasOverflowClip() && m_block->document().hasListenerType(Document::OVERFLOWCHANGED_LISTENER);
        if (m_shouldDispatchEvent) {
            m_hadHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
            m_hadVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();
        }
    }

    ~OverflowEventDispatcher()
    {
        if (!m_shouldDispatchEvent)
            return;

        bool hasHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
        bool hasVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();

        bool horizontalLayoutOverflowChanged = hasHorizontalLayoutOverflow != m_hadHorizontalLayoutOverflow;
        bool verticalLayoutOverflowChanged = hasVerticalLayoutOverflow != m_hadVerticalLayoutOverflow;

        if (!horizontalLayoutOverflowChanged && !verticalLayoutOverflowChanged)
            return;

        RefPtrWillBeRawPtr<OverflowEvent> event = OverflowEvent::create(horizontalLayoutOverflowChanged, hasHorizontalLayoutOverflow, verticalLayoutOverflowChanged, hasVerticalLayoutOverflow);
        event->setTarget(m_block->node());
        m_block->document().enqueueAnimationFrameEvent(event.release());
    }

private:
    const RenderBlock* m_block;
    bool m_shouldDispatchEvent;
    bool m_hadHorizontalLayoutOverflow;
    bool m_hadVerticalLayoutOverflow;
};

RenderBlock::RenderBlock(ContainerNode* node)
    : RenderBox(node)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_hasBorderOrPaddingLogicalWidthChanged(false)
    , m_hasOnlySelfCollapsingChildren(false)
{
    setChildrenInline(true);
}

static void removeBlockFromDescendantAndContainerMaps(RenderBlock* block, TrackedDescendantsMap*& descendantMap, TrackedContainerMap*& containerMap)
{
    if (OwnPtr<TrackedRendererListHashSet> descendantSet = descendantMap->take(block)) {
        TrackedRendererListHashSet::iterator end = descendantSet->end();
        for (TrackedRendererListHashSet::iterator descendant = descendantSet->begin(); descendant != end; ++descendant) {
            TrackedContainerMap::iterator it = containerMap->find(*descendant);
            ASSERT(it != containerMap->end());
            if (it == containerMap->end())
                continue;
            HashSet<RenderBlock*>* containerSet = it->value.get();
            ASSERT(containerSet->contains(block));
            containerSet->remove(block);
            if (containerSet->isEmpty())
                containerMap->remove(it);
        }
    }
}

static void appendImageIfNotNull(Vector<ImageResource*>& imageResources, const StyleImage* styleImage)
{
    if (styleImage && styleImage->cachedImage()) {
        ImageResource* imageResource = styleImage->cachedImage();
        if (imageResource && !imageResource->isLoaded())
            imageResources.append(styleImage->cachedImage());
    }
}

static void appendLayers(Vector<ImageResource*>& images, const FillLayer* styleLayer)
{
    for (const FillLayer* layer = styleLayer; layer; layer = layer->next()) {
        appendImageIfNotNull(images, layer->image());
    }
}

static void appendImagesFromStyle(Vector<ImageResource*>& images, RenderStyle& blockStyle)
{
    appendLayers(images, blockStyle.backgroundLayers());
    appendLayers(images, blockStyle.maskLayers());

    const ContentData* contentData = blockStyle.contentData();
    if (contentData && contentData->isImage()) {
        const ImageContentData* imageContentData = static_cast<const ImageContentData*>(contentData);
        appendImageIfNotNull(images, imageContentData->image());
    }
    if (blockStyle.boxReflect())
        appendImageIfNotNull(images, blockStyle.boxReflect()->mask().image());
    appendImageIfNotNull(images, blockStyle.listStyleImage());
    appendImageIfNotNull(images, blockStyle.borderImageSource());
    appendImageIfNotNull(images, blockStyle.maskBoxImageSource());
    if (blockStyle.shapeOutside())
        appendImageIfNotNull(images, blockStyle.shapeOutside()->image());
}

RenderBlock::~RenderBlock()
{
    if (hasColumns())
        gColumnInfoMap->take(this);
    if (gPercentHeightDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
    if (gPositionedDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::willBeDestroyed()
{
    // Mark as being destroyed to avoid trouble with merges in removeChild().
    m_beingDestroyed = true;

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from. Effects that do :before/:after only on hover could crash otherwise.
    children()->destroyLeftoverChildren();

    // Destroy our continuation before anything other than anonymous children.
    // The reason we don't destroy it before anonymous children is that they may
    // have continuations of their own that are anonymous children of our continuation.
    RenderBoxModelObject* continuation = this->continuation();
    if (continuation) {
        continuation->destroy();
        setContinuation(0);
    }

    if (!documentBeingDestroyed()) {
        if (firstLineBox()) {
            // We can't wait for RenderBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            // FIXME: The FrameSelection should be responsible for this when it
            // is notified of DOM mutations.
            if (isSelectionBorder())
                view()->clearSelection();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (InlineFlowBox* box = firstLineBox(); box; box = box->nextLineBox()) {
                    while (InlineBox* childBox = box->firstChild())
                        childBox->remove();
                }
            }
        } else if (parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    m_lineBoxes.deleteLineBoxes();

    if (UNLIKELY(gDelayedUpdateScrollInfoSet != 0))
        gDelayedUpdateScrollInfoSet->remove(this);

    if (FastTextAutosizer* textAutosizer = document().fastTextAutosizer())
        textAutosizer->destroy(this);

    RenderBox::willBeDestroyed();
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    RenderStyle* oldStyle = style();

    setReplaced(newStyle.isDisplayInlineType());

    if (oldStyle && parent()) {
        bool oldStyleIsContainer = oldStyle->position() != StaticPosition || oldStyle->hasTransformRelatedProperty();
        bool newStyleIsContainer = newStyle.position() != StaticPosition || newStyle.hasTransformRelatedProperty();

        if (oldStyleIsContainer && !newStyleIsContainer) {
            // Clear our positioned objects list. Our absolutely positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(0, NewContainingBlock);
        } else if (!oldStyleIsContainer && newStyleIsContainer) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            RenderObject* cb = parent();
            while (cb && (cb->style()->position() == StaticPosition || (cb->isInline() && !cb->isReplaced())) && !cb->isRenderView()) {
                if (cb->style()->position() == RelativePosition && cb->isInline() && !cb->isReplaced()) {
                    cb = cb->containingBlock();
                    break;
                }
                cb = cb->parent();
            }

            if (cb->isRenderBlock())
                toRenderBlock(cb)->removePositionedObjects(this, NewContainingBlock);
        }
    }

    RenderBox::styleWillChange(diff, newStyle);
}

static bool borderOrPaddingLogicalWidthChanged(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (newStyle->isHorizontalWritingMode())
        return oldStyle->borderLeftWidth() != newStyle->borderLeftWidth()
            || oldStyle->borderRightWidth() != newStyle->borderRightWidth()
            || oldStyle->paddingLeft() != newStyle->paddingLeft()
            || oldStyle->paddingRight() != newStyle->paddingRight();

    return oldStyle->borderTopWidth() != newStyle->borderTopWidth()
        || oldStyle->borderBottomWidth() != newStyle->borderBottomWidth()
        || oldStyle->paddingTop() != newStyle->paddingTop()
        || oldStyle->paddingBottom() != newStyle->paddingBottom();
}

void RenderBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);

    RenderStyle* newStyle = style();

    if (!isAnonymousBlock()) {
        // Ensure that all of our continuation blocks pick up the new style.
        for (RenderBlock* currCont = blockElementContinuation(); currCont; currCont = currCont->blockElementContinuation()) {
            RenderBoxModelObject* nextCont = currCont->continuation();
            currCont->setContinuation(0);
            currCont->setStyle(newStyle);
            currCont->setContinuation(nextCont);
        }
    }

    if (FastTextAutosizer* textAutosizer = document().fastTextAutosizer())
        textAutosizer->record(this);

    propagateStyleToAnonymousChildren(true);

    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    m_hasBorderOrPaddingLogicalWidthChanged = oldStyle && diff.needsFullLayout() && needsLayout() && borderOrPaddingLogicalWidthChanged(oldStyle, newStyle);

    // If the style has unloaded images, want to notify the ResourceLoadPriorityOptimizer so that
    // network priorities can be set.
    Vector<ImageResource*> images;
    appendImagesFromStyle(images, *newStyle);
    if (images.isEmpty())
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->removeRenderObject(this);
    else
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->addRenderObject(this);
}

void RenderBlock::invalidateTreeAfterLayout(const RenderLayerModelObject& invalidationContainer)
{
    // Note, we don't want to early out here using shouldCheckForInvalidationAfterLayout as
    // we have to make sure we go through any positioned objects as they won't be seen in
    // the normal tree walk.

    if (shouldCheckForPaintInvalidationAfterLayout())
        RenderBox::invalidateTreeAfterLayout(invalidationContainer);

    // Take care of positioned objects. This is required as LayoutState keeps a single clip rect.
    if (TrackedRendererListHashSet* positionedObjects = this->positionedObjects()) {
        TrackedRendererListHashSet::iterator end = positionedObjects->end();
        LayoutState state(*this, isTableRow() ? LayoutSize() : locationOffset());
        for (TrackedRendererListHashSet::iterator it = positionedObjects->begin(); it != end; ++it) {
            RenderBox* box = *it;

            // One of the renderers we're skipping over here may be the child's repaint container,
            // so we can't pass our own repaint container along.
            const RenderLayerModelObject& repaintContainerForChild = *box->containerForPaintInvalidation();

            // If the positioned renderer is absolutely positioned and it is inside
            // a relatively positioend inline element, we need to account for
            // the inline elements position in LayoutState.
            if (box->style()->position() == AbsolutePosition) {
                RenderObject* container = box->container(&repaintContainerForChild, 0);
                if (container->isInFlowPositioned() && container->isRenderInline()) {
                    // FIXME: We should be able to use layout-state for this.
                    // Currently, we will place absolutly positioned elements inside
                    // relatively positioned inline blocks in the wrong location. crbug.com/371485
                    ForceHorriblySlowRectMapping slowRectMapping(*this);
                    box->invalidateTreeAfterLayout(repaintContainerForChild);
                    continue;
                }
            }

            box->invalidateTreeAfterLayout(repaintContainerForChild);
        }
    }
}

RenderBlock* RenderBlock::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    RenderBlock* curr = toRenderBlock(continuation());
    RenderBlock* nextToLast = this;
    RenderBlock* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }

        nextToLast = last;
        last = curr;
        curr = toRenderBlock(curr->continuation());
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderBlock::addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlock* flow = continuationBefore(beforeChild);
    ASSERT(!beforeChild || beforeChild->parent()->isAnonymousColumnSpanBlock() || beforeChild->parent()->isRenderBlock());
    RenderBoxModelObject* beforeChildParent = 0;
    if (beforeChild)
        beforeChildParent = toRenderBoxModelObject(beforeChild->parent());
    else {
        RenderBoxModelObject* cont = flow->continuation();
        if (cont)
            beforeChildParent = cont;
        else
            beforeChildParent = flow;
    }

    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

    // A continuation always consists of two potential candidates: a block or an anonymous
    // column span box holding column span children.
    bool childIsNormal = newChild->isInline() || !newChild->style()->columnSpan();
    bool bcpIsNormal = beforeChildParent->isInline() || !beforeChildParent->style()->columnSpan();
    bool flowIsNormal = flow->isInline() || !flow->style()->columnSpan();

    if (flow == beforeChildParent) {
        flow->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childIsNormal == bcpIsNormal) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }
    if (flowIsNormal == childIsNormal) {
        flow->addChildIgnoringContinuation(newChild, 0); // Just treat like an append.
        return;
    }
    beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
}


void RenderBlock::addChildToAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild)
{
    ASSERT(!continuation()); // We don't yet support column spans that aren't immediate children of the multi-column block.

    // The goal is to locate a suitable box in which to place our child.
    RenderBlock* beforeChildParent = 0;
    if (beforeChild) {
        RenderObject* curr = beforeChild;
        while (curr && curr->parent() != this)
            curr = curr->parent();
        beforeChildParent = toRenderBlock(curr);
        ASSERT(beforeChildParent);
        ASSERT(beforeChildParent->isAnonymousColumnsBlock() || beforeChildParent->isAnonymousColumnSpanBlock());
    } else
        beforeChildParent = toRenderBlock(lastChild());

    // If the new child is floating or positioned it can just go in that block.
    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    // See if the child can be placed in the box.
    bool newChildHasColumnSpan = newChild->style()->columnSpan() && !newChild->isInline();
    bool beforeChildParentHoldsColumnSpans = beforeChildParent->isAnonymousColumnSpanBlock();

    if (newChildHasColumnSpan == beforeChildParentHoldsColumnSpans) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    if (!beforeChild) {
        // Create a new block of the correct type.
        RenderBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
        children()->appendChildNode(this, newBox);
        newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
        return;
    }

    RenderObject* immediateChild = beforeChild;
    bool isPreviousBlockViable = true;
    while (immediateChild->parent() != this) {
        if (isPreviousBlockViable)
            isPreviousBlockViable = !immediateChild->previousSibling();
        immediateChild = immediateChild->parent();
    }
    if (isPreviousBlockViable && immediateChild->previousSibling()) {
        toRenderBlock(immediateChild->previousSibling())->addChildIgnoringAnonymousColumnBlocks(newChild, 0); // Treat like an append.
        return;
    }

    // Split our anonymous blocks.
    RenderObject* newBeforeChild = splitAnonymousBoxesAroundChild(beforeChild);


    // Create a new anonymous box of the appropriate type.
    RenderBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
    children()->insertChildNode(this, newBox, newBeforeChild);
    newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
    return;
}

RenderBlockFlow* RenderBlock::containingColumnsBlock(bool allowAnonymousColumnBlock)
{
    RenderBlock* firstChildIgnoringAnonymousWrappers = 0;
    for (RenderObject* curr = this; curr; curr = curr->parent()) {
        if (!curr->isRenderBlock() || curr->isFloatingOrOutOfFlowPositioned() || curr->isTableCell() || curr->isDocumentElement() || curr->isRenderView() || curr->hasOverflowClip()
            || curr->isInlineBlockOrInlineTable())
            return 0;

        // FIXME: Renderers that do special management of their children (tables, buttons,
        // lists, flexboxes, etc.) breaks when the flow is split through them. Disabling
        // multi-column for them to avoid this problem.)
        if (!curr->isRenderBlockFlow() || curr->isListItem())
            return 0;

        RenderBlockFlow* currBlock = toRenderBlockFlow(curr);
        if (!currBlock->createsAnonymousWrapper())
            firstChildIgnoringAnonymousWrappers = currBlock;

        if (currBlock->style()->specifiesColumns() && (allowAnonymousColumnBlock || !currBlock->isAnonymousColumnsBlock()))
            return toRenderBlockFlow(firstChildIgnoringAnonymousWrappers);

        if (currBlock->isAnonymousColumnSpanBlock())
            return 0;
    }
    return 0;
}

RenderBlock* RenderBlock::clone() const
{
    RenderBlock* cloneBlock;
    if (isAnonymousBlock()) {
        cloneBlock = createAnonymousBlock();
        cloneBlock->setChildrenInline(childrenInline());
    }
    else {
        RenderObject* cloneRenderer = toElement(node())->createRenderer(style());
        cloneBlock = toRenderBlock(cloneRenderer);
        cloneBlock->setStyle(style());

        // This takes care of setting the right value of childrenInline in case
        // generated content is added to cloneBlock and 'this' does not have
        // generated content added yet.
        cloneBlock->setChildrenInline(cloneBlock->firstChild() ? cloneBlock->firstChild()->isInline() : childrenInline());
    }
    cloneBlock->setFlowThreadState(flowThreadState());
    return cloneBlock;
}

void RenderBlock::splitBlocks(RenderBlock* fromBlock, RenderBlock* toBlock,
                              RenderBlock* middleBlock,
                              RenderObject* beforeChild, RenderBoxModelObject* oldCont)
{
    // Create a clone of this inline.
    RenderBlock* cloneBlock = clone();
    if (!isAnonymousBlock())
        cloneBlock->setContinuation(oldCont);

    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    // If we are moving inline children from |this| to cloneBlock, then we need
    // to clear our line box tree.
    if (beforeChild && childrenInline())
        deleteLineBoxTree();

    // Now take all of the children from beforeChild to the end and remove
    // them from |this| and place them in the clone.
    moveChildrenTo(cloneBlock, beforeChild, 0, true);

    // Hook |clone| up as the continuation of the middle block.
    if (!cloneBlock->isAnonymousBlock())
        middleBlock->setContinuation(cloneBlock);

    // We have been reparented and are now under the fromBlock.  We need
    // to walk up our block parent chain until we hit the containing anonymous columns block.
    // Once we hit the anonymous columns block we're done.
    RenderBoxModelObject* curr = toRenderBoxModelObject(parent());
    RenderBoxModelObject* currChild = this;
    RenderObject* currChildNextSibling = currChild->nextSibling();

    while (curr && curr->isDescendantOf(fromBlock) && curr != fromBlock) {
        ASSERT_WITH_SECURITY_IMPLICATION(curr->isRenderBlock());

        RenderBlock* blockCurr = toRenderBlock(curr);

        // Create a new clone.
        RenderBlock* cloneChild = cloneBlock;
        cloneBlock = blockCurr->clone();

        // Insert our child clone as the first child.
        cloneBlock->addChildIgnoringContinuation(cloneChild, 0);

        // Hook the clone up as a continuation of |curr|.  Note we do encounter
        // anonymous blocks possibly as we walk up the block chain.  When we split an
        // anonymous block, there's no need to do any continuation hookup, since we haven't
        // actually split a real element.
        if (!blockCurr->isAnonymousBlock()) {
            oldCont = blockCurr->continuation();
            blockCurr->setContinuation(cloneBlock);
            cloneBlock->setContinuation(oldCont);
        }

        // Now we need to take all of the children starting from the first child
        // *after* currChild and append them all to the clone.
        blockCurr->moveChildrenTo(cloneBlock, currChildNextSibling, 0, true);

        // Keep walking up the chain.
        currChild = curr;
        currChildNextSibling = currChild->nextSibling();
        curr = toRenderBoxModelObject(curr->parent());
    }

    // Now we are at the columns block level. We need to put the clone into the toBlock.
    toBlock->children()->appendChildNode(toBlock, cloneBlock);

    // Now take all the children after currChild and remove them from the fromBlock
    // and put them in the toBlock.
    fromBlock->moveChildrenTo(toBlock, currChildNextSibling, 0, true);
}

void RenderBlock::splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                            RenderObject* newChild, RenderBoxModelObject* oldCont)
{
    RenderBlock* pre = 0;
    RenderBlock* block = containingColumnsBlock();

    // Delete our line boxes before we do the inline split into continuations.
    block->deleteLineBoxTree();

    bool madeNewBeforeBlock = false;
    if (block->isAnonymousColumnsBlock()) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        pre->removePositionedObjects(0);
        if (block->isRenderBlockFlow())
            toRenderBlockFlow(pre)->removeFloatingObjects();
        block = toRenderBlock(block->parent());
    } else {
        // No anonymous block available for use.  Make one.
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(false);
        madeNewBeforeBlock = true;
    }

    RenderBlock* post = block->createAnonymousColumnsBlock();
    post->setChildrenInline(false);

    RenderObject* boxFirst = madeNewBeforeBlock ? block->firstChild() : pre->nextSibling();
    if (madeNewBeforeBlock)
        block->children()->insertChildNode(block, pre, boxFirst);
    block->children()->insertChildNode(block, newBlockBox, boxFirst);
    block->children()->insertChildNode(block, post, boxFirst);
    block->setChildrenInline(false);

    if (madeNewBeforeBlock)
        block->moveChildrenTo(pre, boxFirst, 0, true);

    splitBlocks(pre, post, newBlockBox, beforeChild, oldCont);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moves from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    pre->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    block->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    post->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
}

void RenderBlock::makeChildrenAnonymousColumnBlocks(RenderObject* beforeChild, RenderBlockFlow* newBlockBox, RenderObject* newChild)
{
    RenderBlockFlow* pre = 0;
    RenderBlockFlow* post = 0;
    RenderBlock* block = this; // Eventually block will not just be |this|, but will also be a block nested inside |this|.  Assign to a variable
                               // so that we don't have to patch all of the rest of the code later on.

    // Delete the block's line boxes before we do the split.
    block->deleteLineBoxTree();

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    if (beforeChild != firstChild()) {
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(block->childrenInline());
    }

    if (beforeChild) {
        post = block->createAnonymousColumnsBlock();
        post->setChildrenInline(block->childrenInline());
    }

    RenderObject* boxFirst = block->firstChild();
    if (pre)
        block->children()->insertChildNode(block, pre, boxFirst);
    block->children()->insertChildNode(block, newBlockBox, boxFirst);
    if (post)
        block->children()->insertChildNode(block, post, boxFirst);
    block->setChildrenInline(false);

    // The pre/post blocks always have layers, so we know to always do a full insert/remove (so we pass true as the last argument).
    block->moveChildrenTo(pre, boxFirst, beforeChild, true);
    block->moveChildrenTo(post, beforeChild, 0, true);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moved from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    if (pre)
        pre->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    block->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    if (post)
        post->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
}

RenderBlockFlow* RenderBlock::columnsBlockForSpanningElement(RenderObject* newChild)
{
    // FIXME: This function is the gateway for the addition of column-span support.  It will
    // be added to in three stages:
    // (1) Immediate children of a multi-column block can span.
    // (2) Nested block-level children with only block-level ancestors between them and the multi-column block can span.
    // (3) Nested children with block or inline ancestors between them and the multi-column block can span (this is when we
    // cross the streams and have to cope with both types of continuations mixed together).
    // This function currently supports (1) and (2).
    RenderBlockFlow* columnsBlockAncestor = 0;
    if (!newChild->isText() && newChild->style()->columnSpan() && !newChild->isBeforeOrAfterContent()
        && !newChild->isFloatingOrOutOfFlowPositioned() && !newChild->isInline() && !isAnonymousColumnSpanBlock()) {
        columnsBlockAncestor = containingColumnsBlock(false);
        if (columnsBlockAncestor) {
            // Make sure that none of the parent ancestors have a continuation.
            // If yes, we do not want split the block into continuations.
            RenderObject* curr = this;
            while (curr && curr != columnsBlockAncestor) {
                if (curr->isRenderBlock() && toRenderBlock(curr)->continuation()) {
                    columnsBlockAncestor = 0;
                    break;
                }
                curr = curr->parent();
            }
        }
    }
    return columnsBlockAncestor;
}

void RenderBlock::addChildIgnoringAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != this) {
        RenderObject* beforeChildContainer = beforeChild->parent();
        while (beforeChildContainer->parent() != this)
            beforeChildContainer = beforeChildContainer->parent();
        ASSERT(beforeChildContainer);

        if (beforeChildContainer->isAnonymous()) {
            // If the requested beforeChild is not one of our children, then this is because
            // there is an anonymous container within this object that contains the beforeChild.
            RenderObject* beforeChildAnonymousContainer = beforeChildContainer;
            if (beforeChildAnonymousContainer->isAnonymousBlock()
                // Full screen renderers and full screen placeholders act as anonymous blocks, not tables:
                || beforeChildAnonymousContainer->isRenderFullScreen()
                || beforeChildAnonymousContainer->isRenderFullScreenPlaceholder()
                ) {
                // Insert the child into the anonymous block box instead of here.
                if (newChild->isInline() || newChild->isFloatingOrOutOfFlowPositioned() || beforeChild->parent()->slowFirstChild() != beforeChild)
                    beforeChild->parent()->addChild(newChild, beforeChild);
                else
                    addChild(newChild, beforeChild->parent());
                return;
            }

            ASSERT(beforeChildAnonymousContainer->isTable());
            if (newChild->isTablePart()) {
                // Insert into the anonymous table.
                beforeChildAnonymousContainer->addChild(newChild, beforeChild);
                return;
            }

            beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

            ASSERT(beforeChild->parent() == this);
            if (beforeChild->parent() != this) {
                // We should never reach here. If we do, we need to use the
                // safe fallback to use the topmost beforeChild container.
                beforeChild = beforeChildContainer;
            }
        }
    }

    // Check for a spanning element in columns.
    if (gColumnFlowSplitEnabled && !document().regionBasedColumnsEnabled()) {
        RenderBlockFlow* columnsBlockAncestor = columnsBlockForSpanningElement(newChild);
        if (columnsBlockAncestor) {
            TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);
            // We are placing a column-span element inside a block.
            RenderBlockFlow* newBox = createAnonymousColumnSpanBlock();

            if (columnsBlockAncestor != this && !isRenderFlowThread()) {
                // We are nested inside a multi-column element and are being split by the span. We have to break up
                // our block into continuations.
                RenderBoxModelObject* oldContinuation = continuation();

                // When we split an anonymous block, there's no need to do any continuation hookup,
                // since we haven't actually split a real element.
                if (!isAnonymousBlock())
                    setContinuation(newBox);

                splitFlow(beforeChild, newBox, newChild, oldContinuation);
                return;
            }

            // We have to perform a split of this block's children. This involves creating an anonymous block box to hold
            // the column-spanning |newChild|. We take all of the children from before |newChild| and put them into
            // one anonymous columns block, and all of the children after |newChild| go into another anonymous block.
            makeChildrenAnonymousColumnBlocks(beforeChild, newBox, newChild);
            return;
        }
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (childrenInline() && !newChild->isInline() && !newChild->isFloatingOrOutOfFlowPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == this);
        }
    } else if (!childrenInline() && (newChild->isFloatingOrOutOfFlowPositioned() || newChild->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            afterChild->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            RenderBlock* newBox = createAnonymousBlock();
            RenderBox::addChild(newBox, beforeChild);
            newBox->addChild(newChild);
            return;
        }
    }

    RenderBox::addChild(newChild, beforeChild);

    if (madeBoxesNonInline && parent() && isAnonymousBlock() && parent()->isRenderBlock())
        toRenderBlock(parent())->removeLeftoverAnonymousBlock(this);
    // this object may be dead here
}

void RenderBlock::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (continuation() && !isAnonymousBlock())
        addChildToContinuation(newChild, beforeChild);
    else
        addChildIgnoringContinuation(newChild, beforeChild);
}

void RenderBlock::addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!isAnonymousBlock() && firstChild() && (firstChild()->isAnonymousColumnsBlock() || firstChild()->isAnonymousColumnSpanBlock()))
        addChildToAnonymousColumnBlocks(newChild, beforeChild);
    else
        addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
}

static void getInlineRun(RenderObject* start, RenderObject* boundary,
                         RenderObject*& inlineRunStart,
                         RenderObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.

    // Start by skipping as many non-inlines as we can.
    RenderObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()))
            curr = curr->nextSibling();

        inlineRunStart = inlineRunEnd = curr;

        if (!curr)
            return; // No more inline children to be found.

        sawInline = curr->isInline();

        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void RenderBlock::deleteLineBoxTree()
{
    m_lineBoxes.deleteLineBoxTree();

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->recomputeIsIgnored(this);
}

void RenderBlock::makeChildrenNonInline(RenderObject *insertionPoint)
{
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(isInlineBlockOrInlineTable() || !isInline());
    ASSERT(!insertionPoint || insertionPoint->parent() == this);

    setChildrenInline(false);

    RenderObject *child = firstChild();
    if (!child)
        return;

    deleteLineBoxTree();

    while (child) {
        RenderObject *inlineRunStart, *inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        RenderBlock* block = createAnonymousBlock();
        children()->insertChildNode(this, block, inlineRunStart);
        moveChildrenTo(block, inlineRunStart, child);
    }

#ifndef NDEBUG
    for (RenderObject *c = firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif

    paintInvalidationForWholeRenderer();
}

void RenderBlock::removeLeftoverAnonymousBlock(RenderBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());

    if (child->continuation() || (child->firstChild() && (child->isAnonymousColumnSpanBlock() || child->isAnonymousColumnsBlock())))
        return;

    RenderObject* firstAnChild = child->m_children.firstChild();
    RenderObject* lastAnChild = child->m_children.lastChild();
    if (firstAnChild) {
        RenderObject* o = firstAnChild;
        while (o) {
            o->setParent(this);
            o = o->nextSibling();
        }
        firstAnChild->setPreviousSibling(child->previousSibling());
        lastAnChild->setNextSibling(child->nextSibling());
        if (child->previousSibling())
            child->previousSibling()->setNextSibling(firstAnChild);
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(lastAnChild);

        if (child == m_children.firstChild())
            m_children.setFirstChild(firstAnChild);
        if (child == m_children.lastChild())
            m_children.setLastChild(lastAnChild);
    } else {
        if (child == m_children.firstChild())
            m_children.setFirstChild(child->nextSibling());
        if (child == m_children.lastChild())
            m_children.setLastChild(child->previousSibling());

        if (child->previousSibling())
            child->previousSibling()->setNextSibling(child->nextSibling());
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(child->previousSibling());
    }

    child->children()->setFirstChild(0);
    child->m_next = 0;

    // Remove all the information in the flow thread associated with the leftover anonymous block.
    child->removeFromRenderFlowThread();

    // RenderGrid keeps track of its children, we must notify it about changes in the tree.
    if (child->parent()->isRenderGrid())
        toRenderGrid(child->parent())->dirtyGrid();

    child->setParent(0);
    child->setPreviousSibling(0);
    child->setNextSibling(0);

    child->destroy();
}

static bool canMergeContiguousAnonymousBlocks(RenderObject* oldChild, RenderObject* prev, RenderObject* next)
{
    if (oldChild->documentBeingDestroyed() || oldChild->isInline() || oldChild->virtualContinuation())
        return false;

    if ((prev && (!prev->isAnonymousBlock() || toRenderBlock(prev)->continuation() || toRenderBlock(prev)->beingDestroyed()))
        || (next && (!next->isAnonymousBlock() || toRenderBlock(next)->continuation() || toRenderBlock(next)->beingDestroyed())))
        return false;

    if ((prev && (prev->isRubyRun() || prev->isRubyBase()))
        || (next && (next->isRubyRun() || next->isRubyBase())))
        return false;

    if (!prev || !next)
        return true;

    // Make sure the types of the anonymous blocks match up.
    return prev->isAnonymousColumnsBlock() == next->isAnonymousColumnsBlock()
           && prev->isAnonymousColumnSpanBlock() == next->isAnonymousColumnSpanBlock();
}

void RenderBlock::collapseAnonymousBlockChild(RenderBlock* parent, RenderBlock* child)
{
    // It's possible that this block's destruction may have been triggered by the
    // child's removal. Just bail if the anonymous child block is already being
    // destroyed. See crbug.com/282088
    if (child->beingDestroyed())
        return;
    parent->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    parent->setChildrenInline(child->childrenInline());
    RenderObject* nextSibling = child->nextSibling();

    RenderFlowThread* childFlowThread = child->flowThreadContainingBlock();
    CurrentRenderFlowThreadMaintainer flowThreadMaintainer(childFlowThread);

    parent->children()->removeChildNode(parent, child, child->hasLayer());
    child->moveAllChildrenTo(parent, nextSibling, child->hasLayer());
    // Explicitly delete the child's line box tree, or the special anonymous
    // block handling in willBeDestroyed will cause problems.
    child->deleteLineBoxTree();
    child->destroy();
}

void RenderBlock::removeChild(RenderObject* oldChild)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (documentBeingDestroyed()) {
        RenderBox::removeChild(oldChild);
        return;
    }

    // This protects against column split flows when anonymous blocks are getting merged.
    TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);

    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild->previousSibling();
    RenderObject* next = oldChild->nextSibling();
    bool canMergeAnonymousBlocks = canMergeContiguousAnonymousBlocks(oldChild, prev, next);
    if (canMergeAnonymousBlocks && prev && next) {
        prev->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        RenderBlockFlow* nextBlock = toRenderBlockFlow(next);
        RenderBlockFlow* prevBlock = toRenderBlockFlow(prev);

        if (prev->childrenInline() != next->childrenInline()) {
            RenderBlock* inlineChildrenBlock = prev->childrenInline() ? prevBlock : nextBlock;
            RenderBlock* blockChildrenBlock = prev->childrenInline() ? nextBlock : prevBlock;

            // Place the inline children block inside of the block children block instead of deleting it.
            // In order to reuse it, we have to reset it to just be a generic anonymous block.  Make sure
            // to clear out inherited column properties by just making a new style, and to also clear the
            // column span flag if it is set.
            ASSERT(!inlineChildrenBlock->continuation());
            RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK);
            // Cache this value as it might get changed in setStyle() call.
            bool inlineChildrenBlockHasLayer = inlineChildrenBlock->hasLayer();
            inlineChildrenBlock->setStyle(newStyle);
            children()->removeChildNode(this, inlineChildrenBlock, inlineChildrenBlockHasLayer);

            // Now just put the inlineChildrenBlock inside the blockChildrenBlock.
            blockChildrenBlock->children()->insertChildNode(blockChildrenBlock, inlineChildrenBlock, prev == inlineChildrenBlock ? blockChildrenBlock->firstChild() : 0,
                                                            inlineChildrenBlockHasLayer || blockChildrenBlock->hasLayer());
            next->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();

            // inlineChildrenBlock got reparented to blockChildrenBlock, so it is no longer a child
            // of "this". we null out prev or next so that is not used later in the function.
            if (inlineChildrenBlock == prevBlock)
                prev = 0;
            else
                next = 0;
        } else {
            // Take all the children out of the |next| block and put them in
            // the |prev| block.
            nextBlock->moveAllChildrenIncludingFloatsTo(prevBlock, nextBlock->hasLayer() || prevBlock->hasLayer());

            // Delete the now-empty block's lines and nuke it.
            nextBlock->deleteLineBoxTree();
            nextBlock->destroy();
            next = 0;
        }
    }

    RenderBox::removeChild(oldChild);

    RenderObject* child = prev ? prev : next;
    if (canMergeAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling() && canCollapseAnonymousBlockChild()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        collapseAnonymousBlockChild(this, toRenderBlock(child));
    } else if (((prev && prev->isAnonymousBlock()) || (next && next->isAnonymousBlock())) && canCollapseAnonymousBlockChild()) {
        // It's possible that the removal has knocked us down to a single anonymous
        // block with pseudo-style element siblings (e.g. first-letter). If these
        // are floating, then we need to pull the content up also.
        RenderBlock* anonymousBlock = toRenderBlock((prev && prev->isAnonymousBlock()) ? prev : next);
        if ((anonymousBlock->previousSibling() || anonymousBlock->nextSibling())
            && (!anonymousBlock->previousSibling() || (anonymousBlock->previousSibling()->style()->styleType() != NOPSEUDO && anonymousBlock->previousSibling()->isFloating() && !anonymousBlock->previousSibling()->previousSibling()))
            && (!anonymousBlock->nextSibling() || (anonymousBlock->nextSibling()->style()->styleType() != NOPSEUDO && anonymousBlock->nextSibling()->isFloating() && !anonymousBlock->nextSibling()->nextSibling()))) {
            collapseAnonymousBlockChild(this, anonymousBlock);
        }
    }

    if (!firstChild()) {
        // If this was our last child be sure to clear out our line boxes.
        if (childrenInline())
            deleteLineBoxTree();

        // If we are an empty anonymous block in the continuation chain,
        // we need to remove ourself and fix the continuation chain.
        if (!beingDestroyed() && isAnonymousBlockContinuation() && !oldChild->isListMarker()) {
            RenderObject* containingBlockIgnoringAnonymous = containingBlock();
            while (containingBlockIgnoringAnonymous && containingBlockIgnoringAnonymous->isAnonymous())
                containingBlockIgnoringAnonymous = containingBlockIgnoringAnonymous->containingBlock();
            for (RenderObject* curr = this; curr; curr = curr->previousInPreOrder(containingBlockIgnoringAnonymous)) {
                if (curr->virtualContinuation() != this)
                    continue;

                // Found our previous continuation. We just need to point it to
                // |this|'s next continuation.
                RenderBoxModelObject* nextContinuation = continuation();
                if (curr->isRenderInline())
                    toRenderInline(curr)->setContinuation(nextContinuation);
                else if (curr->isRenderBlock())
                    toRenderBlock(curr)->setContinuation(nextContinuation);
                else
                    ASSERT_NOT_REACHED();

                break;
            }
            setContinuation(0);
            destroy();
        }
    }
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension
    // (f) establish a new block formatting context.

    // The early exit must be done before we check for clean layout.
    // We should be able to give a quick answer if the box is a relayout boundary.
    // Being a relayout boundary implies a block formatting context, and also
    // our internal layout shouldn't affect our container in any way.
    if (createsBlockFormattingContext())
        return false;

    // Placeholder elements are not laid out until the dimensions of their parent text control are known, so they
    // don't get layout until their parent has had layout - this is unique in the layout tree and means
    // when we call isSelfCollapsingBlock on them we find that they still need layout.
    ASSERT(!needsLayout() || (node() && node()->isElementNode() && toElement(node())->shadowPseudoId() == "-webkit-input-placeholder"));

    if (logicalHeight() > 0
        || isTable() || borderAndPaddingLogicalHeight()
        || style()->logicalMinHeight().isPositive()
        || style()->marginBeforeCollapse() == MSEPARATE || style()->marginAfterCollapse() == MSEPARATE)
        return false;

    Length logicalHeightLength = style()->logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.isPercent() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); !cb->isRenderView(); cb = cb->containingBlock()) {
            if (cb->style()->logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.isPercent()) && logicalHeightLength.isZero())) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !firstLineBox();

        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        if (m_hasOnlySelfCollapsingChildren)
            return true;
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isFloatingOrOutOfFlowPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void RenderBlock::startDelayUpdateScrollInfo()
{
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(!gDelayedUpdateScrollInfoSet);
        gDelayedUpdateScrollInfoSet = new DelayedUpdateScrollInfoSet;
    }
    ASSERT(gDelayedUpdateScrollInfoSet);
    ++gDelayUpdateScrollInfo;
}

void RenderBlock::finishDelayUpdateScrollInfo()
{
    --gDelayUpdateScrollInfo;
    ASSERT(gDelayUpdateScrollInfo >= 0);
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(gDelayedUpdateScrollInfoSet);

        OwnPtr<DelayedUpdateScrollInfoSet> infoSet(adoptPtr(gDelayedUpdateScrollInfoSet));
        gDelayedUpdateScrollInfoSet = 0;

        for (DelayedUpdateScrollInfoSet::iterator it = infoSet->begin(); it != infoSet->end(); ++it) {
            RenderBlock* block = *it;
            if (block->hasOverflowClip()) {
                block->layer()->scrollableArea()->updateAfterLayout();
            }
        }
    }
}

void RenderBlock::updateScrollInfoAfterLayout()
{
    if (hasOverflowClip()) {
        if (style()->isFlippedBlocksWritingMode()) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
            // Workaround for now. We cannot delay the scroll info for overflow
            // for items with opposite writing directions, as the contents needs
            // to overflow in that direction
            layer()->scrollableArea()->updateAfterLayout();
            return;
        }

        if (gDelayUpdateScrollInfo)
            gDelayedUpdateScrollInfoSet->add(this);
        else
            layer()->scrollableArea()->updateAfterLayout();
    }
}

void RenderBlock::layout()
{
    OverflowEventDispatcher dispatcher(this);

    // Update our first letter info now.
    updateFirstLetter();

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);

    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();
}

bool RenderBlock::updateImageLoadingPriorities()
{
    Vector<ImageResource*> images;
    appendImagesFromStyle(images, *style());

    if (images.isEmpty())
        return false;

    LayoutRect viewBounds = viewRect();
    LayoutRect objectBounds = absoluteContentBox();
    // The object bounds might be empty right now, so intersects will fail since it doesn't deal
    // with empty rects. Use LayoutRect::contains in that case.
    bool isVisible;
    if (!objectBounds.isEmpty())
        isVisible =  viewBounds.intersects(objectBounds);
    else
        isVisible = viewBounds.contains(objectBounds);

    ResourceLoadPriorityOptimizer::VisibilityStatus status = isVisible ?
        ResourceLoadPriorityOptimizer::Visible : ResourceLoadPriorityOptimizer::NotVisible;

    LayoutRect screenArea;
    if (!objectBounds.isEmpty()) {
        screenArea = viewBounds;
        screenArea.intersect(objectBounds);
    }

    for (Vector<ImageResource*>::iterator it = images.begin(), end = images.end(); it != end; ++it)
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->notifyImageResourceVisibility(*it, status, screenArea);

    return true;
}

void RenderBlock::computeRegionRangeForBlock(RenderFlowThread* flowThread)
{
    if (flowThread)
        flowThread->setRegionRangeForBox(this, offsetFromLogicalTopOfFirstPage());
}

bool RenderBlock::updateLogicalWidthAndColumnWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    LayoutUnit oldColumnWidth = desiredColumnWidth();

    updateLogicalWidth();
    calcColumnWidth();

    bool hasBorderOrPaddingLogicalWidthChanged = m_hasBorderOrPaddingLogicalWidthChanged;
    m_hasBorderOrPaddingLogicalWidthChanged = false;

    return oldWidth != logicalWidth() || oldColumnWidth != desiredColumnWidth() || hasBorderOrPaddingLogicalWidthChanged;
}

void RenderBlock::layoutBlock(bool)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void RenderBlock::addOverflowFromChildren()
{
    if (!hasColumns()) {
        if (childrenInline())
            toRenderBlockFlow(this)->addOverflowFromInlineChildren();
        else
            addOverflowFromBlockChildren();
    } else {
        ColumnInfo* colInfo = columnInfo();
        if (columnCount(colInfo)) {
            LayoutRect lastRect = columnRectAt(colInfo, columnCount(colInfo) - 1);
            addLayoutOverflow(lastRect);
            addContentsVisualOverflow(lastRect);
        }
    }
}

void RenderBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool)
{
    m_overflow.clear();

    // Add overflow from children.
    addOverflowFromChildren();

    // Add in the overflow from positioned objects.
    addOverflowFromPositionedObjects();

    if (hasOverflowClip()) {
        // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins
        // and bottom padding.  Set the axis we don't care about to be 1, since we want this overflow to always
        // be considered reachable.
        LayoutRect clientRect(noOverflowRect());
        LayoutRect rectToApply;
        if (isHorizontalWritingMode())
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), 1, max<LayoutUnit>(0, oldClientAfterEdge - clientRect.y()));
        else
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), max<LayoutUnit>(0, oldClientAfterEdge - clientRect.x()), 1);
        addLayoutOverflow(rectToApply);
        if (hasRenderOverflow())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }

    addVisualEffectOverflow();

    addVisualOverflowFromTheme();
}

void RenderBlock::addOverflowFromBlockChildren()
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!child->isFloatingOrOutOfFlowPositioned())
            addOverflowFromChild(child);
    }
}

void RenderBlock::addOverflowFromPositionedObjects()
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    RenderBox* positionedObject;
    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        positionedObject = *it;

        // Fixed positioned elements don't contribute to layout overflow, since they don't scroll with the content.
        if (positionedObject->style()->position() != FixedPosition)
            addOverflowFromChild(positionedObject, LayoutSize(positionedObject->x(), positionedObject->y()));
    }
}

void RenderBlock::addVisualOverflowFromTheme()
{
    if (!style()->hasAppearance())
        return;

    IntRect inflatedRect = pixelSnappedBorderBoxRect();
    RenderTheme::theme().adjustRepaintRect(this, inflatedRect);
    addVisualOverflow(inflatedRect);
}

bool RenderBlock::createsBlockFormattingContext() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasOverflowClip() || isFlexItemIncludingDeprecated()
        || style()->specifiesColumns() || isRenderFlowThread() || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot() || isDocumentElement() || style()->columnSpan();
}

void RenderBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox* child)
{
    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutChildren || (child->hasRelativeLogicalHeight() && !isRenderView()))
        child->setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren && child->needsPreferredWidthsRecalculation())
        child->setPreferredLogicalWidthsDirty(MarkOnlyThis);
}

void RenderBlock::simplifiedNormalFlowLayout()
{
    if (childrenInline()) {
        ListHashSet<RootInlineBox*> lineBoxes;
        for (InlineWalker walker(this); !walker.atEnd(); walker.advance()) {
            RenderObject* o = walker.current();
            if (!o->isOutOfFlowPositioned() && (o->isReplaced() || o->isFloating())) {
                o->layoutIfNeeded();
                if (toRenderBox(o)->inlineBoxWrapper()) {
                    RootInlineBox& box = toRenderBox(o)->inlineBoxWrapper()->root();
                    lineBoxes.add(&box);
                }
            } else if (o->isText() || (o->isRenderInline() && !walker.atEndOfInline())) {
                o->clearNeedsLayout();
            }
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;
        for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (RenderBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned())
                box->layoutIfNeeded();
        }
    }
}

bool RenderBlock::simplifiedLayout()
{
    // Check if we need to do a full layout.
    if (normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    // Check that we actually need to do a simplified layout.
    if (!posChildNeedsLayout() && !(needsSimplifiedNormalFlowLayout() || needsPositionedMovementLayout()))
        return false;


    {
        // LayoutState needs this deliberate scope to pop before repaint
        LayoutState state(*this, locationOffset());

        if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly())
            return false;

        FastTextAutosizer::LayoutScope fastTextAutosizerLayoutScope(this);

        // Lay out positioned descendants or objects that just need to recompute overflow.
        if (needsSimplifiedNormalFlowLayout())
            simplifiedNormalFlowLayout();

        // Lay out our positioned objects if our positioned child bit is set.
        // Also, if an absolute position element inside a relative positioned container moves, and the absolute element has a fixed position
        // child, neither the fixed element nor its container learn of the movement since posChildNeedsLayout() is only marked as far as the
        // relative positioned container. So if we can have fixed pos objects in our positioned objects list check if any of them
        // are statically positioned and thus need to move with their absolute ancestors.
        bool canContainFixedPosObjects = canContainFixedPositionObjects();
        if (posChildNeedsLayout() || needsPositionedMovementLayout() || canContainFixedPosObjects)
            layoutPositionedObjects(false, needsPositionedMovementLayout() ? ForcedLayoutAfterContainingBlockMoved : (!posChildNeedsLayout() && canContainFixedPosObjects ? LayoutOnlyFixedPositionedObjects : DefaultLayout));

        // Recompute our overflow information.
        // FIXME: We could do better here by computing a temporary overflow object from layoutPositionedObjects and only
        // updating our overflow if we either used to have overflow or if the new temporary object has overflow.
        // For now just always recompute overflow. This is no worse performance-wise than the old code that called rightmostPosition and
        // lowestPosition on every relayout so it's not a regression.
        // computeOverflow expects the bottom edge before we clamp our height. Since this information isn't available during
        // simplifiedLayout, we cache the value in m_overflow.
        LayoutUnit oldClientAfterEdge = hasRenderOverflow() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
        computeOverflow(oldClientAfterEdge, true);
    }

    updateLayerTransformAfterLayout();

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
    return true;
}

void RenderBlock::markFixedPositionObjectForLayoutIfNeeded(RenderObject* child, SubtreeLayoutScope& layoutScope)
{
    if (child->style()->position() != FixedPosition)
        return;

    bool hasStaticBlockPosition = child->style()->hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = child->style()->hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    RenderObject* o = child->parent();
    while (o && !o->isRenderView() && o->style()->position() != AbsolutePosition)
        o = o->parent();
    if (o->style()->position() != AbsolutePosition)
        return;

    RenderBox* box = toRenderBox(child);
    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        box->computeLogicalWidth(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != box->logicalLeft())
            layoutScope.setChildNeedsLayout(child);
    } else if (hasStaticBlockPosition) {
        LayoutUnit oldTop = box->logicalTop();
        box->updateLogicalHeight();
        if (box->logicalTop() != oldTop)
            layoutScope.setChildNeedsLayout(child);
    }
}

LayoutUnit RenderBlock::marginIntrinsicLogicalWidthForChild(RenderBox* child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child->style()->marginStartUsing(style());
    Length marginRight = child->style()->marginEndUsing(style());
    LayoutUnit margin = 0;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior info)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    if (hasColumns())
        view()->layoutState()->clearPaginationInformation(); // Positioned objects are not part of the column flow, so they don't paginate with the columns.

    RenderBox* r;
    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        r = *it;

        // FIXME: this should only be set from clearNeedsLayout crbug.com/361250
        r->setLayoutDidGetCalled(true);

        SubtreeLayoutScope layoutScope(*r);
        // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
        // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e.
        // it has static position.
        markFixedPositionObjectForLayoutIfNeeded(r, layoutScope);
        if (info == LayoutOnlyFixedPositionedObjects) {
            r->layoutIfNeeded();
            continue;
        }

        // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
        // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
        // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
        // positioned explicitly) this should not incur a performance penalty.
        if (relayoutChildren || (r->style()->hasStaticBlockPosition(isHorizontalWritingMode()) && r->parent() != this))
            layoutScope.setChildNeedsLayout(r);

        // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
        if (relayoutChildren && r->needsPreferredWidthsRecalculation())
            r->setPreferredLogicalWidthsDirty(MarkOnlyThis);

        if (!r->needsLayout())
            r->markForPaginationRelayoutIfNeeded(layoutScope);

        // If we are paginated or in a line grid, go ahead and compute a vertical position for our object now.
        // If it's wrong we'll lay out again.
        LayoutUnit oldLogicalTop = 0;
        bool needsBlockDirectionLocationSetBeforeLayout = r->needsLayout() && view()->layoutState()->needsBlockDirectionLocationSetBeforeLayout();
        if (needsBlockDirectionLocationSetBeforeLayout) {
            if (isHorizontalWritingMode() == r->isHorizontalWritingMode())
                r->updateLogicalHeight();
            else
                r->updateLogicalWidth();
            oldLogicalTop = logicalTopForChild(r);
        }

        // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout() here instead of a full layout. Need
        // to investigate why it does not trigger the correct invalidations in that case. crbug.com/350756
        if (info == ForcedLayoutAfterContainingBlockMoved)
            r->setNeedsLayoutAndFullPaintInvalidation();

        r->layoutIfNeeded();

        // Lay out again if our estimate was wrong.
        if (needsBlockDirectionLocationSetBeforeLayout && logicalTopForChild(r) != oldLogicalTop)
            r->forceChildLayout();
    }

    if (hasColumns())
        view()->layoutState()->setColumnInfo(columnInfo()); // FIXME: Kind of gross. We just put this back into the layout state so that pop() will work.
}

void RenderBlock::markPositionedObjectsForLayout()
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (positionedDescendants) {
        RenderBox* r;
        TrackedRendererListHashSet::iterator end = positionedDescendants->end();
        for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
            r = *it;
            r->setChildNeedsLayout();
        }
    }
}

void RenderBlock::markForPaginationRelayoutIfNeeded(SubtreeLayoutScope& layoutScope)
{
    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    if (view()->layoutState()->pageLogicalHeightChanged() || (view()->layoutState()->pageLogicalHeight() && view()->layoutState()->pageLogicalOffset(*this, logicalTop()) != pageLogicalOffset()))
        layoutScope.setChildNeedsLayout(this);
}

void RenderBlock::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, this);

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    PaintPhase phase = paintInfo.phase;

    LayoutRect overflowBox;
    // Check if we need to do anything at all.
    // FIXME: Could eliminate the isDocumentElement() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (!isDocumentElement()) {
        overflowBox = overflowRectForPaintRejection();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    // There are some cases where not all clipped visual overflow is accounted for.
    // FIXME: reduce the number of such cases.
    ContentsClipBehavior contentsClipBehavior = ForceContentsClip;
    if (hasOverflowClip() && !hasControlClip() && !(shouldPaintSelectionGaps() && phase == PaintPhaseForeground) && !hasCaret())
        contentsClipBehavior = SkipContentsClipIfPossible;

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset, contentsClipBehavior);
    {
        GraphicsContextCullSaver cullSaver(*paintInfo.context);
        // Cull if we have more than one child and we didn't already clip.
        bool shouldCull = document().settings()->containerCullingEnabled() && !pushedClip && !isDocumentElement()
            && firstChild() && lastChild() && firstChild() != lastChild();
        if (shouldCull)
            cullSaver.cull(overflowBox);

        paintObject(paintInfo, adjustedPaintOffset);
    }
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index.  We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (hasOverflowClip() && style()->visibility() == VISIBLE && (phase == PaintPhaseBlockBackground || phase == PaintPhaseChildBlockBackground) && paintInfo.shouldPaintWithinRoot(this) && !paintInfo.paintRootBackgroundOnly())
        layer()->scrollableArea()->paintOverflowControls(paintInfo.context, roundedIntPoint(adjustedPaintOffset), paintInfo.rect, false /* paintingOverlayControls */);
}

void RenderBlock::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context->paintingDisabled())
        return;

    const Color& ruleColor = resolveColor(CSSPropertyWebkitColumnRuleColor);
    bool ruleTransparent = style()->columnRuleIsTransparent();
    EBorderStyle ruleStyle = style()->columnRuleStyle();
    LayoutUnit ruleThickness = style()->columnRuleWidth();
    LayoutUnit colGap = columnGap();
    bool renderRule = ruleStyle > BHIDDEN && !ruleTransparent;
    if (!renderRule)
        return;

    ColumnInfo* colInfo = columnInfo();
    unsigned colCount = columnCount(colInfo);

    bool antialias = shouldAntialiasLines(paintInfo.context);

    if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
        bool leftToRight = style()->isLeftToRightDirection();
        LayoutUnit currLogicalLeftOffset = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit ruleAdd = logicalLeftOffsetForContent();
        LayoutUnit ruleLogicalLeft = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit inlineDirectionSize = colInfo->desiredColumnWidth();
        BoxSide boxSide = isHorizontalWritingMode()
            ? leftToRight ? BSLeft : BSRight
            : leftToRight ? BSTop : BSBottom;

        for (unsigned i = 0; i < colCount; i++) {
            // Move to the next position.
            if (leftToRight) {
                ruleLogicalLeft += inlineDirectionSize + colGap / 2;
                currLogicalLeftOffset += inlineDirectionSize + colGap;
            } else {
                ruleLogicalLeft -= (inlineDirectionSize + colGap / 2);
                currLogicalLeftOffset -= (inlineDirectionSize + colGap);
            }

            // Now paint the column rule.
            if (i < colCount - 1) {
                LayoutUnit ruleLeft = isHorizontalWritingMode() ? paintOffset.x() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd : paintOffset.x() + borderLeft() + paddingLeft();
                LayoutUnit ruleRight = isHorizontalWritingMode() ? ruleLeft + ruleThickness : ruleLeft + contentWidth();
                LayoutUnit ruleTop = isHorizontalWritingMode() ? paintOffset.y() + borderTop() + paddingTop() : paintOffset.y() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd;
                LayoutUnit ruleBottom = isHorizontalWritingMode() ? ruleTop + contentHeight() : ruleTop + ruleThickness;
                IntRect pixelSnappedRuleRect = pixelSnappedIntRectFromEdges(ruleLeft, ruleTop, ruleRight, ruleBottom);
                drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
            }

            ruleLogicalLeft = currLogicalLeftOffset;
        }
    } else {
        bool topToBottom = !style()->isFlippedBlocksWritingMode();
        LayoutUnit ruleLeft = isHorizontalWritingMode()
            ? borderLeft() + paddingLeft()
            : colGap / 2 - colGap - ruleThickness / 2 + borderBefore() + paddingBefore();
        LayoutUnit ruleWidth = isHorizontalWritingMode() ? contentWidth() : ruleThickness;
        LayoutUnit ruleTop = isHorizontalWritingMode()
            ? colGap / 2 - colGap - ruleThickness / 2 + borderBefore() + paddingBefore()
            : borderStart() + paddingStart();
        LayoutUnit ruleHeight = isHorizontalWritingMode() ? ruleThickness : contentHeight();
        LayoutRect ruleRect(ruleLeft, ruleTop, ruleWidth, ruleHeight);

        if (!topToBottom) {
            if (isHorizontalWritingMode())
                ruleRect.setY(height() - ruleRect.maxY());
            else
                ruleRect.setX(width() - ruleRect.maxX());
        }

        ruleRect.moveBy(paintOffset);

        BoxSide boxSide = isHorizontalWritingMode()
            ? topToBottom ? BSTop : BSBottom
            : topToBottom ? BSLeft : BSRight;

        LayoutSize step(0, topToBottom ? colInfo->columnHeight() + colGap : -(colInfo->columnHeight() + colGap));
        if (!isHorizontalWritingMode())
            step = step.transposedSize();

        for (unsigned i = 1; i < colCount; i++) {
            ruleRect.move(step);
            IntRect pixelSnappedRuleRect = pixelSnappedIntRect(ruleRect);
            drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
        }
    }
}

void RenderBlock::paintColumnContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool paintingFloats)
{
    // We need to do multiple passes, breaking up our child painting into strips.
    GraphicsContext* context = paintInfo.context;
    ColumnInfo* colInfo = columnInfo();
    unsigned colCount = columnCount(colInfo);
    if (!colCount)
        return;
    LayoutUnit currLogicalTopOffset = 0;
    LayoutUnit colGap = columnGap();
    for (unsigned i = 0; i < colCount; i++) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnRectAt(colInfo, i);
        flipForWritingMode(colRect);
        LayoutUnit logicalLeftOffset = (isHorizontalWritingMode() ? colRect.x() : colRect.y()) - logicalLeftOffsetForContent();
        LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(logicalLeftOffset, currLogicalTopOffset) : LayoutSize(currLogicalTopOffset, logicalLeftOffset);
        if (colInfo->progressionAxis() == ColumnInfo::BlockAxis) {
            if (isHorizontalWritingMode())
                offset.expand(0, colRect.y() - borderTop() - paddingTop());
            else
                offset.expand(colRect.x() - borderLeft() - paddingLeft(), 0);
        }
        colRect.moveBy(paintOffset);
        PaintInfo info(paintInfo);
        info.rect.intersect(enclosingIntRect(colRect));

        if (!info.rect.isEmpty()) {
            GraphicsContextStateSaver stateSaver(*context);
            LayoutRect clipRect(colRect);

            if (i < colCount - 1) {
                if (isHorizontalWritingMode())
                    clipRect.expand(colGap / 2, 0);
                else
                    clipRect.expand(0, colGap / 2);
            }
            // Each strip pushes a clip, since column boxes are specified as being
            // like overflow:hidden.
            // FIXME: Content and column rules that extend outside column boxes at the edges of the multi-column element
            // are clipped according to the 'overflow' property.
            context->clip(enclosingIntRect(clipRect));

            // Adjust our x and y when painting.
            LayoutPoint adjustedPaintOffset = paintOffset + offset;
            if (paintingFloats)
                paintFloats(info, adjustedPaintOffset, paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip);
            else
                paintContents(info, adjustedPaintOffset);
        }

        LayoutUnit blockDelta = (isHorizontalWritingMode() ? colRect.height() : colRect.width());
        if (style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset += blockDelta;
        else
            currLogicalTopOffset -= blockDelta;
    }
}

void RenderBlock::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Avoid painting descendants of the root element when stylesheets haven't loaded.  This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, styleResolverChanged() on the Document
    // will do a full repaint.
    if (document().didLayoutWithPendingStylesheets() && !isRenderView())
        return;

    if (childrenInline())
        m_lineBoxes.paint(this, paintInfo, paintOffset);
    else {
        PaintPhase newPhase = (paintInfo.phase == PaintPhaseChildOutlines) ? PaintPhaseOutline : paintInfo.phase;
        newPhase = (newPhase == PaintPhaseChildBlockBackgrounds) ? PaintPhaseChildBlockBackground : newPhase;

        // We don't paint our own background, but we do let the kids paint their backgrounds.
        PaintInfo paintInfoForChild(paintInfo);
        paintInfoForChild.phase = newPhase;
        paintInfoForChild.updatePaintingRootForChildren(this);
        paintChildren(paintInfoForChild, paintOffset);
    }
}

void RenderBlock::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox())
        paintChild(child, paintInfo, paintOffset);
}

void RenderBlock::paintChild(RenderBox* child, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint childPoint = flipForWritingModeForChild(child, paintOffset);
    if (!child->hasSelfPaintingLayer() && !child->isFloating())
        child->paint(paintInfo, childPoint);
}

void RenderBlock::paintChildAsInlineBlock(RenderBox* child, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint childPoint = flipForWritingModeForChild(child, paintOffset);
    if (!child->hasSelfPaintingLayer() && !child->isFloating())
        paintAsInlineBlock(child, paintInfo, childPoint);
}

void RenderBlock::paintAsInlineBlock(RenderObject* renderer, PaintInfo& paintInfo, const LayoutPoint& childPoint)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    // Paint all phases atomically, as though the element established its own
    // stacking context.  (See Appendix E.2, section 7.2.1.4 on
    // inline block/table/replaced elements in the CSS2.1 specification.)
    // This is also used by other elements (e.g. flex items and grid items).
    bool preservePhase = paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip;
    PaintInfo info(paintInfo);
    info.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
    renderer->paint(info, childPoint);
    if (!preservePhase) {
        info.phase = PaintPhaseChildBlockBackgrounds;
        renderer->paint(info, childPoint);
        info.phase = PaintPhaseFloat;
        renderer->paint(info, childPoint);
        info.phase = PaintPhaseForeground;
        renderer->paint(info, childPoint);
        info.phase = PaintPhaseOutline;
        renderer->paint(info, childPoint);
    }
}

static inline bool caretBrowsingEnabled(const Frame* frame)
{
    Settings* settings = frame->settings();
    return settings && settings->caretBrowsingEnabled();
}

static inline bool hasCursorCaret(const FrameSelection& selection, const RenderBlock* block, bool caretBrowsing)
{
    return selection.caretRenderer() == block && (selection.rendererIsEditable() || caretBrowsing);
}

static inline bool hasDragCaret(const DragCaretController& dragCaretController, const RenderBlock* block, bool caretBrowsing)
{
    return dragCaretController.caretRenderer() == block && (dragCaretController.isContentEditable() || caretBrowsing);
}

bool RenderBlock::hasCaret() const
{
    bool caretBrowsing = caretBrowsingEnabled(frame());
    return hasCursorCaret(frame()->selection(), this, caretBrowsing)
        || hasDragCaret(frame()->page()->dragCaretController(), this, caretBrowsing);
}

void RenderBlock::paintCarets(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    bool caretBrowsing = caretBrowsingEnabled(frame());

    FrameSelection& selection = frame()->selection();
    if (hasCursorCaret(selection, this, caretBrowsing)) {
        selection.paintCaret(paintInfo.context, paintOffset, paintInfo.rect);
    }

    DragCaretController& dragCaretController = frame()->page()->dragCaretController();
    if (hasDragCaret(dragCaretController, this, caretBrowsing)) {
        dragCaretController.paintDragCaret(frame(), paintInfo.context, paintOffset, paintInfo.rect);
    }
}

void RenderBlock::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    if (hasOverflowClip())
        scrolledOffset.move(-scrolledContentOffset());

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && style()->visibility() == VISIBLE) {
        if (hasBoxDecorations())
            paintBoxDecorations(paintInfo, paintOffset);
        if (hasColumns() && !paintInfo.paintRootBackgroundOnly())
            paintColumnRules(paintInfo, scrolledOffset);
    }

    if (paintPhase == PaintPhaseMask && style()->visibility() == VISIBLE) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhaseClippingMask && style()->visibility() == VISIBLE) {
        paintClippingMask(paintInfo, paintOffset);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground || paintInfo.paintRootBackgroundOnly())
        return;

    // 2. paint contents
    if (paintPhase != PaintPhaseSelfOutline) {
        if (hasColumns())
            paintColumnContents(paintInfo, scrolledOffset);
        else
            paintContents(paintInfo, scrolledOffset);
    }

    // 3. paint selection
    // FIXME: Make this work with multi column layouts.  For now don't fill gaps.
    bool isPrinting = document().printing();
    if (!isPrinting && !hasColumns())
        paintSelection(paintInfo, scrolledOffset); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (paintPhase == PaintPhaseFloat || paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip) {
        if (hasColumns())
            paintColumnContents(paintInfo, scrolledOffset, true);
        else
            paintFloats(paintInfo, scrolledOffset, paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && hasOutline() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo, LayoutRect(paintOffset, size()));

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseChildOutlines)) {
        RenderInline* inlineCont = inlineElementContinuation();
        if (inlineCont && inlineCont->hasOutline() && inlineCont->style()->visibility() == VISIBLE) {
            RenderInline* inlineRenderer = toRenderInline(inlineCont->node()->renderer());
            RenderBlock* cb = containingBlock();

            bool inlineEnclosedInSelfPaintingLayer = false;
            for (RenderBoxModelObject* box = inlineRenderer; box != cb; box = box->parent()->enclosingBoxModelObject()) {
                if (box->hasSelfPaintingLayer()) {
                    inlineEnclosedInSelfPaintingLayer = true;
                    break;
                }
            }

            // Do not add continuations for outline painting by our containing block if we are a relative positioned
            // anonymous block (i.e. have our own layer), paint them straightaway instead. This is because a block depends on renderers in its continuation table being
            // in the same layer.
            if (!inlineEnclosedInSelfPaintingLayer && !hasLayer())
                cb->addContinuationWithOutline(inlineRenderer);
            else if (!inlineRenderer->firstLineBox() || (!inlineEnclosedInSelfPaintingLayer && hasLayer()))
                inlineRenderer->paintOutline(paintInfo, paintOffset - locationOffset() + inlineRenderer->containingBlock()->location());
        }
        paintContinuationOutlines(paintInfo, paintOffset);
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhaseForeground,
    // then paint the caret.
    if (paintPhase == PaintPhaseForeground) {
        paintCarets(paintInfo, paintOffset);
    }
}

RenderInline* RenderBlock::inlineElementContinuation() const
{
    RenderBoxModelObject* continuation = this->continuation();
    return continuation && continuation->isInline() ? toRenderInline(continuation) : 0;
}

RenderBlock* RenderBlock::blockElementContinuation() const
{
    RenderBoxModelObject* currentContinuation = continuation();
    if (!currentContinuation || currentContinuation->isInline())
        return 0;
    RenderBlock* nextContinuation = toRenderBlock(currentContinuation);
    if (nextContinuation->isAnonymousBlock())
        return nextContinuation->blockElementContinuation();
    return nextContinuation;
}

static ContinuationOutlineTableMap* continuationOutlineTable()
{
    DEFINE_STATIC_LOCAL(ContinuationOutlineTableMap, table, ());
    return &table;
}

void RenderBlock::addContinuationWithOutline(RenderInline* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer() && !flow->isInlineElementContinuation());

    ContinuationOutlineTableMap* table = continuationOutlineTable();
    ListHashSet<RenderInline*>* continuations = table->get(this);
    if (!continuations) {
        continuations = new ListHashSet<RenderInline*>;
        table->set(this, adoptPtr(continuations));
    }

    continuations->add(flow);
}

bool RenderBlock::paintsContinuationOutline(RenderInline* flow)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return false;

    ListHashSet<RenderInline*>* continuations = table->get(this);
    if (!continuations)
        return false;

    return continuations->contains(flow);
}

void RenderBlock::paintContinuationOutlines(PaintInfo& info, const LayoutPoint& paintOffset)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return;

    OwnPtr<ListHashSet<RenderInline*> > continuations = table->take(this);
    if (!continuations)
        return;

    LayoutPoint accumulatedPaintOffset = paintOffset;
    // Paint each continuation outline.
    ListHashSet<RenderInline*>::iterator end = continuations->end();
    for (ListHashSet<RenderInline*>::iterator it = continuations->begin(); it != end; ++it) {
        // Need to add in the coordinates of the intervening blocks.
        RenderInline* flow = *it;
        RenderBlock* block = flow->containingBlock();
        for ( ; block && block != this; block = block->containingBlock())
            accumulatedPaintOffset.moveBy(block->location());
        ASSERT(block);
        flow->paintOutline(info, accumulatedPaintOffset);
    }
}

bool RenderBlock::shouldPaintSelectionGaps() const
{
    return selectionState() != SelectionNone && style()->visibility() == VISIBLE && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(node() || isAnonymous());

    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;

    if (isBody() || isDocumentElement() || hasOverflowClip()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransform() || hasReflection() || hasMask() || isWritingModeRoot()
        || isRenderFlowThread() || isFlexItemIncludingDeprecated())
        return true;

    if (view() && view()->selectionStart()) {
        Node* startElement = view()->selectionStart()->node();
        if (startElement && startElement->rootEditableElement() == node())
            return true;
    }

    return false;
}

GapRects RenderBlock::selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer)
{
    ASSERT(!needsLayout());

    if (!shouldPaintSelectionGaps())
        return GapRects();

    TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint());
    mapLocalToContainer(repaintContainer, transformState, ApplyContainerFlip | UseTransforms);
    LayoutPoint offsetFromRepaintContainer = roundedLayoutPoint(transformState.mappedPoint());

    if (hasOverflowClip())
        offsetFromRepaintContainer -= scrolledContentOffset();

    LayoutUnit lastTop = 0;
    LayoutUnit lastLeft = logicalLeftSelectionOffset(this, lastTop);
    LayoutUnit lastRight = logicalRightSelectionOffset(this, lastTop);

    return selectionGaps(this, offsetFromRepaintContainer, IntSize(), lastTop, lastLeft, lastRight);
}

void RenderBlock::paintSelection(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (shouldPaintSelectionGaps() && paintInfo.phase == PaintPhaseForeground) {
        LayoutUnit lastTop = 0;
        LayoutUnit lastLeft = logicalLeftSelectionOffset(this, lastTop);
        LayoutUnit lastRight = logicalRightSelectionOffset(this, lastTop);
        GraphicsContextStateSaver stateSaver(*paintInfo.context);

        LayoutRect gapRectsBounds = selectionGaps(this, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight, &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            RenderLayer* layer = enclosingLayer();
            gapRectsBounds.moveBy(-paintOffset);
            if (!hasLayer()) {
                LayoutRect localBounds(gapRectsBounds);
                flipForWritingMode(localBounds);
                gapRectsBounds = localToContainerQuad(FloatRect(localBounds), layer->renderer()).enclosingBoundingBox();
                if (layer->renderer()->hasOverflowClip())
                    gapRectsBounds.move(layer->renderBox()->scrolledContentOffset());
            }
            layer->addBlockSelectionGapsBounds(gapRectsBounds);
        }
    }
}

static void clipOutPositionedObjects(const PaintInfo* paintInfo, const LayoutPoint& offset, TrackedRendererListHashSet* positionedObjects)
{
    if (!positionedObjects)
        return;

    TrackedRendererListHashSet::const_iterator end = positionedObjects->end();
    for (TrackedRendererListHashSet::const_iterator it = positionedObjects->begin(); it != end; ++it) {
        RenderBox* r = *it;
        paintInfo->context->clipOut(IntRect(offset.x() + r->x(), offset.y() + r->y(), r->width(), r->height()));
    }
}

LayoutUnit RenderBlock::blockDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.height() : offsetFromBlock.width();
}

LayoutUnit RenderBlock::inlineDirectionOffset(const LayoutSize& offsetFromBlock) const
{
    return isHorizontalWritingMode() ? offsetFromBlock.width() : offsetFromBlock.height();
}

LayoutRect RenderBlock::logicalRectToPhysicalRect(const LayoutPoint& rootBlockPhysicalPosition, const LayoutRect& logicalRect)
{
    LayoutRect result;
    if (isHorizontalWritingMode())
        result = logicalRect;
    else
        result = LayoutRect(logicalRect.y(), logicalRect.x(), logicalRect.height(), logicalRect.width());
    flipForWritingMode(result);
    result.moveBy(rootBlockPhysicalPosition);
    return result;
}

GapRects RenderBlock::selectionGaps(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo* paintInfo)
{
    // IMPORTANT: Callers of this method that intend for painting to happen need to do a save/restore.
    // Clip out floating and positioned objects when painting selection gaps.
    if (paintInfo) {
        // Note that we don't clip out overflow for positioned objects.  We just stick to the border box.
        LayoutRect flippedBlockRect(offsetFromRootBlock.width(), offsetFromRootBlock.height(), width(), height());
        rootBlock->flipForWritingMode(flippedBlockRect);
        flippedBlockRect.moveBy(rootBlockPhysicalPosition);
        clipOutPositionedObjects(paintInfo, flippedBlockRect.location(), positionedObjects());
        if (isBody() || isDocumentElement()) // The <body> must make sure to examine its containingBlock's positioned objects.
            for (RenderBlock* cb = containingBlock(); cb && !cb->isRenderView(); cb = cb->containingBlock())
                clipOutPositionedObjects(paintInfo, LayoutPoint(cb->x(), cb->y()), cb->positionedObjects()); // FIXME: Not right for flipped writing modes.
        clipOutFloatingObjects(rootBlock, paintInfo, rootBlockPhysicalPosition, offsetFromRootBlock);
    }

    // FIXME: overflow: auto/scroll regions need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isRenderBlockFlow() && !isFlexibleBoxIncludingDeprecated()) // FIXME: Make multi-column selection gap filling work someday.
        return result;

    if (hasColumns() || hasTransform() || style()->columnSpan()) {
        // FIXME: We should learn how to gap fill multiple columns and transforms eventually.
        lastLogicalTop = rootBlock->blockDirectionOffset(offsetFromRootBlock) + logicalHeight();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight());
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight());
        return result;
    }

    if (childrenInline())
        result = toRenderBlockFlow(this)->inlineSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, paintInfo);
    else
        result = blockSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, paintInfo);

    // Go ahead and fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (rootBlock == this && (selectionState() != SelectionBoth && selectionState() != SelectionEnd))
        result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight,
                                             logicalHeight(), paintInfo));
    return result;
}

GapRects RenderBlock::blockSelectionGaps(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                         LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo* paintInfo)
{
    GapRects result;

    // Go ahead and jump right to the first block child that contains some selected objects.
    RenderBox* curr;
    for (curr = firstChildBox(); curr && curr->selectionState() == SelectionNone; curr = curr->nextSiblingBox()) { }

    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSiblingBox()) {
        SelectionState childState = curr->selectionState();
        if (childState == SelectionBoth || childState == SelectionEnd)
            sawSelectionEnd = true;

        if (curr->isFloatingOrOutOfFlowPositioned())
            continue; // We must be a normal flow object in order to even be considered.

        if (curr->isInFlowPositioned() && curr->hasLayer()) {
            // If the relposition offset is anything other than 0, then treat this just like an absolute positioned element.
            // Just disregard it completely.
            LayoutSize relOffset = curr->layer()->offsetForInFlowPosition();
            if (relOffset.width() || relOffset.height())
                continue;
        }

        bool paintsOwnSelection = curr->shouldPaintSelectionGaps() || curr->isTable(); // FIXME: Eventually we won't special-case table like this.
        bool fillBlockGaps = paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != SelectionNone);
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == SelectionEnd || childState == SelectionInside)
                // Fill the gap above the object.
                result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight,
                                                     curr->logicalTop(), paintInfo));

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == SelectionStart || sawSelectionEnd))
                childState = SelectionNone;

            // Fill side gaps on this object based off its state.
            bool leftGap, rightGap;
            getSelectionGapInfo(childState, leftGap, rightGap);

            if (leftGap)
                result.uniteLeft(logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalLeft(), curr->logicalTop(), curr->logicalHeight(), paintInfo));
            if (rightGap)
                result.uniteRight(logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalRight(), curr->logicalTop(), curr->logicalHeight(), paintInfo));

            // Update lastLogicalTop to be just underneath the object.  lastLogicalLeft and lastLogicalRight extend as far as
            // they can without bumping into floating or positioned objects.  Ideally they will go right up
            // to the border of the root selection block.
            lastLogicalTop = rootBlock->blockDirectionOffset(offsetFromRootBlock) + curr->logicalBottom();
            lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, curr->logicalBottom());
            lastLogicalRight = logicalRightSelectionOffset(rootBlock, curr->logicalBottom());
        } else if (childState != SelectionNone)
            // We must be a block that has some selected object inside it.  Go ahead and recur.
            result.unite(toRenderBlock(curr)->selectionGaps(rootBlock, rootBlockPhysicalPosition, LayoutSize(offsetFromRootBlock.width() + curr->x(), offsetFromRootBlock.height() + curr->y()),
                                                            lastLogicalTop, lastLogicalLeft, lastLogicalRight, paintInfo));
    }
    return result;
}

LayoutRect RenderBlock::blockSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                          LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const PaintInfo* paintInfo)
{
    LayoutUnit logicalTop = lastLogicalTop;
    LayoutUnit logicalHeight = rootBlock->blockDirectionOffset(offsetFromRootBlock) + logicalBottom - logicalTop;
    if (logicalHeight <= 0)
        return LayoutRect();

    // Get the selection offsets for the bottom of the gap
    LayoutUnit logicalLeft = max(lastLogicalLeft, logicalLeftSelectionOffset(rootBlock, logicalBottom));
    LayoutUnit logicalRight = min(lastLogicalRight, logicalRightSelectionOffset(rootBlock, logicalBottom));
    LayoutUnit logicalWidth = logicalRight - logicalLeft;
    if (logicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock->logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(logicalLeft, logicalTop, logicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalLeftSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                                RenderObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = rootBlock->blockDirectionOffset(offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = max(logicalLeftSelectionOffset(rootBlock, logicalTop), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight));
    LayoutUnit rootBlockLogicalRight = min(rootBlock->inlineDirectionOffset(offsetFromRootBlock) + floorToInt(logicalLeft), min(logicalRightSelectionOffset(rootBlock, logicalTop), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight)));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock->logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selObj->selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalRightSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                                 RenderObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = rootBlock->blockDirectionOffset(offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = max(rootBlock->inlineDirectionOffset(offsetFromRootBlock) + floorToInt(logicalRight), max(logicalLeftSelectionOffset(rootBlock, logicalTop), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight)));
    LayoutUnit rootBlockLogicalRight = min(logicalRightSelectionOffset(rootBlock, logicalTop), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock->logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selObj->selectionBackgroundColor());
    return gapRect;
}

void RenderBlock::getSelectionGapInfo(SelectionState state, bool& leftGap, bool& rightGap)
{
    bool ltr = style()->isLeftToRightDirection();
    leftGap = (state == RenderObject::SelectionInside) ||
              (state == RenderObject::SelectionEnd && ltr) ||
              (state == RenderObject::SelectionStart && !ltr);
    rightGap = (state == RenderObject::SelectionInside) ||
               (state == RenderObject::SelectionStart && ltr) ||
               (state == RenderObject::SelectionEnd && !ltr);
}

LayoutUnit RenderBlock::logicalLeftSelectionOffset(RenderBlock* rootBlock, LayoutUnit position)
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalLeftSelectionOffset(rootBlock, position + logicalTop());
    return logicalLeftOffsetForContent();
}

LayoutUnit RenderBlock::logicalRightSelectionOffset(RenderBlock* rootBlock, LayoutUnit position)
{
    // The border can potentially be further extended by our containingBlock().
    if (rootBlock != this)
        return containingBlock()->logicalRightSelectionOffset(rootBlock, position + logicalTop());
    return logicalRightOffsetForContent();
}

RenderBlock* RenderBlock::blockBeforeWithinSelectionRoot(LayoutSize& offset) const
{
    if (isSelectionRoot())
        return 0;

    const RenderObject* object = this;
    RenderObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!sibling->isRenderBlock() || toRenderBlock(sibling)->isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(toRenderBlock(object)->logicalLeft(), toRenderBlock(object)->logicalTop());
        object = object->parent();
    } while (!sibling && object && object->isRenderBlock() && !toRenderBlock(object)->isSelectionRoot());

    if (!sibling)
        return 0;

    RenderBlock* beforeBlock = toRenderBlock(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    RenderObject* child = beforeBlock->lastChild();
    while (child && child->isRenderBlock()) {
        beforeBlock = toRenderBlock(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

void RenderBlock::insertIntoTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap) {
        descendantsMap = new TrackedDescendantsMap;
        containerMap = new TrackedContainerMap;
    }

    TrackedRendererListHashSet* descendantSet = descendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedRendererListHashSet;
        descendantsMap->set(this, adoptPtr(descendantSet));
    }
    bool added = descendantSet->add(descendant).isNewEntry;
    if (!added) {
        ASSERT(containerMap->get(descendant));
        ASSERT(containerMap->get(descendant)->contains(this));
        return;
    }

    HashSet<RenderBlock*>* containerSet = containerMap->get(descendant);
    if (!containerSet) {
        containerSet = new HashSet<RenderBlock*>;
        containerMap->set(descendant, adoptPtr(containerSet));
    }
    ASSERT(!containerSet->contains(this));
    containerSet->add(this);
}

void RenderBlock::removeFromTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap)
        return;

    OwnPtr<HashSet<RenderBlock*> > containerSet = containerMap->take(descendant);
    if (!containerSet)
        return;

    HashSet<RenderBlock*>::iterator end = containerSet->end();
    for (HashSet<RenderBlock*>::iterator it = containerSet->begin(); it != end; ++it) {
        RenderBlock* container = *it;

        // FIXME: Disabling this assert temporarily until we fix the layout
        // bugs associated with positioned objects not properly cleared from
        // their ancestor chain before being moved. See webkit bug 93766.
        // ASSERT(descendant->isDescendantOf(container));

        TrackedDescendantsMap::iterator descendantsMapIterator = descendantsMap->find(container);
        ASSERT(descendantsMapIterator != descendantsMap->end());
        if (descendantsMapIterator == descendantsMap->end())
            continue;
        TrackedRendererListHashSet* descendantSet = descendantsMapIterator->value.get();
        ASSERT(descendantSet->contains(descendant));
        descendantSet->remove(descendant);
        if (descendantSet->isEmpty())
            descendantsMap->remove(descendantsMapIterator);
    }
}

TrackedRendererListHashSet* RenderBlock::positionedObjects() const
{
    if (gPositionedDescendantsMap)
        return gPositionedDescendantsMap->get(this);
    return 0;
}

void RenderBlock::insertPositionedObject(RenderBox* o)
{
    ASSERT(!isAnonymousBlock());

    if (o->isRenderFlowThread())
        return;

    insertIntoTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::removePositionedObject(RenderBox* o)
{
    removeFromTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::removePositionedObjects(RenderBlock* o, ContainingBlockState containingBlockState)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    RenderBox* r;

    TrackedRendererListHashSet::iterator end = positionedDescendants->end();

    Vector<RenderBox*, 16> deadObjects;

    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        r = *it;
        if (!o || r->isDescendantOf(o)) {
            if (containingBlockState == NewContainingBlock)
                r->setChildNeedsLayout(MarkOnlyThis);

            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            RenderObject* p = r->parent();
            while (p && !p->isRenderBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout();

            deadObjects.append(r);
        }
    }

    for (unsigned i = 0; i < deadObjects.size(); i++)
        removePositionedObject(deadObjects.at(i));
}

void RenderBlock::addPercentHeightDescendant(RenderBox* descendant)
{
    insertIntoTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

void RenderBlock::removePercentHeightDescendant(RenderBox* descendant)
{
    removeFromTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

TrackedRendererListHashSet* RenderBlock::percentHeightDescendants() const
{
    return gPercentHeightDescendantsMap ? gPercentHeightDescendantsMap->get(this) : 0;
}

bool RenderBlock::hasPercentHeightContainerMap()
{
    return gPercentHeightContainerMap;
}

bool RenderBlock::hasPercentHeightDescendant(RenderBox* descendant)
{
    // We don't null check gPercentHeightContainerMap since the caller
    // already ensures this and we need to call this function on every
    // descendant in clearPercentHeightDescendantsFrom().
    ASSERT(gPercentHeightContainerMap);
    return gPercentHeightContainerMap->contains(descendant);
}

void RenderBlock::dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope& layoutScope)
{
    if (!gPercentHeightDescendantsMap)
        return;

    TrackedRendererListHashSet* descendants = gPercentHeightDescendantsMap->get(this);
    if (!descendants)
        return;

    TrackedRendererListHashSet::iterator end = descendants->end();
    for (TrackedRendererListHashSet::iterator it = descendants->begin(); it != end; ++it) {
        RenderBox* box = *it;
        while (box != this) {
            if (box->normalChildNeedsLayout())
                break;
            layoutScope.setChildNeedsLayout(box);
            box = box->containingBlock();
            ASSERT(box);
            if (!box)
                break;
        }
    }
}

void RenderBlock::removePercentHeightDescendantIfNeeded(RenderBox* descendant)
{
    // We query the map directly, rather than looking at style's
    // logicalHeight()/logicalMinHeight()/logicalMaxHeight() since those
    // can change with writing mode/directional changes.
    if (!hasPercentHeightContainerMap())
        return;

    if (!hasPercentHeightDescendant(descendant))
        return;

    removePercentHeightDescendant(descendant);
}

void RenderBlock::clearPercentHeightDescendantsFrom(RenderBox* parent)
{
    ASSERT(gPercentHeightContainerMap);
    for (RenderObject* curr = parent->slowFirstChild(); curr; curr = curr->nextInPreOrder(parent)) {
        if (!curr->isBox())
            continue;

        RenderBox* box = toRenderBox(curr);
        if (!hasPercentHeightDescendant(box))
            continue;

        removePercentHeightDescendant(box);
    }
}

LayoutUnit RenderBlock::textIndentOffset() const
{
    LayoutUnit cw = 0;
    if (style()->textIndent().isPercent())
        cw = containingBlock()->availableLogicalWidth();
    return minimumValueForLength(style()->textIndent(), cw);
}

void RenderBlock::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    RootInlineBox* lowestDirtyLine = lastRootBox();
    RootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->lineBottomWithLeading() >= logicalBottom && logicalBottom < LayoutUnit::max()) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest != highest && (afterLowest->lineBottomWithLeading() >= logicalTop || afterLowest->lineBottomWithLeading() < 0)) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

bool RenderBlock::avoidsFloats() const
{
    // Floats can't intrude into our box if we have a non-auto column count or width.
    return RenderBox::avoidsFloats() || !style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth();
}

bool RenderBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!scrollsOverflow())
        return false;

    return layer()->scrollableArea()->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
}

Node* RenderBlock::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    return isAnonymousBlockContinuation() ? continuation()->node() : node();
}

bool RenderBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint adjustedLocation(accumulatedOffset + location());
    LayoutSize localOffset = toLayoutSize(adjustedLocation);

    if (!isRenderView()) {
        // Check if we need to do anything at all.
        // If we have clipping, then we can't have any spillout.
        LayoutRect overflowBox = hasOverflowClip() ? borderBoxRect() : visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedLocation);
        if (!locationInContainer.intersects(overflowBox))
            return false;
    }

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground)
        && visibleToHitTestRequest(request)
        && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (!result.addNodeToRectBasedTestResult(nodeForHitTest(), request, locationInContainer))
           return true;
    }

    if (style()->clipPath()) {
        switch (style()->clipPath()->type()) {
        case ClipPathOperation::SHAPE: {
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(style()->clipPath());
            // FIXME: handle marginBox etc.
            if (!clipPath->path(borderBoxRect()).contains(locationInContainer.point() - localOffset, clipPath->windRule()))
                return false;
            break;
        }
        case ClipPathOperation::REFERENCE:
            // FIXME: handle REFERENCE
            break;
        }
    }

    // If we have clipping, then we can't have any spillout.
    bool useOverflowClip = hasOverflowClip() && !hasSelfPaintingLayer();
    bool useClip = (hasControlClip() || useOverflowClip);
    bool checkChildren = !useClip;
    if (!checkChildren) {
        if (hasControlClip()) {
            checkChildren = locationInContainer.intersects(controlClipRect(adjustedLocation));
        } else {
            LayoutRect clipRect = overflowClipRect(adjustedLocation, IncludeOverlayScrollbarSize);
            if (style()->hasBorderRadius())
                checkChildren = locationInContainer.intersects(style()->getRoundedBorderFor(clipRect));
            else
                checkChildren = locationInContainer.intersects(clipRect);
        }
    }
    if (checkChildren) {
        // Hit test descendants first.
        LayoutSize scrolledOffset(localOffset);
        if (hasOverflowClip())
            scrolledOffset -= scrolledContentOffset();

        // Hit test contents if we don't have columns.
        if (!hasColumns()) {
            if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
                updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
                return true;
            }
            if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
                return true;
        } else if (hitTestColumns(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            return true;
        }
    }

    // Check if the point is outside radii.
    if (style()->hasBorderRadius()) {
        LayoutRect borderRect = borderBoxRect();
        borderRect.moveBy(adjustedLocation);
        RoundedRect border = style()->getRoundedBorderFor(borderRect);
        if (!locationInContainer.intersects(border))
            return false;
    }

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTestRequest(request) && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (!result.addNodeToRectBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect))
                return true;
        }
    }

    return false;
}

class ColumnRectIterator {
    WTF_MAKE_NONCOPYABLE(ColumnRectIterator);
public:
    ColumnRectIterator(const RenderBlock& block)
        : m_block(block)
        , m_colInfo(block.columnInfo())
        , m_direction(m_block.style()->isFlippedBlocksWritingMode() ? 1 : -1)
        , m_isHorizontal(block.isHorizontalWritingMode())
        , m_logicalLeft(block.logicalLeftOffsetForContent())
    {
        int colCount = m_colInfo->columnCount();
        m_colIndex = colCount - 1;
        m_currLogicalTopOffset = colCount * m_colInfo->columnHeight() * m_direction;
        update();
    }

    void advance()
    {
        ASSERT(hasMore());
        m_colIndex--;
        update();
    }

    LayoutRect columnRect() const { return m_colRect; }
    bool hasMore() const { return m_colIndex >= 0; }

    void adjust(LayoutSize& offset) const
    {
        LayoutUnit currLogicalLeftOffset = (m_isHorizontal ? m_colRect.x() : m_colRect.y()) - m_logicalLeft;
        offset += m_isHorizontal ? LayoutSize(currLogicalLeftOffset, m_currLogicalTopOffset) : LayoutSize(m_currLogicalTopOffset, currLogicalLeftOffset);
        if (m_colInfo->progressionAxis() == ColumnInfo::BlockAxis) {
            if (m_isHorizontal)
                offset.expand(0, m_colRect.y() - m_block.borderTop() - m_block.paddingTop());
            else
                offset.expand(m_colRect.x() - m_block.borderLeft() - m_block.paddingLeft(), 0);
        }
    }

private:
    void update()
    {
        if (m_colIndex < 0)
            return;

        m_colRect = m_block.columnRectAt(const_cast<ColumnInfo*>(m_colInfo), m_colIndex);
        m_block.flipForWritingMode(m_colRect);
        m_currLogicalTopOffset -= (m_isHorizontal ? m_colRect.height() : m_colRect.width()) * m_direction;
    }

    const RenderBlock& m_block;
    const ColumnInfo* const m_colInfo;
    const int m_direction;
    const bool m_isHorizontal;
    const LayoutUnit m_logicalLeft;
    int m_colIndex;
    LayoutUnit m_currLogicalTopOffset;
    LayoutRect m_colRect;
};

bool RenderBlock::hitTestColumns(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // We need to do multiple passes, breaking up our hit testing into strips.
    if (!hasColumns())
        return false;

    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect hitRect = locationInContainer.boundingBox();
        LayoutRect colRect = it.columnRect();
        colRect.moveBy(accumulatedOffset);
        if (locationInContainer.intersects(colRect)) {
            // The point is inside this column.
            // Adjust accumulatedOffset to change where we hit test.
            LayoutSize offset;
            it.adjust(offset);
            LayoutPoint finalLocation = accumulatedOffset + offset;
            if (!result.isRectBasedTest() || colRect.contains(hitRect))
                return hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction) || (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, finalLocation));

            hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction);
        }
    }

    return false;
}

void RenderBlock::adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const
{
    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect colRect = it.columnRect();
        if (colRect.contains(locationInContainer)) {
            it.adjust(offset);
            return;
        }
    }
}

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable()) {
        // We have to hit-test our line boxes.
        if (m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction))
            return true;
    } else {
        // Hit test our children.
        HitTestAction childHitTest = hitTestAction;
        if (hitTestAction == HitTestChildBlockBackgrounds)
            childHitTest = HitTestChildBlockBackground;
        for (RenderBox* child = lastChildBox(); child; child = child->previousSiblingBox()) {
            LayoutPoint childPoint = flipForWritingModeForChild(child, accumulatedOffset);
            if (!child->hasSelfPaintingLayer() && !child->isFloating() && child->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest))
                return true;
        }
    }

    return false;
}

Position RenderBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->renderer().nonPseudoNode())
        return createLegacyEditingPosition(nonPseudoNode(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    InlineTextBox* textBox = toInlineTextBox(box);
    return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? textBox->start() : textBox->start() + textBox->len());
}

static inline bool isEditingBoundary(RenderObject* ancestor, RenderObject* child)
{
    ASSERT(!ancestor || ancestor->nonPseudoNode());
    ASSERT(child && child->nonPseudoNode());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isRenderView())
        || ancestor->nonPseudoNode()->rendererIsEditable() == child->nonPseudoNode()->rendererIsEditable();
}

// FIXME: This function should go on RenderObject as an instance method. Then
// all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
static PositionWithAffinity positionForPointRespectingEditingBoundaries(RenderBlock* parent, RenderBox* child, const LayoutPoint& pointInParentCoordinates)
{
    LayoutPoint childLocation = child->location();
    if (child->isInFlowPositioned())
        childLocation += child->offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous renderer, we just recur normally
    Node* childNode = child->nonPseudoNode();
    if (!childNode)
        return child->positionForPoint(pointInChildCoordinates);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    RenderObject* ancestor = parent;
    while (ancestor && !ancestor->nonPseudoNode())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child->positionForPoint(pointInChildCoordinates);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent->logicalWidthForChild(child) / 2;
    LayoutUnit logicalLeft = parent->isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createPositionWithAffinity(childNode->nodeIndex(), DOWNSTREAM);
    return ancestor->createPositionWithAffinity(childNode->nodeIndex() + 1, UPSTREAM);
}

PositionWithAffinity RenderBlock::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents)
{
    ASSERT(childrenInline());

    if (!firstRootBox())
        return createPositionWithAffinity(0, DOWNSTREAM);

    bool linesAreFlipped = style()->isFlippedLinesWritingMode();
    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineBox* closestBox = 0;
    RootInlineBox* firstRootBoxWithChildren = 0;
    RootInlineBox* lastRootBoxWithChildren = 0;
    for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
        if (!root->firstLeafChild())
            continue;
        if (!firstRootBoxWithChildren)
            firstRootBoxWithChildren = root;

        if (!linesAreFlipped && root->isFirstAfterPageBreak() && (pointInLogicalContents.y() < root->lineTopWithLeading()
            || (blocksAreFlipped && pointInLogicalContents.y() == root->lineTopWithLeading())))
            break;

        lastRootBoxWithChildren = root;

        // check if this root line box is located at this y coordinate
        if (pointInLogicalContents.y() < root->selectionBottom() || (blocksAreFlipped && pointInLogicalContents.y() == root->selectionBottom())) {
            if (linesAreFlipped) {
                RootInlineBox* nextRootBoxWithChildren = root->nextRootBox();
                while (nextRootBoxWithChildren && !nextRootBoxWithChildren->firstLeafChild())
                    nextRootBoxWithChildren = nextRootBoxWithChildren->nextRootBox();

                if (nextRootBoxWithChildren && nextRootBoxWithChildren->isFirstAfterPageBreak() && (pointInLogicalContents.y() > nextRootBoxWithChildren->lineTopWithLeading()
                    || (!blocksAreFlipped && pointInLogicalContents.y() == nextRootBoxWithChildren->lineTopWithLeading())))
                    continue;
            }
            closestBox = root->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
            if (closestBox)
                break;
        }
    }

    bool moveCaretToBoundary = document().frame()->editor().behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastRootBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = lastRootBoxWithChildren->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            LayoutUnit firstRootBoxWithChildrenTop = min<LayoutUnit>(firstRootBoxWithChildren->selectionTop(), firstRootBoxWithChildren->logicalTop());
            if (pointInLogicalContents.y() < firstRootBoxWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstRootBoxWithChildrenTop)) {
                InlineBox* box = firstRootBoxWithChildren->firstLeafChild();
                if (box->isLineBreak()) {
                    if (InlineBox* newBox = box->nextLeafChildIgnoringLineBreak())
                        box = newBox;
                }
                // y coordinate is above first root line box, so return the start of the first
                return PositionWithAffinity(positionForBox(box, true), DOWNSTREAM);
            }
        }

        // pass the box a top position that is inside it
        LayoutPoint point(pointInLogicalContents.x(), closestBox->root().blockDirectionPointInLine());
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->renderer().isReplaced())
            return positionForPointRespectingEditingBoundaries(this, &toRenderBox(closestBox->renderer()), point);
        return closestBox->renderer().positionForPoint(point);
    }

    if (lastRootBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineBox* logicallyLastBox;
        if (lastRootBoxWithChildren->getLogicalEndBoxWithNode(logicallyLastBox))
            return PositionWithAffinity(positionForBox(logicallyLastBox, false), DOWNSTREAM);
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createPositionWithAffinity(0, DOWNSTREAM);
}

static inline bool isChildHitTestCandidate(RenderBox* box)
{
    return box->height() && box->style()->visibility() == VISIBLE && !box->isFloatingOrOutOfFlowPositioned();
}

PositionWithAffinity RenderBlock::positionForPoint(const LayoutPoint& point)
{
    if (isTable())
        return RenderBox::positionForPoint(point);

    if (isReplaced()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalLeft < 0)
            return createPositionWithAffinity(caretMinOffset(), DOWNSTREAM);
        if (pointLogicalLeft >= logicalWidth())
            return createPositionWithAffinity(caretMaxOffset(), DOWNSTREAM);
        if (pointLogicalTop < 0)
            return createPositionWithAffinity(caretMinOffset(), DOWNSTREAM);
        if (pointLogicalTop >= logicalHeight())
            return createPositionWithAffinity(caretMaxOffset(), DOWNSTREAM);
    }

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (childrenInline())
        return positionForPointWithInlineChildren(pointInLogicalContents);

    RenderBox* lastCandidateBox = lastChildBox();
    while (lastCandidateBox && !isChildHitTestCandidate(lastCandidateBox))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style()->isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(this, lastCandidateBox, pointInContents);

        for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(childBox))
                continue;
            LayoutUnit childLogicalBottom = logicalTopForChild(childBox) + logicalHeightForChild(childBox);
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (isChildHitTestCandidate(childBox) && (pointInLogicalContents.y() < childLogicalBottom
                || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom)))
                return positionForPointRespectingEditingBoundaries(this, childBox, pointInContents);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return RenderBox::positionForPoint(point);
}

void RenderBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);

    if (hasOverflowClip())
        offset += scrolledContentOffset();

    if (hasColumns())
        adjustPointToColumnContents(offset);

    offset = flipForWritingMode(offset);
}

LayoutUnit RenderBlock::availableLogicalWidth() const
{
    // If we have multiple columns, then the available logical width is reduced to our column width.
    if (hasColumns())
        return desiredColumnWidth();
    return RenderBox::availableLogicalWidth();
}

int RenderBlock::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

void RenderBlock::calcColumnWidth()
{
    if (document().regionBasedColumnsEnabled())
        return;

    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned desiredColumnCount = 1;
    LayoutUnit desiredColumnWidth = contentLogicalWidth();

    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document().paginated() || !style()->specifiesColumns()) {
        setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }

    LayoutUnit availWidth = desiredColumnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = max<LayoutUnit>(1, LayoutUnit(style()->columnWidth()));
    int colCount = max<int>(1, style()->columnCount());

    if (style()->hasAutoColumnWidth() && !style()->hasAutoColumnCount()) {
        desiredColumnCount = colCount;
        desiredColumnWidth = max<LayoutUnit>(0, (availWidth - ((desiredColumnCount - 1) * colGap)) / desiredColumnCount);
    } else if (!style()->hasAutoColumnWidth() && style()->hasAutoColumnCount()) {
        desiredColumnCount = max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    } else {
        desiredColumnCount = max<LayoutUnit>(min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    }
    setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
}

bool RenderBlock::requiresColumns(int desiredColumnCount) const
{
    // Paged overflow is treated as multicol here, unless this element was the one that got its
    // overflow propagated to the viewport.
    bool isPaginated = style()->isOverflowPaged() && node() != document().viewportDefiningElement();

    return firstChild()
        && (desiredColumnCount != 1 || !style()->hasAutoColumnWidth() || isPaginated)
        && !firstChild()->isAnonymousColumnsBlock()
        && !firstChild()->isAnonymousColumnSpanBlock();
}

void RenderBlock::setDesiredColumnCountAndWidth(int count, LayoutUnit width)
{
    bool destroyColumns = !requiresColumns(count);
    if (destroyColumns) {
        if (hasColumns()) {
            gColumnInfoMap->take(this);
            setHasColumns(false);
        }
    } else {
        ColumnInfo* info;
        if (hasColumns())
            info = gColumnInfoMap->get(this);
        else {
            if (!gColumnInfoMap)
                gColumnInfoMap = new ColumnInfoMap;
            info = new ColumnInfo;
            gColumnInfoMap->add(this, adoptPtr(info));
            setHasColumns(true);
        }
        info->setDesiredColumnWidth(width);
        if (style()->isOverflowPaged()) {
            info->setDesiredColumnCount(1);
            info->setProgressionAxis(style()->hasInlinePaginationAxis() ? ColumnInfo::InlineAxis : ColumnInfo::BlockAxis);
        } else {
            info->setDesiredColumnCount(count);
            info->setProgressionAxis(ColumnInfo::InlineAxis);
        }
    }
}

LayoutUnit RenderBlock::desiredColumnWidth() const
{
    if (!hasColumns())
        return contentLogicalWidth();
    return gColumnInfoMap->get(this)->desiredColumnWidth();
}

ColumnInfo* RenderBlock::columnInfo() const
{
    if (!hasColumns())
        return 0;
    return gColumnInfoMap->get(this);
}

unsigned RenderBlock::columnCount(ColumnInfo* colInfo) const
{
    ASSERT(hasColumns());
    ASSERT(gColumnInfoMap->get(this) == colInfo);
    return colInfo->columnCount();
}

LayoutRect RenderBlock::columnRectAt(ColumnInfo* colInfo, unsigned index) const
{
    ASSERT(hasColumns() && gColumnInfoMap->get(this) == colInfo);

    // Compute the appropriate rect based off our information.
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();
    LayoutUnit colLogicalTop = borderBefore() + paddingBefore();
    LayoutUnit colLogicalLeft = logicalLeftOffsetForContent();
    LayoutUnit colGap = columnGap();
    if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
        if (style()->isLeftToRightDirection())
            colLogicalLeft += index * (colLogicalWidth + colGap);
        else
            colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);
    } else {
        colLogicalTop += index * (colLogicalHeight + colGap);
    }

    if (isHorizontalWritingMode())
        return LayoutRect(colLogicalLeft, colLogicalTop, colLogicalWidth, colLogicalHeight);
    return LayoutRect(colLogicalTop, colLogicalLeft, colLogicalHeight, colLogicalWidth);
}

void RenderBlock::adjustPointToColumnContents(LayoutPoint& point) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;

    ColumnInfo* colInfo = columnInfo();
    if (!columnCount(colInfo))
        return;

    // Determine which columns we intersect.
    LayoutUnit colGap = columnGap();
    LayoutUnit halfColGap = colGap / 2;
    LayoutPoint columnPoint(columnRectAt(colInfo, 0).location());
    LayoutUnit logicalOffset = 0;
    for (unsigned i = 0; i < colInfo->columnCount(); i++) {
        // Add in half the column gap to the left and right of the rect.
        LayoutRect colRect = columnRectAt(colInfo, i);
        flipForWritingMode(colRect);
        if (isHorizontalWritingMode() == (colInfo->progressionAxis() == ColumnInfo::InlineAxis)) {
            LayoutRect gapAndColumnRect(colRect.x() - halfColGap, colRect.y(), colRect.width() + colGap, colRect.height());
            if (point.x() >= gapAndColumnRect.x() && point.x() < gapAndColumnRect.maxX()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    // Clamp everything above the column to its top left.
                    if (point.y() < gapAndColumnRect.y())
                        point = gapAndColumnRect.location();
                    // Clamp everything below the column to the next column's top left. If there is
                    // no next column, this still maps to just after this column.
                    else if (point.y() >= gapAndColumnRect.maxY()) {
                        point = gapAndColumnRect.location();
                        point.move(0, gapAndColumnRect.height());
                    }
                } else {
                    if (point.x() < colRect.x())
                        point.setX(colRect.x());
                    else if (point.x() >= colRect.maxX())
                        point.setX(colRect.maxX() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    point.move(columnPoint.x() - colRect.x(), (!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset));
                else
                    point.move((!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.x() + borderLeft() + paddingLeft(), 0);
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionAxis() == ColumnInfo::InlineAxis ? colRect.height() : colRect.width();
        } else {
            LayoutRect gapAndColumnRect(colRect.x(), colRect.y() - halfColGap, colRect.width(), colRect.height() + colGap);
            if (point.y() >= gapAndColumnRect.y() && point.y() < gapAndColumnRect.maxY()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    // Clamp everything above the column to its top left.
                    if (point.x() < gapAndColumnRect.x())
                        point = gapAndColumnRect.location();
                    // Clamp everything below the column to the next column's top left. If there is
                    // no next column, this still maps to just after this column.
                    else if (point.x() >= gapAndColumnRect.maxX()) {
                        point = gapAndColumnRect.location();
                        point.move(gapAndColumnRect.width(), 0);
                    }
                } else {
                    if (point.y() < colRect.y())
                        point.setY(colRect.y());
                    else if (point.y() >= colRect.maxY())
                        point.setY(colRect.maxY() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    point.move((!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset), columnPoint.y() - colRect.y());
                else
                    point.move(0, (!style()->isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.y() + borderTop() + paddingTop());
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionAxis() == ColumnInfo::InlineAxis ? colRect.width() : colRect.height();
        }
    }
}

void RenderBlock::adjustRectForColumns(LayoutRect& r) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;

    ColumnInfo* colInfo = columnInfo();

    // Determine which columns we intersect.
    unsigned colCount = columnCount(colInfo);
    if (!colCount)
        return;

    // Begin with a result rect that is empty.
    LayoutRect result;

    bool isHorizontal = isHorizontalWritingMode();
    LayoutUnit beforeBorderPadding = borderBefore() + paddingBefore();
    LayoutUnit colHeight = colInfo->columnHeight();
    if (!colHeight)
        return;

    LayoutUnit startOffset = max(isHorizontal ? r.y() : r.x(), beforeBorderPadding);
    LayoutUnit endOffset = max(min<LayoutUnit>(isHorizontal ? r.maxY() : r.maxX(), beforeBorderPadding + colCount * colHeight), beforeBorderPadding);

    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned startColumn = (startOffset - beforeBorderPadding) / colHeight;
    unsigned endColumn = (endOffset - beforeBorderPadding) / colHeight;

    if (startColumn == endColumn) {
        // The rect is fully contained within one column. Adjust for our offsets
        // and repaint only that portion.
        LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent();
        LayoutRect colRect = columnRectAt(colInfo, startColumn);
        LayoutRect repaintRect = r;

        if (colInfo->progressionAxis() == ColumnInfo::InlineAxis) {
            if (isHorizontal)
                repaintRect.move(colRect.x() - logicalLeftOffset, - static_cast<int>(startColumn) * colHeight);
            else
                repaintRect.move(- static_cast<int>(startColumn) * colHeight, colRect.y() - logicalLeftOffset);
        } else {
            if (isHorizontal)
                repaintRect.move(0, colRect.y() - startColumn * colHeight - beforeBorderPadding);
            else
                repaintRect.move(colRect.x() - startColumn * colHeight - beforeBorderPadding, 0);
        }
        repaintRect.intersect(colRect);
        result.unite(repaintRect);
    } else {
        // We span multiple columns. We can just unite the start and end column to get the final
        // repaint rect.
        result.unite(columnRectAt(colInfo, startColumn));
        result.unite(columnRectAt(colInfo, endColumn));
    }

    r = result;
}

LayoutPoint RenderBlock::flipForWritingModeIncludingColumns(const LayoutPoint& point) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style()->isFlippedBlocksWritingMode())
        return point;
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderBefore() + paddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAfter() + paddingAfter() + scrollbarLogicalHeight();
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), expandedLogicalHeight - point.y());
    return LayoutPoint(expandedLogicalHeight - point.x(), point.y());
}

void RenderBlock::adjustStartEdgeForWritingModeIncludingColumns(LayoutRect& rect) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style()->isFlippedBlocksWritingMode())
        return;

    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderBefore() + paddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAfter() + paddingAfter() + scrollbarLogicalHeight();

    if (isHorizontalWritingMode())
        rect.setY(expandedLogicalHeight - rect.maxY());
    else
        rect.setX(expandedLogicalHeight - rect.maxX());
}

LayoutSize RenderBlock::columnOffset(const LayoutPoint& point) const
{
    if (!hasColumns())
        return LayoutSize();

    ColumnInfo* colInfo = columnInfo();

    LayoutUnit logicalLeft = logicalLeftOffsetForContent();
    unsigned colCount = columnCount(colInfo);
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();

    for (unsigned i = 0; i < colCount; ++i) {
        // Compute the edges for a given column in the block progression direction.
        LayoutRect sliceRect = LayoutRect(logicalLeft, borderBefore() + paddingBefore() + i * colLogicalHeight, colLogicalWidth, colLogicalHeight);
        if (!isHorizontalWritingMode())
            sliceRect = sliceRect.transposedRect();

        LayoutUnit logicalOffset = i * colLogicalHeight;

        // Now we're in the same coordinate space as the point.  See if it is inside the rectangle.
        if (isHorizontalWritingMode()) {
            if (point.y() >= sliceRect.y() && point.y() < sliceRect.maxY()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    return LayoutSize(columnRectAt(colInfo, i).x() - logicalLeft, -logicalOffset);
                return LayoutSize(0, columnRectAt(colInfo, i).y() - logicalOffset - borderBefore() - paddingBefore());
            }
        } else {
            if (point.x() >= sliceRect.x() && point.x() < sliceRect.maxX()) {
                if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                    return LayoutSize(-logicalOffset, columnRectAt(colInfo, i).y() - logicalLeft);
                return LayoutSize(columnRectAt(colInfo, i).x() - logicalOffset - borderBefore() - paddingBefore(), 0);
            }
        }
    }

    return LayoutSize();
}

void RenderBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (childrenInline()) {
        // FIXME: Remove this const_cast.
        toRenderBlockFlow(const_cast<RenderBlock*>(this))->computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    } else {
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    }

    maxLogicalWidth = max(minLogicalWidth, maxLogicalWidth);

    adjustIntrinsicLogicalWidthsForColumns(minLogicalWidth, maxLogicalWidth);

    // A horizontal marquee with inline children has no minimum width.
    if (childrenInline() && isMarquee() && toRenderMarquee(this)->isHorizontal())
        minLogicalWidth = 0;

    if (isTableCell()) {
        Length tableCellWidth = toRenderTableCell(this)->styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(tableCellWidth.value()));
    }

    int scrollbarWidth = instrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void RenderBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    updateFirstLetter();

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    // FIXME: The isFixed() calls here should probably be checking for isSpecified since you
    // should be able to use percentage, calc or viewport relative values for width.
    RenderStyle* styleToUse = style();
    if (!isTableCell() && styleToUse->logicalWidth().isFixed() && styleToUse->logicalWidth().value() >= 0
        && !(isDeprecatedFlexItem() && !styleToUse->logicalWidth().intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse->logicalMinWidth().isFixed() && styleToUse->logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMinWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMinWidth().value()));
    }

    if (styleToUse->logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMaxWidth().value()));
    }

    // Table layout uses integers, ceil the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth.ceil();
        m_maxPreferredLogicalWidth = m_maxPreferredLogicalWidth.ceil();
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearPreferredLogicalWidthsDirty();
}

void RenderBlock::adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth()) {
        // The min/max intrinsic widths calculated really tell how much space elements need when
        // laid out inside the columns. In order to eventually end up with the desired column width,
        // we need to convert them to values pertaining to the multicol container.
        int columnCount = style()->hasAutoColumnCount() ? 1 : style()->columnCount();
        LayoutUnit columnWidth;
        LayoutUnit gapExtra = (columnCount - 1) * columnGap();
        if (style()->hasAutoColumnWidth()) {
            minLogicalWidth = minLogicalWidth * columnCount + gapExtra;
        } else {
            columnWidth = style()->columnWidth();
            minLogicalWidth = min(minLogicalWidth, columnWidth);
        }
        // FIXME: If column-count is auto here, we should resolve it to calculate the maximum
        // intrinsic width, instead of pretending that it's 1. The only way to do that is by
        // performing a layout pass, but this is not an appropriate time or place for layout. The
        // good news is that if height is unconstrained and there are no explicit breaks, the
        // resolved column-count really should be 1.
        maxLogicalWidth = max(maxLogicalWidth, columnWidth) * columnCount + gapExtra;
    }
}

void RenderBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    RenderStyle* styleToUse = style();
    bool nowrap = styleToUse->whiteSpace() == NOWRAP;

    RenderObject* child = firstChild();
    RenderBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth = 0, floatRightWidth = 0;
    while (child) {
        // Positioned children don't affect the min/max width
        if (child->isOutOfFlowPositioned()) {
            child = child->nextSibling();
            continue;
        }

        RenderStyle* childStyle = child->style();
        if (child->isFloating() || (child->isBox() && toRenderBox(child)->avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (childStyle->clear() & CLEFT) {
                maxLogicalWidth = max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = 0;
            }
            if (childStyle->clear() & CRIGHT) {
                maxLogicalWidth = max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = 0;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle->marginStartUsing(styleToUse);
        Length endMarginLength = childStyle->marginEndUsing(styleToUse);
        LayoutUnit margin = 0;
        LayoutUnit marginStart = 0;
        LayoutUnit marginEnd = 0;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        if (child->isBox() && child->isHorizontalWritingMode() != isHorizontalWritingMode()) {
            RenderBox* childBox = toRenderBox(child);
            LogicalExtentComputedValues computedValues;
            childBox->computeLogicalHeight(childBox->borderAndPaddingLogicalHeight(), 0, computedValues);
            childMinPreferredLogicalWidth = childMaxPreferredLogicalWidth = computedValues.m_extent;
        } else {
            childMinPreferredLogicalWidth = child->minPreferredLogicalWidth();
            childMaxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
        }

        LayoutUnit w = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = max(w, minLogicalWidth);

        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            maxLogicalWidth = max(w, maxLogicalWidth);

        w = childMaxPreferredLogicalWidth + margin;

        if (!child->isFloating()) {
            if (child->isBox() && toRenderBox(child)->avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                bool ltr = containingBlock ? containingBlock->style()->isLeftToRightDirection() : styleToUse->isLeftToRightDirection();
                LayoutUnit marginLogicalLeft = ltr ? marginStart : marginEnd;
                LayoutUnit marginLogicalRight = ltr ? marginEnd : marginStart;
                LayoutUnit maxLeft = marginLogicalLeft > 0 ? max(floatLeftWidth, marginLogicalLeft) : floatLeftWidth + marginLogicalLeft;
                LayoutUnit maxRight = marginLogicalRight > 0 ? max(floatRightWidth, marginLogicalRight) : floatRightWidth + marginLogicalRight;
                w = childMaxPreferredLogicalWidth + maxLeft + maxRight;
                w = max(w, floatLeftWidth + floatRightWidth);
            }
            else
                maxLogicalWidth = max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
            floatLeftWidth = floatRightWidth = 0;
        }

        if (child->isFloating()) {
            if (childStyle->floating() == LeftFloat)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        } else
            maxLogicalWidth = max(w, maxLogicalWidth);

        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    minLogicalWidth = max<LayoutUnit>(0, minLogicalWidth);
    maxLogicalWidth = max<LayoutUnit>(0, maxLogicalWidth);

    maxLogicalWidth = max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
}

bool RenderBlock::hasLineIfEmpty() const
{
    if (!node())
        return false;

    if (node()->isRootEditableElement())
        return true;

    if (node()->isShadowRoot() && isHTMLInputElement(*toShadowRoot(node())->host()))
        return true;

    return false;
}

LayoutUnit RenderBlock::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && linePositionMode == PositionOnContainingLine)
        return RenderBox::lineHeight(firstLine, direction, linePositionMode);

    RenderStyle* s = style(firstLine && document().styleEngine()->usesFirstLineRules());
    return s->computedLineHeight();
}

int RenderBlock::beforeMarginInLineDirection(LineDirectionMode direction) const
{
    return direction == HorizontalLine ? marginTop() : marginRight();
}

int RenderBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isInline() && linePositionMode == PositionOnContainingLine) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style()->hasAppearance() && !RenderTheme::theme().isControlContainer(style()->appearance()))
            return RenderTheme::theme().baselinePosition(this);

        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        bool ignoreBaseline = (layer() && layer()->scrollableArea() && (isMarquee() || (direction == HorizontalLine ? (layer()->scrollableArea()->verticalScrollbar() || layer()->scrollableArea()->scrollYOffset())
            : (layer()->scrollableArea()->horizontalScrollbar() || layer()->scrollableArea()->scrollXOffset())))) || (isWritingModeRoot() && !isRubyRun());

        int baselinePos = ignoreBaseline ? -1 : inlineBlockBaseline(direction);

        if (isDeprecatedFlexibleBox()) {
            // Historically, we did this check for all baselines. But we can't
            // remove this code from deprecated flexbox, because it effectively
            // breaks -webkit-line-clamp, which is used in the wild -- we would
            // calculate the baseline as if -webkit-line-clamp wasn't used.
            // For simplicity, we use this for all uses of deprecated flexbox.
            LayoutUnit bottomOfContent = direction == HorizontalLine ? borderTop() + paddingTop() + contentHeight() : borderRight() + paddingRight() + contentWidth();
            if (baselinePos > bottomOfContent)
                baselinePos = -1;
        }
        if (baselinePos != -1)
            return beforeMarginInLineDirection(direction) + baselinePos;

        return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    }

    // If we're not replaced, we'll only get called with PositionOfInteriorLineBoxes.
    // Note that inline-block counts as replaced here.
    ASSERT(linePositionMode == PositionOfInteriorLineBoxes);

    const FontMetrics& fontMetrics = style(firstLine)->fontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

LayoutUnit RenderBlock::minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    if (!(style(isFirstLine)->lineBoxContain() & LineBoxContainBlock))
        return 0;

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

int RenderBlock::firstLineBoxBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (firstLineBox())
            return firstLineBox()->logicalTop() + style(true)->fontMetrics().ascent(firstRootBox()->baselineType());
        else
            return -1;
    }
    else {
        for (RenderBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
            if (!curr->isFloatingOrOutOfFlowPositioned()) {
                int result = curr->firstLineBoxBaseline();
                if (result != -1)
                    return curr->logicalTop() + result; // Translate to our coordinate space.
            }
        }
    }

    return -1;
}

int RenderBlock::inlineBlockBaseline(LineDirectionMode direction) const
{
    if (!style()->isOverflowVisible()) {
        // We are not calling RenderBox::baselinePosition here because the caller should add the margin-top/margin-right, not us.
        return direction == HorizontalLine ? height() + m_marginBox.bottom() : width() + m_marginBox.left();
    }

    return lastLineBoxBaseline(direction);
}

int RenderBlock::lastLineBoxBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (childrenInline()) {
        if (!firstLineBox() && hasLineIfEmpty()) {
            const FontMetrics& fontMetrics = firstLineStyle()->fontMetrics();
            return fontMetrics.ascent()
                 + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                 + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
        }
        if (lastLineBox())
            return lastLineBox()->logicalTop() + style(lastLineBox() == firstLineBox())->fontMetrics().ascent(lastRootBox()->baselineType());
        return -1;
    } else {
        bool haveNormalFlowChild = false;
        for (RenderBox* curr = lastChildBox(); curr; curr = curr->previousSiblingBox()) {
            if (!curr->isFloatingOrOutOfFlowPositioned()) {
                haveNormalFlowChild = true;
                int result = curr->inlineBlockBaseline(lineDirection);
                if (result != -1)
                    return curr->logicalTop() + result; // Translate to our coordinate space.
            }
        }
        if (!haveNormalFlowChild && hasLineIfEmpty()) {
            const FontMetrics& fontMetrics = firstLineStyle()->fontMetrics();
            return fontMetrics.ascent()
                 + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                 + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
        }
    }

    return -1;
}

RenderBlock* RenderBlock::firstLineBlock() const
{
    RenderBlock* firstLineBlock = const_cast<RenderBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(FIRST_LINE);
        if (hasPseudo)
            break;
        RenderObject* parentBlock = firstLineBlock->parent();
        // We include isRenderButton in this check because buttons are
        // implemented using flex box but should still support first-line. The
        // flex box spec requires that flex box does not support first-line,
        // though.
        // FIXME: Remove when buttons are implemented with align-items instead
        // of flexbox.
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloating()
            || !parentBlock
            || (!parentBlock->isRenderBlockFlow() && !parentBlock->isRenderButton()))
            break;
        ASSERT_WITH_SECURITY_IMPLICATION(parentBlock->isRenderBlock());
        if (toRenderBlock(parentBlock)->firstChild() != firstLineBlock)
            break;
        firstLineBlock = toRenderBlock(parentBlock);
    }

    if (!hasPseudo)
        return 0;

    return firstLineBlock;
}

static RenderStyle* styleForFirstLetter(RenderObject* firstLetterBlock, RenderObject* firstLetterContainer)
{
    RenderStyle* pseudoStyle = firstLetterBlock->getCachedPseudoStyle(FIRST_LETTER, firstLetterContainer->firstLineStyle());
    // Force inline display (except for floating first-letters).
    pseudoStyle->setDisplay(pseudoStyle->isFloating() ? BLOCK : INLINE);
    // CSS2 says first-letter can't be positioned.
    pseudoStyle->setPosition(StaticPosition);
    return pseudoStyle;
}

// CSS 2.1 http://www.w3.org/TR/CSS21/selector.html#first-letter
// "Punctuation (i.e, characters defined in Unicode [UNICODE] in the "open" (Ps), "close" (Pe),
// "initial" (Pi). "final" (Pf) and "other" (Po) punctuation classes), that precedes or follows the first letter should be included"
static inline bool isPunctuationForFirstLetter(UChar c)
{
    CharCategory charCategory = category(c);
    return charCategory == Punctuation_Open
        || charCategory == Punctuation_Close
        || charCategory == Punctuation_InitialQuote
        || charCategory == Punctuation_FinalQuote
        || charCategory == Punctuation_Other;
}

static inline bool isSpaceForFirstLetter(UChar c)
{
    return isSpaceOrNewline(c) || c == noBreakSpace;
}

static inline RenderObject* findFirstLetterBlock(RenderBlock* start)
{
    RenderObject* firstLetterBlock = start;
    while (true) {
        // We include isRenderButton in these two checks because buttons are
        // implemented using flex box but should still support first-letter.
        // The flex box spec requires that flex box does not support
        // first-letter, though.
        // FIXME: Remove when buttons are implemented with align-items instead
        // of flexbox.
        bool canHaveFirstLetterRenderer = firstLetterBlock->style()->hasPseudoStyle(FIRST_LETTER)
            && firstLetterBlock->canHaveGeneratedChildren()
            && (!firstLetterBlock->isFlexibleBox() || firstLetterBlock->isRenderButton());
        if (canHaveFirstLetterRenderer)
            return firstLetterBlock;

        RenderObject* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplaced() || !parentBlock
            || (!parentBlock->isRenderBlockFlow() && !parentBlock->isRenderButton())) {
            return 0;
        }
        ASSERT(parentBlock->isRenderBlock());
        if (toRenderBlock(parentBlock)->firstChild() != firstLetterBlock)
            return 0;
        firstLetterBlock = parentBlock;
    }

    return 0;
}

void RenderBlock::updateFirstLetterStyle(RenderObject* firstLetterBlock, RenderObject* currentChild)
{
    RenderObject* firstLetter = currentChild->parent();
    RenderObject* firstLetterContainer = firstLetter->parent();
    RenderStyle* pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    ASSERT(firstLetter->isFloating() || firstLetter->isInline());

    ForceHorriblySlowRectMapping slowRectMapping(*this);

    if (RenderStyle::stylePropagationDiff(firstLetter->style(), pseudoStyle) == Reattach) {
        // The first-letter renderer needs to be replaced. Create a new renderer of the right type.
        RenderBoxModelObject* newFirstLetter;
        if (pseudoStyle->display() == INLINE)
            newFirstLetter = RenderInline::createAnonymous(&document());
        else
            newFirstLetter = RenderBlockFlow::createAnonymous(&document());
        newFirstLetter->setStyle(pseudoStyle);

        // Move the first letter into the new renderer.
        while (RenderObject* child = firstLetter->slowFirstChild()) {
            if (child->isText())
                toRenderText(child)->removeAndDestroyTextBoxes();
            firstLetter->removeChild(child);
            newFirstLetter->addChild(child, 0);
        }

        RenderObject* nextSibling = firstLetter->nextSibling();
        if (RenderTextFragment* remainingText = toRenderBoxModelObject(firstLetter)->firstLetterRemainingText()) {
            ASSERT(remainingText->isAnonymous() || remainingText->node()->renderer() == remainingText);
            // Replace the old renderer with the new one.
            remainingText->setFirstLetter(newFirstLetter);
            newFirstLetter->setFirstLetterRemainingText(remainingText);
        }
        // To prevent removal of single anonymous block in RenderBlock::removeChild and causing
        // |nextSibling| to go stale, we remove the old first letter using removeChildNode first.
        firstLetterContainer->virtualChildren()->removeChildNode(firstLetterContainer, firstLetter);
        firstLetter->destroy();
        firstLetter = newFirstLetter;
        firstLetterContainer->addChild(firstLetter, nextSibling);
    } else
        firstLetter->setStyle(pseudoStyle);

    for (RenderObject* genChild = firstLetter->slowFirstChild(); genChild; genChild = genChild->nextSibling()) {
        if (genChild->isText())
            genChild->setStyle(pseudoStyle);
    }
}

static inline unsigned firstLetterLength(const String& text)
{
    unsigned length = 0;
    unsigned textLength = text.length();

    // Account for leading spaces first.
    while (length < textLength && isSpaceForFirstLetter(text[length]))
        length++;

    // Now account for leading punctuation.
    while (length < textLength && isPunctuationForFirstLetter(text[length]))
        length++;

    // Bail if we didn't find a letter before the end of the text or before a space.
    if (isSpaceForFirstLetter(text[length]) || (textLength && length == textLength))
        return 0;

    // Account the next character for first letter.
    length++;

    // Keep looking allowed punctuation for the :first-letter.
    for (unsigned scanLength = length; scanLength < textLength; ++scanLength) {
        UChar c = text[scanLength];

        if (!isPunctuationForFirstLetter(c))
            break;

        length = scanLength + 1;
    }

    // FIXME: If textLength is 0, length may still be 1!
    return length;
}

void RenderBlock::createFirstLetterRenderer(RenderObject* firstLetterBlock, RenderObject* currentChild, unsigned length)
{
    ASSERT(length && currentChild->isText());

    RenderObject* firstLetterContainer = currentChild->parent();
    RenderStyle* pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    RenderBoxModelObject* firstLetter = 0;
    if (pseudoStyle->display() == INLINE)
        firstLetter = RenderInline::createAnonymous(&document());
    else
        firstLetter = RenderBlockFlow::createAnonymous(&document());
    firstLetter->setStyle(pseudoStyle);

    // FIXME: The first letter code should not modify the render tree during
    // layout. crbug.com/370458
    DeprecatedDisableModifyRenderTreeStructureAsserts disabler;

    firstLetterContainer->addChild(firstLetter, currentChild);
    RenderText* textObj = toRenderText(currentChild);

    // The original string is going to be either a generated content string or a DOM node's
    // string.  We want the original string before it got transformed in case first-letter has
    // no text-transform or a different text-transform applied to it.
    String oldText = textObj->originalText();
    ASSERT(oldText.impl());

    // Construct a text fragment for the text after the first letter.
    // This text fragment might be empty.
    RenderTextFragment* remainingText =
        new RenderTextFragment(textObj->node() ? textObj->node() : &textObj->document(), oldText.impl(), length, oldText.length() - length);
    remainingText->setStyle(textObj->style());
    if (remainingText->node())
        remainingText->node()->setRenderer(remainingText);

    firstLetterContainer->addChild(remainingText, textObj);
    firstLetterContainer->removeChild(textObj);
    remainingText->setFirstLetter(firstLetter);
    firstLetter->setFirstLetterRemainingText(remainingText);

    // construct text fragment for the first letter
    RenderTextFragment* letter =
        new RenderTextFragment(remainingText->node() ? remainingText->node() : &remainingText->document(), oldText.impl(), 0, length);
    letter->setStyle(pseudoStyle);
    firstLetter->addChild(letter);

    textObj->destroy();
}

void RenderBlock::updateFirstLetter()
{
    if (!document().styleEngine()->usesFirstLetterRules())
        return;
    // Don't recur
    if (style()->styleType() == FIRST_LETTER)
        return;

    // FIXME: We need to destroy the first-letter object if it is no longer the first child. Need to find
    // an efficient way to check for that situation though before implementing anything.
    RenderObject* firstLetterBlock = findFirstLetterBlock(this);
    if (!firstLetterBlock)
        return;

    // Drill into inlines looking for our first text child.
    RenderObject* currChild = firstLetterBlock->slowFirstChild();
    unsigned length = 0;
    while (currChild) {
        if (currChild->isText()) {
            // FIXME: If there is leading punctuation in a different RenderText than
            // the first letter, we'll not apply the correct style to it.
            length = firstLetterLength(toRenderText(currChild)->originalText());
            if (length)
                break;
            currChild = currChild->nextSibling();
        } else if (currChild->isListMarker()) {
            currChild = currChild->nextSibling();
        } else if (currChild->isFloatingOrOutOfFlowPositioned()) {
            if (currChild->style()->styleType() == FIRST_LETTER) {
                currChild = currChild->slowFirstChild();
                break;
            }
            currChild = currChild->nextSibling();
        } else if (currChild->isReplaced() || currChild->isRenderButton() || currChild->isMenuList()) {
            break;
        } else if (currChild->style()->hasPseudoStyle(FIRST_LETTER) && currChild->canHaveGeneratedChildren())  {
            // We found a lower-level node with first-letter, which supersedes the higher-level style
            firstLetterBlock = currChild;
            currChild = currChild->slowFirstChild();
        } else {
            currChild = currChild->slowFirstChild();
        }
    }

    if (!currChild)
        return;

    // If the child already has style, then it has already been created, so we just want
    // to update it.
    if (currChild->parent()->style()->styleType() == FIRST_LETTER) {
        updateFirstLetterStyle(firstLetterBlock, currChild);
        return;
    }

    // FIXME: This black-list of disallowed RenderText subclasses is fragile.
    // Should counter be on this list? What about RenderTextFragment?
    if (!currChild->isText() || currChild->isBR() || toRenderText(currChild)->isWordBreak())
        return;

    // Our layout state is not valid for the repaints we are going to trigger by
    // adding and removing children of firstLetterContainer.
    ForceHorriblySlowRectMapping slowRectMapping(*this);

    createFirstLetterRenderer(firstLetterBlock, currChild, length);
}

// Helper methods for obtaining the last line, computing line counts and heights for line counts
// (crawling into blocks).
static bool shouldCheckLines(RenderObject* obj)
{
    return !obj->isFloatingOrOutOfFlowPositioned()
        && obj->isRenderBlock() && obj->style()->height().isAuto()
        && (!obj->isDeprecatedFlexibleBox() || obj->style()->boxOrient() == VERTICAL);
}

static int getHeightForLineCount(RenderBlock* block, int l, bool includeBottom, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->childrenInline()) {
            for (RootInlineBox* box = block->firstRootBox(); box; box = box->nextRootBox()) {
                if (++count == l)
                    return box->lineBottom() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
            }
        }
        else {
            RenderBox* normalFlowChildWithoutLines = 0;
            for (RenderBox* obj = block->firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (shouldCheckLines(obj)) {
                    int result = getHeightForLineCount(toRenderBlock(obj), l, false, count);
                    if (result != -1)
                        return result + obj->y() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : LayoutUnit());
                } else if (!obj->isFloatingOrOutOfFlowPositioned())
                    normalFlowChildWithoutLines = obj;
            }
            if (normalFlowChildWithoutLines && l == 0)
                return normalFlowChildWithoutLines->y() + normalFlowChildWithoutLines->height();
        }
    }

    return -1;
}

RootInlineBox* RenderBlock::lineAtIndex(int i) const
{
    ASSERT(i >= 0);

    if (style()->visibility() != VISIBLE)
        return 0;

    if (childrenInline()) {
        for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
            if (!i--)
                return box;
    } else {
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (!shouldCheckLines(child))
                continue;
            if (RootInlineBox* box = toRenderBlock(child)->lineAtIndex(i))
                return box;
        }
    }

    return 0;
}

int RenderBlock::lineCount(const RootInlineBox* stopRootInlineBox, bool* found) const
{
    int count = 0;

    if (style()->visibility() == VISIBLE) {
        if (childrenInline())
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
                count++;
                if (box == stopRootInlineBox) {
                    if (found)
                        *found = true;
                    break;
                }
            }
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (shouldCheckLines(obj)) {
                    bool recursiveFound = false;
                    count += toRenderBlock(obj)->lineCount(stopRootInlineBox, &recursiveFound);
                    if (recursiveFound) {
                        if (found)
                            *found = true;
                        break;
                    }
                }
    }
    return count;
}

int RenderBlock::heightForLineCount(int l)
{
    int count = 0;
    return getHeightForLineCount(this, l, true, count);
}

void RenderBlock::adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const
{
    // We don't deal with relative positioning.  Our assumption is that you shrink to fit the lines without accounting
    // for either overflow or translations via relative positioning.
    if (style()->visibility() == VISIBLE) {
        if (childrenInline()) {
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
                if (box->firstChild())
                    left = min(left, x + static_cast<LayoutUnit>(box->firstChild()->x()));
                if (box->lastChild())
                    right = max(right, x + static_cast<LayoutUnit>(ceilf(box->lastChild()->logicalRight())));
            }
        } else {
            for (RenderBox* obj = firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (!obj->isFloatingOrOutOfFlowPositioned()) {
                    if (obj->isRenderBlockFlow() && !obj->hasOverflowClip())
                        toRenderBlock(obj)->adjustForBorderFit(x + obj->x(), left, right);
                    else if (obj->style()->visibility() == VISIBLE) {
                        // We are a replaced element or some kind of non-block-flow object.
                        left = min(left, x + obj->x());
                        right = max(right, x + obj->x() + obj->width());
                    }
                }
            }
        }
    }
}

void RenderBlock::fitBorderToLinesIfNeeded()
{
    if (style()->borderFit() == BorderFitBorder || hasOverrideWidth())
        return;

    // Walk any normal flow lines to snugly fit.
    LayoutUnit left = LayoutUnit::max();
    LayoutUnit right = LayoutUnit::min();
    LayoutUnit oldWidth = contentWidth();
    adjustForBorderFit(0, left, right);

    // Clamp to our existing edges. We can never grow. We only shrink.
    LayoutUnit leftEdge = borderLeft() + paddingLeft();
    LayoutUnit rightEdge = leftEdge + oldWidth;
    left = min(rightEdge, max(leftEdge, left));
    right = max(left, min(rightEdge, right));

    LayoutUnit newContentWidth = right - left;
    if (newContentWidth == oldWidth)
        return;

    setOverrideLogicalContentWidth(newContentWidth);
    layoutBlock(false);
    clearOverrideLogicalContentWidth();
}

void RenderBlock::clearTruncation()
{
    if (style()->visibility() == VISIBLE) {
        if (childrenInline() && hasMarkupTruncation()) {
            setHasMarkupTruncation(false);
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                box->clearTruncation();
        } else {
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj))
                    toRenderBlock(obj)->clearTruncation();
            }
        }
    }
}

void RenderBlock::setPaginationStrut(LayoutUnit strut)
{
    if (!m_rareData) {
        if (!strut)
            return;
        m_rareData = adoptPtr(new RenderBlockRareData());
    }
    m_rareData->m_paginationStrut = strut;
}

void RenderBlock::setPageLogicalOffset(LayoutUnit logicalOffset)
{
    if (!m_rareData) {
        if (!logicalOffset)
            return;
        m_rareData = adoptPtr(new RenderBlockRareData());
    }
    m_rareData->m_pageLogicalOffset = logicalOffset;
}

void RenderBlock::setBreakAtLineToAvoidWidow(int lineToBreak)
{
    ASSERT(lineToBreak >= 0);
    if (!m_rareData)
        m_rareData = adoptPtr(new RenderBlockRareData());

    ASSERT(!m_rareData->m_didBreakAtLineToAvoidWidow);
    m_rareData->m_lineBreakToAvoidWidow = lineToBreak;
}

void RenderBlock::setDidBreakAtLineToAvoidWidow()
{
    ASSERT(!shouldBreakAtLineToAvoidWidow());

    // This function should be called only after a break was applied to avoid widows
    // so assert |m_rareData| exists.
    ASSERT(m_rareData);

    m_rareData->m_didBreakAtLineToAvoidWidow = true;
}

void RenderBlock::clearDidBreakAtLineToAvoidWidow()
{
    if (!m_rareData)
        return;

    m_rareData->m_didBreakAtLineToAvoidWidow = false;
}

void RenderBlock::clearShouldBreakAtLineToAvoidWidow() const
{
    ASSERT(shouldBreakAtLineToAvoidWidow());
    if (!m_rareData)
        return;

    m_rareData->m_lineBreakToAvoidWidow = -1;
}

void RenderBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        rects.append(pixelSnappedIntRect(accumulatedOffset.x(), accumulatedOffset.y() - collapsedMarginBefore(),
                                width(), height() + collapsedMarginBefore() + collapsedMarginAfter()));
        continuation()->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() +
                inlineElementContinuation()->containingBlock()->location()));
    } else
        rects.append(pixelSnappedIntRect(accumulatedOffset, size()));
}

void RenderBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        FloatRect localRect(0, -collapsedMarginBefore().toFloat(),
            width().toFloat(), (height() + collapsedMarginBefore() + collapsedMarginAfter()).toFloat());
        quads.append(localToAbsoluteQuad(localRect, 0 /* mode */, wasFixed));
        continuation()->absoluteQuads(quads, wasFixed);
    } else {
        quads.append(RenderBox::localToAbsoluteQuad(FloatRect(0, 0, width().toFloat(), height().toFloat()), 0 /* mode */, wasFixed));
    }
}

LayoutRect RenderBlock::rectWithOutlineForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBox::rectWithOutlineForPaintInvalidation(paintInvalidationContainer, outlineWidth));
    if (isAnonymousBlockContinuation())
        r.inflateY(collapsedMarginBefore()); // FIXME: This is wrong for block-flows that are horizontal.
    return r;
}

RenderObject* RenderBlock::hoverAncestor() const
{
    return isAnonymousBlockContinuation() ? continuation() : RenderBox::hoverAncestor();
}

void RenderBlock::updateDragState(bool dragOn)
{
    RenderBox::updateDragState(dragOn);
    if (continuation())
        continuation()->updateDragState(dragOn);
}

RenderStyle* RenderBlock::outlineStyleForPaintInvalidation() const
{
    return isAnonymousBlockContinuation() ? continuation()->style() : style();
}

void RenderBlock::childBecameNonInline(RenderObject*)
{
    makeChildrenNonInline();
    if (isAnonymousBlock() && parent() && parent()->isRenderBlock())
        toRenderBlock(parent())->removeLeftoverAnonymousBlock(this);
    // |this| may be dead here
}

void RenderBlock::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (Node* n = nodeForHitTest()) {
        result.setInnerNode(n);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(n);
        result.setLocalPoint(point);
    }
}

LayoutRect RenderBlock::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // Do the normal calculation in most cases.
    if (firstChild())
        return RenderBox::localCaretRect(inlineBox, caretOffset, extraWidthToEndOfLine);

    LayoutRect caretRect = localCaretRectForEmptyElement(width(), textIndentOffset());

    if (extraWidthToEndOfLine) {
        if (isRenderBlock()) {
            *extraWidthToEndOfLine = width() - caretRect.maxX();
        } else {
            // FIXME: This code looks wrong.
            // myRight and containerRight are set up, but then clobbered.
            // So *extraWidthToEndOfLine will always be 0 here.

            LayoutUnit myRight = caretRect.maxX();
            // FIXME: why call localToAbsoluteForContent() twice here, too?
            FloatPoint absRightPoint = localToAbsolute(FloatPoint(myRight.toFloat(), 0));

            LayoutUnit containerRight = containingBlock()->x() + containingBlockLogicalWidthForContent();
            FloatPoint absContainerPoint = localToAbsolute(FloatPoint(containerRight.toFloat(), 0));

            *extraWidthToEndOfLine = absContainerPoint.x() - absRightPoint.x();
        }
    }

    return caretRect;
}

void RenderBlock::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (inlineElementContinuation()) {
        // FIXME: This check really isn't accurate.
        bool nextInlineHasLineBox = inlineElementContinuation()->firstLineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = toRenderInline(inlineElementContinuation()->node()->renderer())->firstLineBox();
        LayoutUnit topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        LayoutUnit bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        LayoutRect rect(additionalOffset.x(), additionalOffset.y() - topMargin, width(), height() + topMargin + bottomMargin);
        if (!rect.isEmpty())
            rects.append(pixelSnappedIntRect(rect));
    } else if (width() && height())
        rects.append(pixelSnappedIntRect(additionalOffset, size()));

    if (!hasOverflowClip() && !hasControlClip()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
            if (!rect.isEmpty())
                rects.append(pixelSnappedIntRect(rect));
        }

        addChildFocusRingRects(rects, additionalOffset, paintContainer);
    }

    if (inlineElementContinuation())
        inlineElementContinuation()->addFocusRingRects(rects, flooredLayoutPoint(additionalOffset + inlineElementContinuation()->containingBlock()->location() - location()), paintContainer);
}

void RenderBlock::computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const
{
    RenderBox::computeSelfHitTestRects(rects, layerOffset);

    if (hasHorizontalLayoutOverflow() || hasVerticalLayoutOverflow()) {
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            LayoutUnit top = max<LayoutUnit>(curr->lineTop(), curr->top());
            LayoutUnit bottom = min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
            LayoutRect rect(layerOffset.x() + curr->x(), layerOffset.y() + top, curr->width(), bottom - top);
            // It's common for this rect to be entirely contained in our box, so exclude that simple case.
            if (!rect.isEmpty() && (rects.isEmpty() || !rects[0].contains(rect)))
                rects.append(rect);
        }
    }
}

RenderBox* RenderBlock::createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const
{
    if (isAnonymousColumnsBlock())
        return createAnonymousColumnsWithParentRenderer(parent);
    if (isAnonymousColumnSpanBlock())
        return createAnonymousColumnSpanWithParentRenderer(parent);
    return createAnonymousWithParentRendererAndDisplay(parent, style()->display());
}

LayoutUnit RenderBlock::nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;

    // The logicalOffset is in our coordinate space.  We can add in our pushed offset.
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset);
    if (pageBoundaryRule == ExcludePageBoundary)
        return logicalOffset + (remainingLogicalHeight ? remainingLogicalHeight : pageLogicalHeight);
    return logicalOffset + remainingLogicalHeight;
}

LayoutUnit RenderBlock::pageLogicalTopForOffset(LayoutUnit offset) const
{
    RenderView* renderView = view();
    LayoutUnit firstPageLogicalTop = isHorizontalWritingMode() ? renderView->layoutState()->pageOffset().height() : renderView->layoutState()->pageOffset().width();
    LayoutUnit blockLogicalTop = isHorizontalWritingMode() ? renderView->layoutState()->layoutOffset().height() : renderView->layoutState()->layoutOffset().width();

    LayoutUnit cumulativeOffset = offset + blockLogicalTop;
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = renderView->layoutState()->pageLogicalHeight();
        if (!pageLogicalHeight)
            return 0;
        return cumulativeOffset - roundToInt(cumulativeOffset - firstPageLogicalTop) % roundToInt(pageLogicalHeight);
    }
    return flowThread->pageLogicalTopForOffset(cumulativeOffset);
}

LayoutUnit RenderBlock::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    RenderView* renderView = view();
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread)
        return renderView->layoutState()->pageLogicalHeight();
    return flowThread->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
}

LayoutUnit RenderBlock::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    RenderView* renderView = view();
    offset += offsetFromLogicalTopOfFirstPage();

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = renderView->layoutState()->pageLogicalHeight();
        LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
        if (pageBoundaryRule == IncludePageBoundary) {
            // If includeBoundaryPoint is true the line exactly on the top edge of a
            // column will act as being part of the previous column.
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }

    return flowThread->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}

LayoutUnit RenderBlock::adjustForUnsplittableChild(RenderBox* child, LayoutUnit logicalOffset, bool includeMargins)
{
    bool checkColumnBreaks = view()->layoutState()->isPaginatingColumns() || flowThreadContainingBlock();
    bool checkPageBreaks = !checkColumnBreaks && view()->layoutState()->pageLogicalHeight();
    bool isUnsplittable = child->isUnsplittableForPagination() || (checkColumnBreaks && child->style()->columnBreakInside() == PBAVOID)
        || (checkPageBreaks && child->style()->pageBreakInside() == PBAVOID);
    if (!isUnsplittable)
        return logicalOffset;
    LayoutUnit childLogicalHeight = logicalHeightForChild(child) + (includeMargins ? marginBeforeForChild(child) + marginAfterForChild(child) : LayoutUnit());
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    updateMinimumPageHeight(logicalOffset, childLogicalHeight);
    if (!pageLogicalHeight || childLogicalHeight > pageLogicalHeight)
        return logicalOffset;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);
    if (remainingLogicalHeight < childLogicalHeight)
        return logicalOffset + remainingLogicalHeight;
    return logicalOffset;
}

void RenderBlock::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->setPageBreak(offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void RenderBlock::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->updateMinimumPageHeight(offsetFromLogicalTopOfFirstPage() + offset, minHeight);
    else if (ColumnInfo* colInfo = view()->layoutState()->columnInfo())
        colInfo->updateMinimumColumnHeight(minHeight);
}

static inline LayoutUnit calculateMinimumPageHeight(RenderStyle* renderStyle, RootInlineBox* lastLine, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // We may require a certain minimum number of lines per page in order to satisfy
    // orphans and widows, and that may affect the minimum page height.
    unsigned lineCount = max<unsigned>(renderStyle->hasAutoOrphans() ? 1 : renderStyle->orphans(), renderStyle->hasAutoWidows() ? 1 : renderStyle->widows());
    if (lineCount > 1) {
        RootInlineBox* line = lastLine;
        for (unsigned i = 1; i < lineCount && line->prevRootBox(); i++)
            line = line->prevRootBox();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        LayoutRect overflow = line->logicalVisualOverflowRect(line->lineTop(), line->lineBottom());
        lineTop = min(line->lineTopWithLeading(), overflow.y());
    }
    return lineBottom - lineTop;
}

void RenderBlock::adjustLinePositionForPagination(RootInlineBox* lineBox, LayoutUnit& delta, RenderFlowThread* flowThread)
{
    // FIXME: For now we paginate using line overflow.  This ensures that lines don't overlap at all when we
    // put a strut between them for pagination purposes.  However, this really isn't the desired rendering, since
    // the line on the top of the next page will appear too far down relative to the same kind of line at the top
    // of the first column.
    //
    // The rendering we would like to see is one where the lineTopWithLeading is at the top of the column, and any line overflow
    // simply spills out above the top of the column.  This effect would match what happens at the top of the first column.
    // We can't achieve this rendering, however, until we stop columns from clipping to the column bounds (thus allowing
    // for overflow to occur), and then cache visible overflow for each column rect.
    //
    // Furthermore, the paint we have to do when a column has overflow has to be special.  We need to exclude
    // content that paints in a previous column (and content that paints in the following column).
    //
    // For now we'll at least honor the lineTopWithLeading when paginating if it is above the logical top overflow. This will
    // at least make positive leading work in typical cases.
    //
    // FIXME: Another problem with simply moving lines is that the available line width may change (because of floats).
    // Technically if the location we move the line to has a different line width than our old position, then we need to dirty the
    // line and all following lines.
    LayoutRect logicalVisualOverflow = lineBox->logicalVisualOverflowRect(lineBox->lineTop(), lineBox->lineBottom());
    LayoutUnit logicalOffset = min(lineBox->lineTopWithLeading(), logicalVisualOverflow.y());
    LayoutUnit logicalBottom = max(lineBox->lineBottomWithLeading(), logicalVisualOverflow.maxY());
    LayoutUnit lineHeight = logicalBottom - logicalOffset;
    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(style(), lineBox, logicalOffset, logicalBottom));
    logicalOffset += delta;
    lineBox->setPaginationStrut(0);
    lineBox->setIsFirstAfterPageBreak(false);
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    bool hasUniformPageLogicalHeight = !flowThread || flowThread->regionsHaveUniformLogicalHeight();
    // If lineHeight is greater than pageLogicalHeight, but logicalVisualOverflow.height() still fits, we are
    // still going to add a strut, so that the visible overflow fits on a single page.
    if (!pageLogicalHeight || (hasUniformPageLogicalHeight && logicalVisualOverflow.height() > pageLogicalHeight))
        // FIXME: In case the line aligns with the top of the page (or it's slightly shifted downwards) it will not be marked as the first line in the page.
        // From here, the fix is not straightforward because it's not easy to always determine when the current line is the first in the page.
        return;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);

    int lineIndex = lineCount(lineBox);
    if (remainingLogicalHeight < lineHeight || (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex)) {
        if (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex) {
            clearShouldBreakAtLineToAvoidWidow();
            setDidBreakAtLineToAvoidWidow();
        }
        if (lineHeight > pageLogicalHeight) {
            // Split the top margin in order to avoid splitting the visible part of the line.
            remainingLogicalHeight -= min(lineHeight - pageLogicalHeight, max<LayoutUnit>(0, logicalVisualOverflow.y() - lineBox->lineTopWithLeading()));
        }
        LayoutUnit totalLogicalHeight = lineHeight + max<LayoutUnit>(0, logicalOffset);
        LayoutUnit pageLogicalHeightAtNewOffset = hasUniformPageLogicalHeight ? pageLogicalHeight : pageLogicalHeightForOffset(logicalOffset + remainingLogicalHeight);
        setPageBreak(logicalOffset, lineHeight - remainingLogicalHeight);
        if (((lineBox == firstRootBox() && totalLogicalHeight < pageLogicalHeightAtNewOffset) || (!style()->hasAutoOrphans() && style()->orphans() >= lineIndex))
            && !isOutOfFlowPositioned() && !isTableCell())
            setPaginationStrut(remainingLogicalHeight + max<LayoutUnit>(0, logicalOffset));
        else {
            delta += remainingLogicalHeight;
            lineBox->setPaginationStrut(remainingLogicalHeight);
            lineBox->setIsFirstAfterPageBreak(true);
        }
    } else if (remainingLogicalHeight == pageLogicalHeight) {
        // We're at the very top of a page or column.
        if (lineBox != firstRootBox())
            lineBox->setIsFirstAfterPageBreak(true);
        if (lineBox != firstRootBox() || offsetFromLogicalTopOfFirstPage())
            setPageBreak(logicalOffset, lineHeight);
    }
}

LayoutUnit RenderBlock::offsetFromLogicalTopOfFirstPage() const
{
    LayoutState* layoutState = view()->layoutState();
    if (layoutState && !layoutState->isPaginated())
        return 0;

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread)
        return flowThread->offsetFromLogicalTopOfFirstRegion(this);

    if (layoutState) {
        ASSERT(layoutState->renderer() == this);

        LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
        return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderBlock::collapsedMarginBeforeForChild(const RenderBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child->isWritingModeRoot())
        return child->collapsedMarginBefore();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->collapsedMarginAfter();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" sides of the child box.  We can just return the raw margin in this case.
    return marginBeforeForChild(child);
}

LayoutUnit RenderBlock::collapsedMarginAfterForChild(const  RenderBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child->isWritingModeRoot())
        return child->collapsedMarginAfter();

    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->collapsedMarginBefore();

    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" side of the child box.  We can just return the raw margin in this case.
    return marginAfterForChild(child);
}

bool RenderBlock::hasMarginBeforeQuirk(const RenderBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isRenderBlock() ? toRenderBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isRenderBlock() ? toRenderBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool RenderBlock::hasMarginAfterQuirk(const RenderBox* child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child->isWritingModeRoot())
        return child->isRenderBlock() ? toRenderBlock(child)->hasMarginAfterQuirk() : child->style()->hasMarginAfterQuirk();

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isRenderBlock() ? toRenderBlock(child)->hasMarginBeforeQuirk() : child->style()->hasMarginBeforeQuirk();

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

const char* RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.

    if (isFloating())
        return "RenderBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousColumnsBlock())
        return "RenderBlock (anonymous multi-column)";
    if (isAnonymousColumnSpanBlock())
        return "RenderBlock (anonymous multi-column span)";
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderBlock (generated)";
    if (isAnonymous())
        return "RenderBlock (generated)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    if (isStickyPositioned())
        return "RenderBlock (sticky positioned)";
    return "RenderBlock";
}

RenderBlock* RenderBlock::createAnonymousWithParentRendererAndDisplay(const RenderObject* parent, EDisplay display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    EDisplay newDisplay;
    RenderBlock* newBox = 0;
    if (display == BOX || display == INLINE_BOX) {
        // FIXME: Remove this case once we have eliminated all internal users of old flexbox
        newBox = RenderDeprecatedFlexibleBox::createAnonymous(&parent->document());
        newDisplay = BOX;
    } else if (display == FLEX || display == INLINE_FLEX) {
        newBox = RenderFlexibleBox::createAnonymous(&parent->document());
        newDisplay = FLEX;
    } else {
        newBox = RenderBlockFlow::createAnonymous(&parent->document());
        newDisplay = BLOCK;
    }

    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent->style(), newDisplay);
    newBox->setStyle(newStyle.release());
    return newBox;
}

RenderBlockFlow* RenderBlock::createAnonymousColumnsWithParentRenderer(const RenderObject* parent)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent->style(), BLOCK);
    newStyle->inheritColumnPropertiesFrom(parent->style());

    RenderBlockFlow* newBox = RenderBlockFlow::createAnonymous(&parent->document());
    newBox->setStyle(newStyle.release());
    return newBox;
}

RenderBlockFlow* RenderBlock::createAnonymousColumnSpanWithParentRenderer(const RenderObject* parent)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent->style(), BLOCK);
    newStyle->setColumnSpan(ColumnSpanAll);

    RenderBlockFlow* newBox = RenderBlockFlow::createAnonymous(&parent->document());
    newBox->setStyle(newStyle.release());
    return newBox;
}

static bool recalcNormalFlowChildOverflowIfNeeded(RenderObject* renderer)
{
    if (renderer->isOutOfFlowPositioned() || !renderer->needsOverflowRecalcAfterStyleChange())
        return false;

    ASSERT(renderer->isRenderBlock());
    return toRenderBlock(renderer)->recalcOverflowAfterStyleChange();
}

bool RenderBlock::recalcChildOverflowAfterStyleChange()
{
    ASSERT(childNeedsOverflowRecalcAfterStyleChange());
    setChildNeedsOverflowRecalcAfterStyleChange(false);

    bool childrenOverflowChanged = false;

    if (childrenInline()) {
        ListHashSet<RootInlineBox*> lineBoxes;
        for (InlineWalker walker(this); !walker.atEnd(); walker.advance()) {
            RenderObject* renderer = walker.current();
            if (recalcNormalFlowChildOverflowIfNeeded(renderer)) {
                childrenOverflowChanged = true;
                if (InlineBox* inlineBoxWrapper = toRenderBlock(renderer)->inlineBoxWrapper())
                    lineBoxes.add(&inlineBoxWrapper->root());
            }
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;
        for (ListHashSet<RootInlineBox*>::const_iterator it = lineBoxes.begin(); it != lineBoxes.end(); ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (RenderBox* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (recalcNormalFlowChildOverflowIfNeeded(box))
                childrenOverflowChanged = true;
        }
    }

    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return childrenOverflowChanged;

    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        RenderBox* box = *it;

        if (!box->needsOverflowRecalcAfterStyleChange())
            continue;
        RenderBlock* block = toRenderBlock(box);
        if (!block->recalcOverflowAfterStyleChange() || box->style()->position() == FixedPosition)
            continue;

        childrenOverflowChanged = true;
    }
    return childrenOverflowChanged;
}

bool RenderBlock::recalcOverflowAfterStyleChange()
{
    ASSERT(needsOverflowRecalcAfterStyleChange());

    bool childrenOverflowChanged = false;
    if (childNeedsOverflowRecalcAfterStyleChange())
        childrenOverflowChanged = recalcChildOverflowAfterStyleChange();

    if (!selfNeedsOverflowRecalcAfterStyleChange() && !childrenOverflowChanged)
        return false;

    setSelfNeedsOverflowRecalcAfterStyleChange(false);
    // If the current block needs layout, overflow will be recalculated during
    // layout time anyway. We can safely exit here.
    if (needsLayout())
        return false;

    LayoutUnit oldClientAfterEdge = hasRenderOverflow() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, true);

    if (hasOverflowClip())
        layer()->scrollableArea()->updateAfterOverflowRecalc();

    return !hasOverflowClip();
}

#ifndef NDEBUG
void RenderBlock::checkPositionedObjectsNeedLayout()
{
    if (!gPositionedDescendantsMap)
        return;

    if (TrackedRendererListHashSet* positionedDescendantSet = positionedObjects()) {
        TrackedRendererListHashSet::const_iterator end = positionedDescendantSet->end();
        for (TrackedRendererListHashSet::const_iterator it = positionedDescendantSet->begin(); it != end; ++it) {
            RenderBox* currBox = *it;
            ASSERT(!currBox->needsLayout());
        }
    }
}

void RenderBlock::showLineTreeAndMark(const InlineBox* markedBox1, const char* markedLabel1, const InlineBox* markedBox2, const char* markedLabel2, const RenderObject* obj) const
{
    showRenderObject();
    for (const RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->showLineTreeAndMark(markedBox1, markedLabel1, markedBox2, markedLabel2, obj, 1);
}

#endif

} // namespace WebCore
