/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#include "core/layout/LayoutObject.h"

#include "core/animation/ElementAnimations.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/TextAffinity.h"
#include "core/frame/DeprecatedScheduleStyleRecalcDuringLayout.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutCounter.h"
#include "core/layout/LayoutDeprecatedFlexibleBox.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutImageResourceStyleImage.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTableCaption.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "platform/InstanceCounters.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/TransformState.h"
#include "platform/tracing/TracedValue.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include <algorithm>
#include <memory>
#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

namespace {

static bool gModifyLayoutTreeStructureAnyState = false;

}  // namespace

const LayoutUnit& caretWidth() {
  static LayoutUnit gCaretWidth(1);
  return gCaretWidth;
}

#if ENABLE(ASSERT)

LayoutObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(
    LayoutObject& layoutObject)
    : m_layoutObject(layoutObject),
      m_preexistingForbidden(m_layoutObject.isSetNeedsLayoutForbidden()) {
  m_layoutObject.setNeedsLayoutIsForbidden(true);
}

LayoutObject::SetLayoutNeededForbiddenScope::~SetLayoutNeededForbiddenScope() {
  m_layoutObject.setNeedsLayoutIsForbidden(m_preexistingForbidden);
}
#endif

struct SameSizeAsLayoutObject : DisplayItemClient {
  virtual ~SameSizeAsLayoutObject() {}  // Allocate vtable pointer.
  void* pointers[5];
  Member<void*> members[1];
#if ENABLE(ASSERT)
  unsigned m_debugBitfields : 2;
#endif
  unsigned m_bitfields;
  unsigned m_bitfields2;
  LayoutRect m_visualRect;
  LayoutPoint m_paintOffset;
  std::unique_ptr<void*> m_paintProperties;
};

static_assert(sizeof(LayoutObject) == sizeof(SameSizeAsLayoutObject),
              "LayoutObject should stay small");

bool LayoutObject::s_affectsParentBlock = false;

void* LayoutObject::operator new(size_t sz) {
  ASSERT(isMainThread());
  return partitionAlloc(WTF::Partitions::layoutPartition(), sz,
                        WTF_HEAP_PROFILER_TYPE_NAME(LayoutObject));
}

void LayoutObject::operator delete(void* ptr) {
  ASSERT(isMainThread());
  partitionFree(ptr);
}

LayoutObject* LayoutObject::createObject(Element* element,
                                         const ComputedStyle& style) {
  ASSERT(isAllowedToModifyLayoutTreeStructure(element->document()));

  // Minimal support for content properties replacing an entire element.
  // Works only if we have exactly one piece of content and it's a URL.
  // Otherwise acts as if we didn't support this feature.
  const ContentData* contentData = style.contentData();
  if (contentData && !contentData->next() && contentData->isImage() &&
      !element->isPseudoElement()) {
    LayoutImage* image = new LayoutImage(element);
    // LayoutImageResourceStyleImage requires a style being present on the image
    // but we don't want to trigger a style change now as the node is not fully
    // attached. Moving this code to style change doesn't make sense as it
    // should be run once at layoutObject creation.
    image->setStyleInternal(const_cast<ComputedStyle*>(&style));
    if (const StyleImage* styleImage =
            toImageContentData(contentData)->image()) {
      image->setImageResource(LayoutImageResourceStyleImage::create(
          const_cast<StyleImage*>(styleImage)));
      image->setIsGeneratedContent();
    } else {
      image->setImageResource(LayoutImageResource::create());
    }
    image->setStyleInternal(nullptr);
    return image;
  }

  switch (style.display()) {
    case EDisplay::None:
    case EDisplay::Contents:
      return nullptr;
    case EDisplay::Inline:
      return new LayoutInline(element);
    case EDisplay::Block:
    case EDisplay::InlineBlock:
      if (RuntimeEnabledFeatures::layoutNGEnabled())
        return new LayoutNGBlockFlow(element);
      return new LayoutBlockFlow(element);
    case EDisplay::ListItem:
      return new LayoutListItem(element);
    case EDisplay::Table:
    case EDisplay::InlineTable:
      return new LayoutTable(element);
    case EDisplay::TableRowGroup:
    case EDisplay::TableHeaderGroup:
    case EDisplay::TableFooterGroup:
      return new LayoutTableSection(element);
    case EDisplay::TableRow:
      return new LayoutTableRow(element);
    case EDisplay::TableColumnGroup:
    case EDisplay::TableColumn:
      return new LayoutTableCol(element);
    case EDisplay::TableCell:
      return new LayoutTableCell(element);
    case EDisplay::TableCaption:
      return new LayoutTableCaption(element);
    case EDisplay::Box:
    case EDisplay::InlineBox:
      return new LayoutDeprecatedFlexibleBox(*element);
    case EDisplay::Flex:
    case EDisplay::InlineFlex:
      return new LayoutFlexibleBox(element);
    case EDisplay::Grid:
    case EDisplay::InlineGrid:
      return new LayoutGrid(element);
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

LayoutObject::LayoutObject(Node* node)
    : m_style(nullptr),
      m_node(node),
      m_parent(nullptr),
      m_previous(nullptr),
      m_next(nullptr),
#if ENABLE(ASSERT)
      m_hasAXObject(false),
      m_setNeedsLayoutForbidden(false),
#endif
      m_bitfields(node) {
  InstanceCounters::incrementCounter(InstanceCounters::LayoutObjectCounter);
  if (m_node)
    frameView()->incrementLayoutObjectCount();
}

LayoutObject::~LayoutObject() {
  ASSERT(!m_hasAXObject);
  InstanceCounters::decrementCounter(InstanceCounters::LayoutObjectCounter);
}

bool LayoutObject::isDescendantOf(const LayoutObject* obj) const {
  for (const LayoutObject* r = this; r; r = r->m_parent) {
    if (r == obj)
      return true;
  }
  return false;
}

bool LayoutObject::isHR() const {
  return isHTMLHRElement(node());
}

bool LayoutObject::isLegend() const {
  return isHTMLLegendElement(node());
}

void LayoutObject::setIsInsideFlowThreadIncludingDescendants(
    bool insideFlowThread) {
  LayoutObject* next;
  for (LayoutObject* object = this; object; object = next) {
    // If object is a fragmentation context it already updated the descendants
    // flag accordingly.
    if (object->isLayoutFlowThread()) {
      next = object->nextInPreOrderAfterChildren(this);
      continue;
    }
    next = object->nextInPreOrder(this);
    ASSERT(insideFlowThread != object->isInsideFlowThread());
    object->setIsInsideFlowThread(insideFlowThread);
  }
}

bool LayoutObject::requiresAnonymousTableWrappers(
    const LayoutObject* newChild) const {
  // Check should agree with:
  // CSS 2.1 Tables: 17.2.1 Anonymous table objects
  // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
  if (newChild->isLayoutTableCol()) {
    const LayoutTableCol* newTableColumn = toLayoutTableCol(newChild);
    bool isColumnInColumnGroup =
        newTableColumn->isTableColumn() && isLayoutTableCol();
    return !isTable() && !isColumnInColumnGroup;
  }
  if (newChild->isTableCaption())
    return !isTable();
  if (newChild->isTableSection())
    return !isTable();
  if (newChild->isTableRow())
    return !isTableSection();
  if (newChild->isTableCell())
    return !isTableRow();
  return false;
}

DISABLE_CFI_PERF
void LayoutObject::addChild(LayoutObject* newChild, LayoutObject* beforeChild) {
  ASSERT(isAllowedToModifyLayoutTreeStructure(document()));

  LayoutObjectChildList* children = virtualChildren();
  ASSERT(children);
  if (!children)
    return;

  if (requiresAnonymousTableWrappers(newChild)) {
    // Generate an anonymous table or reuse existing one from previous child
    // Per: 17.2.1 Anonymous table objects 3. Generate missing parents
    // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
    LayoutTable* table;
    LayoutObject* afterChild =
        beforeChild ? beforeChild->previousSibling() : children->lastChild();
    if (afterChild && afterChild->isAnonymous() && afterChild->isTable() &&
        !afterChild->isBeforeContent()) {
      table = toLayoutTable(afterChild);
    } else {
      table = LayoutTable::createAnonymousWithParent(this);
      children->insertChildNode(this, table, beforeChild);
    }
    table->addChild(newChild);
  } else {
    children->insertChildNode(this, newChild, beforeChild);
  }

  if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE)
    toLayoutText(newChild)->transformText();

  // SVG creates layoutObjects for <g display="none">, as SVG requires children
  // of hidden <g>s to have layoutObjects - at least that's how our
  // implementation works.
  // Consider:
  // <g display="none"><foreignObject><body style="position: relative">FOO...
  // - layerTypeRequired() would return true for the <body>, creating a new
  //   Layer
  // - when the document is painted, both layers are painted. The <body> layer
  //   doesn't know that it's inside a "hidden SVG subtree", and thus paints,
  //   even if it shouldn't.
  // To avoid the problem altogether, detect early if we're inside a hidden SVG
  // subtree and stop creating layers at all for these cases - they're not used
  // anyways.
  if (newChild->hasLayer() && !layerCreationAllowedForSubtree())
    toLayoutBoxModelObject(newChild)
        ->layer()
        ->removeOnlyThisLayerAfterStyleChange();
}

void LayoutObject::removeChild(LayoutObject* oldChild) {
  ASSERT(isAllowedToModifyLayoutTreeStructure(document()));

  LayoutObjectChildList* children = virtualChildren();
  ASSERT(children);
  if (!children)
    return;

  children->removeChildNode(this, oldChild);
}

void LayoutObject::setDangerousOneWayParent(LayoutObject* parent) {
  ASSERT(!previousSibling());
  ASSERT(!nextSibling());
  ASSERT(!parent || !m_parent);
  setParent(parent);
}

void LayoutObject::registerSubtreeChangeListenerOnDescendants(bool value) {
  // If we're set to the same value then we're done as that means it's
  // set down the tree that way already.
  if (m_bitfields.subtreeChangeListenerRegistered() == value)
    return;

  m_bitfields.setSubtreeChangeListenerRegistered(value);

  for (LayoutObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
    curr->registerSubtreeChangeListenerOnDescendants(value);
}

void LayoutObject::notifyAncestorsOfSubtreeChange() {
  if (m_bitfields.notifiedOfSubtreeChange())
    return;

  m_bitfields.setNotifiedOfSubtreeChange(true);
  if (parent())
    parent()->notifyAncestorsOfSubtreeChange();
}

void LayoutObject::notifyOfSubtreeChange() {
  if (!m_bitfields.subtreeChangeListenerRegistered())
    return;
  if (m_bitfields.notifiedOfSubtreeChange())
    return;

  notifyAncestorsOfSubtreeChange();

  // We can modify the layout tree during layout which means that we may
  // try to schedule this during performLayout. This should no longer
  // happen when crbug.com/370457 is fixed.
  DeprecatedScheduleStyleRecalcDuringLayout marker(document().lifecycle());
  document().scheduleLayoutTreeUpdateIfNeeded();
}

void LayoutObject::handleSubtreeModifications() {
  ASSERT(wasNotifiedOfSubtreeChange());
  ASSERT(document().lifecycle().stateAllowsLayoutTreeNotifications());

  if (consumesSubtreeChangeNotification())
    subtreeDidChange();

  m_bitfields.setNotifiedOfSubtreeChange(false);

  for (LayoutObject* object = slowFirstChild(); object;
       object = object->nextSibling()) {
    if (!object->wasNotifiedOfSubtreeChange())
      continue;
    object->handleSubtreeModifications();
  }
}

LayoutObject* LayoutObject::nextInPreOrder() const {
  if (LayoutObject* o = slowFirstChild())
    return o;

  return nextInPreOrderAfterChildren();
}

LayoutObject* LayoutObject::nextInPreOrderAfterChildren() const {
  LayoutObject* o = nextSibling();
  if (!o) {
    o = parent();
    while (o && !o->nextSibling())
      o = o->parent();
    if (o)
      o = o->nextSibling();
  }

  return o;
}

LayoutObject* LayoutObject::nextInPreOrder(
    const LayoutObject* stayWithin) const {
  if (LayoutObject* o = slowFirstChild())
    return o;

  return nextInPreOrderAfterChildren(stayWithin);
}

LayoutObject* LayoutObject::nextInPreOrderAfterChildren(
    const LayoutObject* stayWithin) const {
  if (this == stayWithin)
    return nullptr;

  const LayoutObject* current = this;
  LayoutObject* next = current->nextSibling();
  for (; !next; next = current->nextSibling()) {
    current = current->parent();
    if (!current || current == stayWithin)
      return nullptr;
  }
  return next;
}

LayoutObject* LayoutObject::previousInPreOrder() const {
  if (LayoutObject* o = previousSibling()) {
    while (LayoutObject* lastChild = o->slowLastChild())
      o = lastChild;
    return o;
  }

  return parent();
}

LayoutObject* LayoutObject::previousInPreOrder(
    const LayoutObject* stayWithin) const {
  if (this == stayWithin)
    return nullptr;

  return previousInPreOrder();
}

LayoutObject* LayoutObject::childAt(unsigned index) const {
  LayoutObject* child = slowFirstChild();
  for (unsigned i = 0; child && i < index; i++)
    child = child->nextSibling();
  return child;
}

LayoutObject* LayoutObject::lastLeafChild() const {
  LayoutObject* r = slowLastChild();
  while (r) {
    LayoutObject* n = nullptr;
    n = r->slowLastChild();
    if (!n)
      break;
    r = n;
  }
  return r;
}

static void addLayers(LayoutObject* obj,
                      PaintLayer* parentLayer,
                      LayoutObject*& newObject,
                      PaintLayer*& beforeChild) {
  if (obj->hasLayer()) {
    if (!beforeChild && newObject) {
      // We need to figure out the layer that follows newObject. We only do
      // this the first time we find a child layer, and then we update the
      // pointer values for newObject and beforeChild used by everyone else.
      beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
      newObject = nullptr;
    }
    parentLayer->addChild(toLayoutBoxModelObject(obj)->layer(), beforeChild);
    return;
  }

  for (LayoutObject* curr = obj->slowFirstChild(); curr;
       curr = curr->nextSibling())
    addLayers(curr, parentLayer, newObject, beforeChild);
}

void LayoutObject::addLayers(PaintLayer* parentLayer) {
  if (!parentLayer)
    return;

  LayoutObject* object = this;
  PaintLayer* beforeChild = nullptr;
  blink::addLayers(this, parentLayer, object, beforeChild);
}

void LayoutObject::removeLayers(PaintLayer* parentLayer) {
  if (!parentLayer)
    return;

  if (hasLayer()) {
    parentLayer->removeChild(toLayoutBoxModelObject(this)->layer());
    return;
  }

  for (LayoutObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
    curr->removeLayers(parentLayer);
}

void LayoutObject::moveLayers(PaintLayer* oldParent, PaintLayer* newParent) {
  if (!newParent)
    return;

  if (hasLayer()) {
    PaintLayer* layer = toLayoutBoxModelObject(this)->layer();
    ASSERT(oldParent == layer->parent());
    if (oldParent)
      oldParent->removeChild(layer);
    newParent->addChild(layer);
    return;
  }

  for (LayoutObject* curr = slowFirstChild(); curr; curr = curr->nextSibling())
    curr->moveLayers(oldParent, newParent);
}

PaintLayer* LayoutObject::findNextLayer(PaintLayer* parentLayer,
                                        LayoutObject* startPoint,
                                        bool checkParent) {
  // Error check the parent layer passed in. If it's null, we can't find
  // anything.
  if (!parentLayer)
    return 0;

  // Step 1: If our layer is a child of the desired parent, then return our
  // layer.
  PaintLayer* ourLayer =
      hasLayer() ? toLayoutBoxModelObject(this)->layer() : nullptr;
  if (ourLayer && ourLayer->parent() == parentLayer)
    return ourLayer;

  // Step 2: If we don't have a layer, or our layer is the desired parent, then
  // descend into our siblings trying to find the next layer whose parent is the
  // desired parent.
  if (!ourLayer || ourLayer == parentLayer) {
    for (LayoutObject* curr = startPoint ? startPoint->nextSibling()
                                         : slowFirstChild();
         curr; curr = curr->nextSibling()) {
      PaintLayer* nextLayer = curr->findNextLayer(parentLayer, nullptr, false);
      if (nextLayer)
        return nextLayer;
    }
  }

  // Step 3: If our layer is the desired parent layer, then we're finished. We
  // didn't find anything.
  if (parentLayer == ourLayer)
    return nullptr;

  // Step 4: If |checkParent| is set, climb up to our parent and check its
  // siblings that follow us to see if we can locate a layer.
  if (checkParent && parent())
    return parent()->findNextLayer(parentLayer, this, true);

  return nullptr;
}

PaintLayer* LayoutObject::enclosingLayer() const {
  for (const LayoutObject* current = this; current;
       current = current->parent()) {
    if (current->hasLayer())
      return toLayoutBoxModelObject(current)->layer();
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

PaintLayer* LayoutObject::paintingLayer() const {
  for (const LayoutObject* current = this; current;
       current = current->paintInvalidationParent()) {
    if (current->hasLayer() &&
        toLayoutBoxModelObject(current)->layer()->isSelfPaintingLayer())
      return toLayoutBoxModelObject(current)->layer();
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

bool LayoutObject::scrollRectToVisible(const LayoutRect& rect,
                                       const ScrollAlignment& alignX,
                                       const ScrollAlignment& alignY,
                                       ScrollType scrollType,
                                       bool makeVisibleInVisualViewport) {
  LayoutBox* enclosingBox = this->enclosingBox();
  if (!enclosingBox)
    return false;

  enclosingBox->scrollRectToVisible(rect, alignX, alignY, scrollType,
                                    makeVisibleInVisualViewport);
  return true;
}

LayoutBox* LayoutObject::enclosingBox() const {
  LayoutObject* curr = const_cast<LayoutObject*>(this);
  while (curr) {
    if (curr->isBox())
      return toLayoutBox(curr);
    curr = curr->parent();
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

LayoutBoxModelObject* LayoutObject::enclosingBoxModelObject() const {
  LayoutObject* curr = const_cast<LayoutObject*>(this);
  while (curr) {
    if (curr->isBoxModelObject())
      return toLayoutBoxModelObject(curr);
    curr = curr->parent();
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

LayoutBox* LayoutObject::enclosingScrollableBox() const {
  for (LayoutObject* ancestor = parent(); ancestor;
       ancestor = ancestor->parent()) {
    if (!ancestor->isBox())
      continue;

    LayoutBox* ancestorBox = toLayoutBox(ancestor);
    if (ancestorBox->canBeScrolledAndHasScrollableArea())
      return ancestorBox;
  }

  return nullptr;
}

LayoutFlowThread* LayoutObject::locateFlowThreadContainingBlock() const {
  ASSERT(isInsideFlowThread());

  // See if we have the thread cached because we're in the middle of layout.
  if (LayoutState* layoutState = view()->layoutState()) {
    if (LayoutFlowThread* flowThread = layoutState->flowThread())
      return flowThread;
  }

  // Not in the middle of layout so have to find the thread the slow way.
  return LayoutFlowThread::locateFlowThreadContainingBlockOf(*this);
}

static inline bool objectIsRelayoutBoundary(const LayoutObject* object) {
  // FIXME: In future it may be possible to broaden these conditions in order to
  // improve performance.
  if (object->isTextControl())
    return true;

  if (object->isSVGRoot())
    return true;

  // Table parts can't be relayout roots since the table is responsible for
  // layouting all the parts.
  if (object->isTablePart())
    return false;

  if (object->style()->containsLayout() && object->style()->containsSize())
    return true;

  if (!object->hasOverflowClip())
    return false;

  if (object->style()->width().isIntrinsicOrAuto() ||
      object->style()->height().isIntrinsicOrAuto() ||
      object->style()->height().isPercentOrCalc())
    return false;

  // Scrollbar parts can be removed during layout. Avoid the complexity of
  // having to deal with that.
  if (object->isLayoutScrollbarPart())
    return false;

  // In general we can't relayout a flex item independently of its container;
  // not only is the result incorrect due to the override size that's set, it
  // also messes with the cached main size on the flexbox.
  if (object->isBox() && toLayoutBox(object)->isFlexItem())
    return false;

  // Inside multicol it's generally problematic to allow relayout roots. The
  // multicol container itself may be scheduled for relayout as well (due to
  // other changes that may have happened since the previous layout pass),
  // which might affect the column heights, which may affect how this object
  // breaks across columns). Spanners may also have been added or removed since
  // the previous layout pass, which is just another way of affecting the column
  // heights (and the number of rows). Instead of identifying cases where it's
  // safe to allow relayout roots, just disallow them inside multicol.
  if (object->isInsideFlowThread())
    return false;

  return true;
}

void LayoutObject::markContainerChainForLayout(bool scheduleRelayout,
                                               SubtreeLayoutScope* layouter) {
  ASSERT(!isSetNeedsLayoutForbidden());
  ASSERT(!layouter || this != layouter->root());
  // When we're in layout, we're marking a descendant as needing layout with
  // the intention of visiting it during this layout. We shouldn't be
  // scheduling it to be laid out later. Also, scheduleRelayout() must not be
  // called while iterating FrameView::m_layoutSubtreeRootList.
  scheduleRelayout &= !frameView()->isInPerformLayout();

  LayoutObject* object = container();
  LayoutObject* last = this;

  bool simplifiedNormalFlowLayout = needsSimplifiedNormalFlowLayout() &&
                                    !selfNeedsLayout() &&
                                    !normalChildNeedsLayout();

  while (object) {
    if (object->selfNeedsLayout())
      return;

    // Don't mark the outermost object of an unrooted subtree. That object will
    // be marked when the subtree is added to the document.
    LayoutObject* container = object->container();
    if (!container && !object->isLayoutView())
      return;
    if (!last->isTextOrSVGChild() && last->style()->hasOutOfFlowPosition()) {
      object = last->containingBlock();
      if (object->posChildNeedsLayout())
        return;
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
      layouter->recordObjectMarkedForLayout(object);
      if (object == layouter->root())
        return;
    }

    last = object;
    if (scheduleRelayout && objectIsRelayoutBoundary(last))
      break;
    object = container;
  }

  if (scheduleRelayout)
    last->scheduleRelayout();
}

#if ENABLE(ASSERT)
void LayoutObject::checkBlockPositionedObjectsNeedLayout() {
  ASSERT(!needsLayout());

  if (isLayoutBlock())
    toLayoutBlock(this)->checkPositionedObjectsNeedLayout();
}
#endif

void LayoutObject::setPreferredLogicalWidthsDirty(MarkingBehavior markParents) {
  m_bitfields.setPreferredLogicalWidthsDirty(true);
  if (markParents == MarkContainerChain &&
      (isText() || !style()->hasOutOfFlowPosition()))
    invalidateContainerPreferredLogicalWidths();
}

void LayoutObject::clearPreferredLogicalWidthsDirty() {
  m_bitfields.setPreferredLogicalWidthsDirty(false);
}

inline void LayoutObject::invalidateContainerPreferredLogicalWidths() {
  // In order to avoid pathological behavior when inlines are deeply nested, we
  // do include them in the chain that we mark dirty (even though they're kind
  // of irrelevant).
  LayoutObject* o = isTableCell() ? containingBlock() : container();
  while (o && !o->preferredLogicalWidthsDirty()) {
    // Don't invalidate the outermost object of an unrooted subtree. That object
    // will be invalidated when the subtree is added to the document.
    LayoutObject* container =
        o->isTableCell() ? o->containingBlock() : o->container();
    if (!container && !o->isLayoutView())
      break;

    o->m_bitfields.setPreferredLogicalWidthsDirty(true);
    // A positioned object has no effect on the min/max width of its containing
    // block ever. We can optimize this case and not go up any further.
    if (o->style()->hasOutOfFlowPosition())
      break;
    o = container;
  }
}

LayoutObject* LayoutObject::containerForAbsolutePosition(
    const LayoutBoxModelObject* ancestor,
    bool* ancestorSkipped,
    bool* filterSkipped) const {
  DCHECK(!ancestorSkipped || !*ancestorSkipped);
  DCHECK(!filterSkipped || !*filterSkipped);

  // We technically just want our containing block, but we may not have one if
  // we're part of an uninstalled subtree. We'll climb as high as we can though.
  for (LayoutObject* object = parent(); object; object = object->parent()) {
    if (object->canContainAbsolutePositionObjects())
      return object;

    if (ancestorSkipped && object == ancestor)
      *ancestorSkipped = true;

    if (filterSkipped && object->hasFilterInducingProperty())
      *filterSkipped = true;
  }
  return nullptr;
}

LayoutBlock* LayoutObject::containerForFixedPosition(
    const LayoutBoxModelObject* ancestor,
    bool* ancestorSkipped,
    bool* filterSkipped) const {
  DCHECK(!ancestorSkipped || !*ancestorSkipped);
  DCHECK(!filterSkipped || !*filterSkipped);
  DCHECK(!isText());

  LayoutObject* object = parent();
  for (; object && !object->canContainFixedPositionObjects();
       object = object->parent()) {
    if (ancestorSkipped && object == ancestor)
      *ancestorSkipped = true;

    if (filterSkipped && object->hasFilterInducingProperty())
      *filterSkipped = true;
  }

  ASSERT(!object || !object->isAnonymousBlock());
  return toLayoutBlock(object);
}

LayoutBlock* LayoutObject::containingBlockForAbsolutePosition() const {
  LayoutObject* o = containerForAbsolutePosition();

  // For relpositioned inlines, we return the nearest non-anonymous enclosing
  // block. We don't try to return the inline itself. This allows us to avoid
  // having a positioned objects list in all LayoutInlines and lets us return a
  // strongly-typed LayoutBlock* result from this method. The container() method
  // can actually be used to obtain the inline directly.
  if (o && o->isInline() && !o->isAtomicInlineLevel()) {
    ASSERT(o->style()->hasInFlowPosition());
    o = o->containingBlock();
  }

  if (o && !o->isLayoutBlock())
    o = o->containingBlock();

  while (o && o->isAnonymousBlock())
    o = o->containingBlock();

  if (!o || !o->isLayoutBlock())
    return nullptr;  // This can still happen in case of an orphaned tree

  return toLayoutBlock(o);
}

LayoutBlock* LayoutObject::containingBlock() const {
  LayoutObject* o = parent();
  if (!o && isLayoutScrollbarPart())
    o = toLayoutScrollbarPart(this)->layoutObjectOwningScrollbar();
  if (!isTextOrSVGChild()) {
    if (m_style->position() == FixedPosition)
      return containerForFixedPosition();
    if (m_style->position() == AbsolutePosition)
      return containingBlockForAbsolutePosition();
  }
  if (isColumnSpanAll()) {
    o = spannerPlaceholder()->containingBlock();
  } else {
    while (o && ((o->isInline() && !o->isAtomicInlineLevel()) ||
                 !o->isLayoutBlock()))
      o = o->parent();
  }

  if (!o || !o->isLayoutBlock())
    return nullptr;  // This can still happen in case of an orphaned tree

  return toLayoutBlock(o);
}

FloatRect LayoutObject::absoluteBoundingBoxFloatRect() const {
  Vector<FloatQuad> quads;
  absoluteQuads(quads);

  size_t n = quads.size();
  if (n == 0)
    return FloatRect();

  FloatRect result = quads[0].boundingBox();
  for (size_t i = 1; i < n; ++i)
    result.unite(quads[i].boundingBox());
  return result;
}

IntRect LayoutObject::absoluteBoundingBoxRect() const {
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

IntRect LayoutObject::absoluteBoundingBoxRectIgnoringTransforms() const {
  FloatPoint absPos = localToAbsolute();
  Vector<IntRect> rects;
  absoluteRects(rects, flooredLayoutPoint(absPos));

  size_t n = rects.size();
  if (!n)
    return IntRect();

  IntRect result = rects[0];
  for (size_t i = 1; i < n; ++i)
    result.unite(rects[i]);
  return result;
}

IntRect LayoutObject::absoluteElementBoundingBoxRect() const {
  Vector<LayoutRect> rects;
  const LayoutBoxModelObject* container = enclosingLayer()->layoutObject();
  addElementVisualOverflowRects(
      rects, LayoutPoint(localToAncestorPoint(FloatPoint(), container)));
  return container->localToAbsoluteQuad(FloatQuad(FloatRect(unionRect(rects))))
      .enclosingBoundingBox();
}

FloatRect LayoutObject::absoluteBoundingBoxRectForRange(const Range* range) {
  if (!range || !range->startContainer())
    return FloatRect();

  range->ownerDocument().updateStyleAndLayout();

  Vector<FloatQuad> quads;
  range->textQuads(quads);

  FloatRect result;
  for (size_t i = 0; i < quads.size(); ++i)
    result.unite(quads[i].boundingBox());

  return result;
}

void LayoutObject::addAbsoluteRectForLayer(IntRect& result) {
  if (hasLayer())
    result.unite(absoluteBoundingBoxRect());
  for (LayoutObject* current = slowFirstChild(); current;
       current = current->nextSibling())
    current->addAbsoluteRectForLayer(result);
}

IntRect LayoutObject::absoluteBoundingBoxRectIncludingDescendants() const {
  IntRect result = absoluteBoundingBoxRect();
  for (LayoutObject* current = slowFirstChild(); current;
       current = current->nextSibling())
    current->addAbsoluteRectForLayer(result);
  return result;
}

void LayoutObject::paint(const PaintInfo&, const LayoutPoint&) const {}

const LayoutBoxModelObject& LayoutObject::containerForPaintInvalidation()
    const {
  RELEASE_ASSERT(isRooted());

  if (const LayoutBoxModelObject* paintInvalidationContainer =
          enclosingCompositedContainer())
    return *paintInvalidationContainer;

  // If the current frame is not composited, we send just return the main
  // frame's LayoutView so that we generate invalidations on the window.
  const LayoutView* layoutView = view();
  while (
      LayoutAPIShim::layoutObjectFrom(layoutView->frame()->ownerLayoutItem()))
    layoutView =
        LayoutAPIShim::layoutObjectFrom(layoutView->frame()->ownerLayoutItem())
            ->view();
  ASSERT(layoutView);
  return *layoutView;
}

const LayoutBoxModelObject* LayoutObject::enclosingCompositedContainer() const {
  LayoutBoxModelObject* container = nullptr;
  // FIXME: CompositingState is not necessarily up to date for many callers of
  // this function.
  DisableCompositingQueryAsserts disabler;

  if (PaintLayer* paintingLayer = this->paintingLayer()) {
    if (PaintLayer* compositingLayer =
            paintingLayer
                ->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
      container = compositingLayer->layoutObject();
  }
  return container;
}

String LayoutObject::decoratedName() const {
  StringBuilder name;
  name.append(this->name());

  if (isAnonymous())
    name.append(" (anonymous)");
  // FIXME: Remove the special case for LayoutView here (requires rebaseline of
  // all tests).
  if (isOutOfFlowPositioned() && !isLayoutView())
    name.append(" (positioned)");
  if (isRelPositioned())
    name.append(" (relative positioned)");
  if (isStickyPositioned())
    name.append(" (sticky positioned)");
  if (isFloating())
    name.append(" (floating)");
  if (spannerPlaceholder())
    name.append(" (column spanner)");

  return name.toString();
}

String LayoutObject::debugName() const {
  StringBuilder name;
  name.append(decoratedName());

  if (const Node* node = this->node()) {
    name.append(' ');
    name.append(node->debugName());
  }
  return name.toString();
}

LayoutRect LayoutObject::visualRect() const {
  return previousVisualRect();
}

bool LayoutObject::isPaintInvalidationContainer() const {
  return hasLayer() &&
         toLayoutBoxModelObject(this)->layer()->isPaintInvalidationContainer();
}

void LayoutObject::invalidateDisplayItemClients(
    PaintInvalidationReason reason) const {
  // This default implementation invalidates only the object itself as a
  // DisplayItemClient.
  ObjectPaintInvalidator(*this).invalidateDisplayItemClient(*this, reason);
}

bool LayoutObject::compositedScrollsWithRespectTo(
    const LayoutBoxModelObject& paintInvalidationContainer) const {
  return paintInvalidationContainer.usesCompositedScrolling() &&
         this != &paintInvalidationContainer;
}

IntSize LayoutObject::scrollAdjustmentForPaintInvalidation(
    const LayoutBoxModelObject& paintInvalidationContainer) const {
  // Non-composited scrolling should be included in the bounds of scrolleditems.
  // Since mapToVisualRectInAncestorSpace does not include scrolling of the
  // ancestor, we need to add it back in after.
  if (paintInvalidationContainer.isBox() &&
      !paintInvalidationContainer.usesCompositedScrolling() &&
      this != &paintInvalidationContainer) {
    const LayoutBox* box = toLayoutBox(&paintInvalidationContainer);
    if (box->hasOverflowClip())
      return -box->scrolledContentOffset();
  }
  return IntSize();
}

LayoutRect LayoutObject::invalidatePaintRectangle(
    const LayoutRect& dirtyRect,
    DisplayItemClient* displayItemClient) const {
  return ObjectPaintInvalidator(*this).invalidatePaintRectangle(
      dirtyRect, displayItemClient);
}

void LayoutObject::invalidateTreeIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  ensureIsReadyForPaintInvalidation();

  // If we didn't need paint invalidation then our children don't need as well.
  // Skip walking down the tree as everything should be fine below us.
  if (!shouldCheckForPaintInvalidation(paintInvalidationState))
    return;

  PaintInvalidationState newPaintInvalidationState(paintInvalidationState,
                                                   *this);

  if (mayNeedPaintInvalidationSubtree()) {
    newPaintInvalidationState
        .setForceSubtreeInvalidationCheckingWithinContainer();
  }

  PaintInvalidationReason reason =
      invalidatePaintIfNeeded(newPaintInvalidationState);
  newPaintInvalidationState.updateForChildren(reason);
  invalidatePaintOfSubtreesIfNeeded(newPaintInvalidationState);

  clearPaintInvalidationFlags();
}

DISABLE_CFI_PERF
void LayoutObject::invalidatePaintOfSubtreesIfNeeded(
    const PaintInvalidationState& childPaintInvalidationState) {
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());

  for (LayoutObject* child = slowFirstChild(); child;
       child = child->nextSibling()) {
    // Column spanners are invalidated through their placeholders. See
    // LayoutMultiColumnSpannerPlaceholder::invalidatePaintOfSubtreesIfNeeded().
    if (child->isColumnSpanAll())
      continue;
    child->invalidateTreeIfNeeded(childPaintInvalidationState);
  }
}

LayoutRect LayoutObject::selectionRectInViewCoordinates() const {
  LayoutRect selectionRect = localSelectionRect();
  if (!selectionRect.isEmpty())
    mapToVisualRectInAncestorSpace(view(), selectionRect);
  return selectionRect;
}

PaintInvalidationReason LayoutObject::invalidatePaintIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  DCHECK(&paintInvalidationState.currentObject() == this);

  if (styleRef().hasOutline()) {
    PaintLayer& layer = paintInvalidationState.paintingLayer();
    if (layer.layoutObject() != this)
      layer.setNeedsPaintPhaseDescendantOutlines();
  }

  LayoutView* v = view();
  if (v->document().printing())
    return PaintInvalidationNone;  // Don't invalidate paints if we're printing.

  PaintInvalidatorContextAdapter context(paintInvalidationState);

  const LayoutBoxModelObject& paintInvalidationContainer =
      paintInvalidationState.paintInvalidationContainer();
  DCHECK(paintInvalidationContainer == containerForPaintInvalidation());

  ObjectPaintInvalidator paintInvalidator(*this);
  context.oldVisualRect = previousVisualRect();
  context.oldLocation = paintInvalidator.previousLocationInBacking();
  context.newVisualRect = paintInvalidationState.computeVisualRectInBacking();
  context.newLocation = paintInvalidationState.computeLocationInBacking(
      context.newVisualRect.location());

  IntSize adjustment =
      scrollAdjustmentForPaintInvalidation(paintInvalidationContainer);
  context.newLocation.move(adjustment);
  context.newVisualRect.move(adjustment);

  adjustVisualRectForRasterEffects(context.newVisualRect);

  setPreviousVisualRect(context.newVisualRect);
  paintInvalidator.setPreviousLocationInBacking(context.newLocation);

  if (!shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState() &&
      paintInvalidationState
          .forcedSubtreeInvalidationRectUpdateWithinContainerOnly()) {
    // We are done updating the visual rect. No other paint invalidation work
    // to do for this object.
    return PaintInvalidationNone;
  }

  return invalidatePaintIfNeeded(context);
}

DISABLE_CFI_PERF
PaintInvalidationReason LayoutObject::invalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) const {
  return ObjectPaintInvalidatorWithContext(*this, context)
      .invalidatePaintIfNeeded();
}

void LayoutObject::adjustVisualRectForCompositedScrolling(
    LayoutRect& rect,
    const LayoutBoxModelObject& paintInvalidationContainer) const {
  if (compositedScrollsWithRespectTo(paintInvalidationContainer)) {
    LayoutSize offset(
        -toLayoutBox(&paintInvalidationContainer)->scrolledContentOffset());
    rect.move(offset);
  }
}

LayoutRect LayoutObject::previousVisualRectIncludingCompositedScrolling(
    const LayoutBoxModelObject& paintInvalidationContainer) const {
  LayoutRect rect = previousVisualRect();
  adjustVisualRectForCompositedScrolling(rect, paintInvalidationContainer);
  return rect;
}

void LayoutObject::adjustPreviousPaintInvalidationForScrollIfNeeded(
    const DoubleSize& scrollDelta) {
  if (containerForPaintInvalidation().usesCompositedScrolling())
    return;
  m_previousVisualRect.move(LayoutSize(scrollDelta));
}

void LayoutObject::clearPreviousVisualRects() {
  setPreviousVisualRect(LayoutRect());
  ObjectPaintInvalidator(*this).setPreviousLocationInBacking(LayoutPoint());
  // Ensure check paint invalidation of subtree that would be triggered by
  // location change if we had valid previous location.
  setMayNeedPaintInvalidationSubtree();
  // After clearing ("invalidating") the visual rects, mark this object as
  // needing to re-compute them.
  setShouldDoFullPaintInvalidation();
}

LayoutRect LayoutObject::absoluteVisualRect() const {
  LayoutRect rect = localVisualRect();
  mapToVisualRectInAncestorSpace(view(), rect);
  return rect;
}

LayoutRect LayoutObject::localVisualRect() const {
  NOTREACHED();
  return LayoutRect();
}

bool LayoutObject::mapToVisualRectInAncestorSpace(
    const LayoutBoxModelObject* ancestor,
    LayoutRect& rect,
    VisualRectFlags visualRectFlags) const {
  // For any layout object that doesn't override this method (the main example
  // is LayoutText), the rect is assumed to be in the parent's coordinate space,
  // except for container flip.

  if (ancestor == this)
    return true;

  if (LayoutObject* parent = this->parent()) {
    if (parent->isBox()) {
      LayoutBox* parentBox = toLayoutBox(parent);

      // Never flip for SVG as it handles writing modes itself.
      if (!isSVG())
        parentBox->flipForWritingMode(rect);

      if (parent != ancestor &&
          !parentBox->mapScrollingContentsRectToBoxSpace(rect, visualRectFlags))
        return false;
    }
    return parent->mapToVisualRectInAncestorSpace(ancestor, rect,
                                                  visualRectFlags);
  }
  return true;
}

void LayoutObject::dirtyLinesFromChangedChild(LayoutObject*, MarkingBehavior) {}

#ifndef NDEBUG

void LayoutObject::showTreeForThis() const {
  if (node())
    ::showTree(node());
}

void LayoutObject::showLayoutTreeForThis() const {
  showLayoutTree(this, 0);
}

void LayoutObject::showLineTreeForThis() const {
  if (LayoutBlock* cb = containingBlock()) {
    if (cb->isLayoutBlockFlow())
      toLayoutBlockFlow(cb)->showLineTreeAndMark(nullptr, nullptr, nullptr,
                                                 nullptr, this);
  }
}

void LayoutObject::showLayoutObject() const {
  StringBuilder stringBuilder;
  showLayoutObject(stringBuilder);
}

void LayoutObject::showLayoutObject(StringBuilder& stringBuilder) const {
  stringBuilder.append(
      String::format("%s %p", decoratedName().ascii().data(), this));

  if (isText() && toLayoutText(this)->isTextFragment())
    stringBuilder.append(
        String::format(" \"%s\" ", toLayoutText(this)->text().ascii().data()));

  if (virtualContinuation())
    stringBuilder.append(
        String::format(" continuation=%p", virtualContinuation()));

  if (node()) {
    while (stringBuilder.length() < showTreeCharacterOffset)
      stringBuilder.append(' ');
    stringBuilder.append('\t');
    WTFLogAlways("%s%s", stringBuilder.toString().utf8().data(),
                 node()->toString().utf8().data());
  } else {
    WTFLogAlways("%s", stringBuilder.toString().utf8().data());
  }
}

void LayoutObject::showLayoutTreeAndMark(const LayoutObject* markedObject1,
                                         const char* markedLabel1,
                                         const LayoutObject* markedObject2,
                                         const char* markedLabel2,
                                         unsigned depth) const {
  StringBuilder stringBuilder;
  if (markedObject1 == this && markedLabel1)
    stringBuilder.append(markedLabel1);
  if (markedObject2 == this && markedLabel2)
    stringBuilder.append(markedLabel2);
  while (stringBuilder.length() < depth * 2)
    stringBuilder.append(' ');

  showLayoutObject(stringBuilder);

  for (const LayoutObject* child = slowFirstChild(); child;
       child = child->nextSibling())
    child->showLayoutTreeAndMark(markedObject1, markedLabel1, markedObject2,
                                 markedLabel2, depth + 1);
}

#endif  // NDEBUG

bool LayoutObject::isSelectable() const {
  return !isInert() &&
         !(style()->userSelect() == SELECT_NONE &&
           style()->userModify() == READ_ONLY);
}

Color LayoutObject::selectionBackgroundColor() const {
  if (!isSelectable())
    return Color::transparent;

  if (RefPtr<ComputedStyle> pseudoStyle =
          getUncachedPseudoStyleFromParentOrShadowHost())
    return resolveColor(*pseudoStyle, CSSPropertyBackgroundColor)
        .blendWithWhite();
  return frame()->selection().isFocusedAndActive()
             ? LayoutTheme::theme().activeSelectionBackgroundColor()
             : LayoutTheme::theme().inactiveSelectionBackgroundColor();
}

Color LayoutObject::selectionColor(
    int colorProperty,
    const GlobalPaintFlags globalPaintFlags) const {
  // If the element is unselectable, or we are only painting the selection,
  // don't override the foreground color with the selection foreground color.
  if (!isSelectable() || (globalPaintFlags & GlobalPaintSelectionOnly))
    return resolveColor(colorProperty);

  if (RefPtr<ComputedStyle> pseudoStyle =
          getUncachedPseudoStyleFromParentOrShadowHost())
    return resolveColor(*pseudoStyle, colorProperty);
  if (!LayoutTheme::theme().supportsSelectionForegroundColors())
    return resolveColor(colorProperty);
  return frame()->selection().isFocusedAndActive()
             ? LayoutTheme::theme().activeSelectionForegroundColor()
             : LayoutTheme::theme().inactiveSelectionForegroundColor();
}

Color LayoutObject::selectionForegroundColor(
    const GlobalPaintFlags globalPaintFlags) const {
  return selectionColor(CSSPropertyWebkitTextFillColor, globalPaintFlags);
}

Color LayoutObject::selectionEmphasisMarkColor(
    const GlobalPaintFlags globalPaintFlags) const {
  return selectionColor(CSSPropertyWebkitTextEmphasisColor, globalPaintFlags);
}

void LayoutObject::selectionStartEnd(int& spos, int& epos) const {
  view()->selectionStartEnd(spos, epos);
}

// Called when an object that was floating or positioned becomes a normal flow
// object again. We have to make sure the layout tree updates as needed to
// accommodate the new normal flow object.
static inline void handleDynamicFloatPositionChange(LayoutObject* object) {
  // We have gone from not affecting the inline status of the parent flow to
  // suddenly having an impact.  See if there is a mismatch between the parent
  // flow's childrenInline() state and our state.
  object->setInline(object->style()->isDisplayInlineType());
  if (object->isInline() != object->parent()->childrenInline()) {
    if (!object->isInline()) {
      toLayoutBoxModelObject(object->parent())->childBecameNonInline(object);
    } else {
      // An anonymous block must be made to wrap this inline.
      LayoutBlock* block =
          toLayoutBlock(object->parent())->createAnonymousBlock();
      LayoutObjectChildList* childlist = object->parent()->virtualChildren();
      childlist->insertChildNode(object->parent(), block, object);
      block->children()->appendChildNode(
          block, childlist->removeChildNode(object->parent(), object));
    }
  }
}

StyleDifference LayoutObject::adjustStyleDifference(
    StyleDifference diff) const {
  if (diff.transformChanged() && isSVG()) {
    // Skip a full layout for transforms at the html/svg boundary which do not
    // affect sizes inside SVG.
    if (!isSVGRoot())
      diff.setNeedsFullLayout();
  }

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // Text nodes share style with their parents but the checked styles don't
    // apply to them, hence the !isText() check.
    if (!isText() && (diff.transformChanged() || diff.opacityChanged() ||
                      diff.zIndexChanged() || diff.filterChanged() ||
                      diff.backdropFilterChanged())) {
      // We don't need to invalidate paint of objects on SPv2 when only paint
      // property or paint order change. Mark the painting layer needing repaint
      // for changed paint property or paint order. Raster invalidation will be
      // issued if needed during paint.
      ObjectPaintInvalidator(*this).slowSetPaintingLayerNeedsRepaint();

      // When transform, opacity, etc. change, paint properties will also change
      // so we need to mark this object as needing an update.
      // TODO(pdr): Also update in the non-spv2 codepath?
      getMutableForPainting().setNeedsPaintPropertyUpdate();
    }
  } else {
    // If transform changed, and the layer does not paint into its own separate
    // backing, then we need to invalidate paints.
    if (diff.transformChanged()) {
      // Text nodes share style with their parents but transforms don't apply to
      // them, hence the !isText() check.
      if (!isText() && (!hasLayer() ||
                        !toLayoutBoxModelObject(this)
                             ->layer()
                             ->hasStyleDeterminedDirectCompositingReasons()))
        diff.setNeedsPaintInvalidationSubtree();
    }

    // If opacity or zIndex changed, and the layer does not paint into its own
    // separate backing, then we need to invalidate paints (also
    // ignoring text nodes).
    if (diff.opacityChanged() || diff.zIndexChanged()) {
      if (!isText() && (!hasLayer() ||
                        !toLayoutBoxModelObject(this)
                             ->layer()
                             ->hasStyleDeterminedDirectCompositingReasons()))
        diff.setNeedsPaintInvalidationSubtree();
    }

    // If filter changed, and the layer does not paint into its own separate
    // backing or it paints with filters, then we need to invalidate paints.
    if (diff.filterChanged() && hasLayer()) {
      PaintLayer* layer = toLayoutBoxModelObject(this)->layer();
      if (!layer->hasStyleDeterminedDirectCompositingReasons() ||
          layer->paintsWithFilters())
        diff.setNeedsPaintInvalidationSubtree();
    }

    // If backdrop filter changed, and the layer does not paint into its own
    // separate backing or it paints with filters, then we need to invalidate
    // paints.
    if (diff.backdropFilterChanged() && hasLayer()) {
      PaintLayer* layer = toLayoutBoxModelObject(this)->layer();
      if (!layer->hasStyleDeterminedDirectCompositingReasons() ||
          layer->paintsWithBackdropFilters())
        diff.setNeedsPaintInvalidationSubtree();
    }
  }

  // Optimization: for decoration/color property changes, invalidation is only
  // needed if we have style or text affected by these properties.
  if (diff.textDecorationOrColorChanged() && !diff.needsPaintInvalidation()) {
    if (style()->hasBorder() || style()->hasOutline() ||
        style()->hasBackgroundRelatedColorReferencingCurrentColor()
        // Skip any text nodes that do not contain text boxes. Whitespace cannot
        // be skipped or we will miss invalidating decorations (e.g.,
        // underlines).
        || (isText() && !isBR() && toLayoutText(this)->hasTextBoxes())
        // Caret is painted in text color.
        || (isLayoutBlock() && toLayoutBlock(this)->hasCaret()) ||
        (isSVG() && style()->svgStyle().isFillColorCurrentColor()) ||
        (isSVG() && style()->svgStyle().isStrokeColorCurrentColor()) ||
        isListMarker())
      diff.setNeedsPaintInvalidationObject();
  }

  // The answer to layerTypeRequired() for plugins, iframes, and canvas can
  // change without the actual style changing, since it depends on whether we
  // decide to composite these elements. When the/ layer status of one of these
  // elements changes, we need to force a layout.
  if (!diff.needsFullLayout() && style() && isBoxModelObject()) {
    bool requiresLayer =
        toLayoutBoxModelObject(this)->layerTypeRequired() != NoPaintLayer;
    if (hasLayer() != requiresLayer)
      diff.setNeedsFullLayout();
  }

  return diff;
}

void LayoutObject::setPseudoStyle(PassRefPtr<ComputedStyle> pseudoStyle) {
  ASSERT(pseudoStyle->styleType() == PseudoIdBefore ||
         pseudoStyle->styleType() == PseudoIdAfter ||
         pseudoStyle->styleType() == PseudoIdFirstLetter);

  // FIXME: We should consider just making all pseudo items use an inherited
  // style.

  // Images are special and must inherit the pseudoStyle so the width and height
  // of the pseudo element doesn't change the size of the image. In all other
  // cases we can just share the style.
  //
  // Quotes are also LayoutInline, so we need to create an inherited style to
  // avoid getting an inline with positioning or an invalid display.
  //
  if (isImage() || isQuote()) {
    RefPtr<ComputedStyle> style = ComputedStyle::create();
    style->inheritFrom(*pseudoStyle);
    setStyle(style.release());
    return;
  }

  setStyle(std::move(pseudoStyle));
}

void LayoutObject::firstLineStyleDidChange(const ComputedStyle& oldStyle,
                                           const ComputedStyle& newStyle) {
  StyleDifference diff = oldStyle.visualInvalidationDiff(newStyle);

  if (diff.needsPaintInvalidation() || diff.textDecorationOrColorChanged()) {
    // We need to invalidate all inline boxes in the first line, because they
    // need to be repainted with the new style, e.g. background, font style,
    // etc.
    LayoutBlockFlow* firstLineContainer = nullptr;
    if (behavesLikeBlockContainer()) {
      // This object is a LayoutBlock having PseudoIdFirstLine pseudo style
      // changed.
      firstLineContainer =
          toLayoutBlock(this)->nearestInnerBlockWithFirstLine();
    } else if (isLayoutInline()) {
      // This object is a LayoutInline having FIRST_LINE_INHERITED pesudo style
      // changed. This method can be called even if the LayoutInline doesn't
      // intersect the first line, but we only need to invalidate if it does.
      if (InlineBox* firstLineBox =
              toLayoutInline(this)->firstLineBoxIncludingCulling()) {
        if (firstLineBox->isFirstLineStyle())
          firstLineContainer = toLayoutBlockFlow(containingBlock());
      }
    }
    if (firstLineContainer)
      firstLineContainer->setShouldDoFullPaintInvalidationForFirstLine();
  }
  if (diff.needsLayout())
    setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
}

void LayoutObject::markAncestorsForOverflowRecalcIfNeeded() {
  LayoutObject* object = this;
  do {
    // Cell and row need to propagate the flag to their containing section and
    // row as their containing block is the table wrapper.
    // This enables us to only recompute overflow the modified sections / rows.
    object = object->isTableCell() || object->isTableRow()
                 ? object->parent()
                 : object->containingBlock();
    if (object)
      object->setChildNeedsOverflowRecalcAfterStyleChange();
  } while (object);
}

void LayoutObject::setNeedsOverflowRecalcAfterStyleChange() {
  bool neededRecalc = needsOverflowRecalcAfterStyleChange();
  setSelfNeedsOverflowRecalcAfterStyleChange();
  if (!neededRecalc)
    markAncestorsForOverflowRecalcIfNeeded();
}

DISABLE_CFI_PERF
void LayoutObject::setStyle(PassRefPtr<ComputedStyle> style) {
  ASSERT(style);

  if (m_style == style) {
    // We need to run through adjustStyleDifference() for iframes, plugins, and
    // canvas so style sharing is disabled for them. That should ensure that we
    // never hit this code path.
    ASSERT(!isLayoutIFrame() && !isEmbeddedObject() && !isCanvas());
    return;
  }

  StyleDifference diff;
  if (m_style)
    diff = m_style->visualInvalidationDiff(*style);

  diff = adjustStyleDifference(diff);

  styleWillChange(diff, *style);

  RefPtr<ComputedStyle> oldStyle = m_style.release();
  setStyleInternal(std::move(style));

  updateFillImages(oldStyle ? &oldStyle->backgroundLayers() : 0,
                   m_style->backgroundLayers());
  updateFillImages(oldStyle ? &oldStyle->maskLayers() : 0,
                   m_style->maskLayers());

  updateImage(oldStyle ? oldStyle->borderImage().image() : 0,
              m_style->borderImage().image());
  updateImage(oldStyle ? oldStyle->maskBoxImage().image() : 0,
              m_style->maskBoxImage().image());

  StyleImage* newContentImage =
      m_style->contentData() && m_style->contentData()->isImage()
          ? toImageContentData(m_style->contentData())->image()
          : nullptr;
  StyleImage* oldContentImage =
      oldStyle && oldStyle->contentData() && oldStyle->contentData()->isImage()
          ? toImageContentData(oldStyle->contentData())->image()
          : nullptr;
  updateImage(oldContentImage, newContentImage);

  StyleImage* newBoxReflectMaskImage =
      m_style->boxReflect() ? m_style->boxReflect()->mask().image() : nullptr;
  StyleImage* oldBoxReflectMaskImage =
      oldStyle && oldStyle->boxReflect()
          ? oldStyle->boxReflect()->mask().image()
          : nullptr;
  updateImage(oldBoxReflectMaskImage, newBoxReflectMaskImage);

  updateShapeImage(oldStyle ? oldStyle->shapeOutside() : 0,
                   m_style->shapeOutside());
  updateCursorImages(oldStyle ? oldStyle->cursors() : nullptr,
                     m_style->cursors());

  bool doesNotNeedLayoutOrPaintInvalidation = !m_parent;

  styleDidChange(diff, oldStyle.get());

  // FIXME: |this| might be destroyed here. This can currently happen for a
  // LayoutTextFragment when its first-letter block gets an update in
  // LayoutTextFragment::styleDidChange. For LayoutTextFragment(s),
  // we will safely bail out with the doesNotNeedLayoutOrPaintInvalidation flag.
  // We might want to broaden this condition in the future as we move
  // layoutObject changes out of layout and into style changes.
  if (doesNotNeedLayoutOrPaintInvalidation)
    return;

  // Now that the layer (if any) has been updated, we need to adjust the diff
  // again, check whether we should layout now, and decide if we need to
  // invalidate paints.
  StyleDifference updatedDiff = adjustStyleDifference(diff);

  if (!diff.needsFullLayout()) {
    if (updatedDiff.needsFullLayout())
      setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
    else if (updatedDiff.needsPositionedMovementLayout())
      setNeedsPositionedMovementLayout();
  }

  if (diff.transformChanged() && !needsLayout()) {
    if (LayoutBlock* container = containingBlock())
      container->setNeedsOverflowRecalcAfterStyleChange();
  }

  if (diff.needsRecomputeOverflow() && !needsLayout()) {
    // TODO(rhogan): Make inlines capable of recomputing overflow too.
    if (isLayoutBlock())
      setNeedsOverflowRecalcAfterStyleChange();
    else
      setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
  }

  if (diff.needsPaintInvalidationSubtree() ||
      updatedDiff.needsPaintInvalidationSubtree())
    setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  else if (diff.needsPaintInvalidationObject() ||
           updatedDiff.needsPaintInvalidationObject())
    setShouldDoFullPaintInvalidation();
}

void LayoutObject::styleWillChange(StyleDifference diff,
                                   const ComputedStyle& newStyle) {
  if (m_style) {
    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    bool visibilityChanged =
        m_style->visibility() != newStyle.visibility() ||
        m_style->zIndex() != newStyle.zIndex() ||
        m_style->isStackingContext() != newStyle.isStackingContext();
    if (visibilityChanged) {
      document().setAnnotatedRegionsDirty(true);
      if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(parent());
    }

    // Keep layer hierarchy visibility bits up to date if visibility changes.
    if (m_style->visibility() != newStyle.visibility()) {
      // We might not have an enclosing layer yet because we might not be in the
      // tree.
      if (PaintLayer* layer = enclosingLayer())
        layer->potentiallyDirtyVisibleContentStatus(newStyle.visibility());
    }

    if (isFloating() && (m_style->floating() != newStyle.floating())) {
      // For changes in float styles, we need to conceivably remove ourselves
      // from the floating objects list.
      toLayoutBox(this)->removeFloatingOrPositionedChildFromBlockLists();
    } else if (isOutOfFlowPositioned() &&
               (m_style->position() != newStyle.position())) {
      // For changes in positioning styles, we need to conceivably remove
      // ourselves from the positioned objects list.
      toLayoutBox(this)->removeFloatingOrPositionedChildFromBlockLists();
    }

    s_affectsParentBlock =
        isFloatingOrOutOfFlowPositioned() &&
        (!newStyle.isFloating() && !newStyle.hasOutOfFlowPosition()) &&
        parent() &&
        (parent()->isLayoutBlockFlow() || parent()->isLayoutInline());

    // Clearing these bits is required to avoid leaving stale layoutObjects.
    // FIXME: We shouldn't need that hack if our logic was totally correct.
    if (diff.needsLayout()) {
      setFloating(false);
      clearPositionedState();
    }
  } else {
    s_affectsParentBlock = false;
  }

  // Elements with non-auto touch-action will send a SetTouchAction message
  // on touchstart in EventHandler::handleTouchEvent, and so effectively have
  // a touchstart handler that must be reported.
  //
  // Since a CSS property cannot be applied directly to a text node, a
  // handler will have already been added for its parent so ignore it.
  // TODO: Remove this blocking event handler; crbug.com/318381
  TouchAction oldTouchAction =
      m_style ? m_style->getTouchAction() : TouchActionAuto;
  if (node() && !node()->isTextNode() &&
      (oldTouchAction == TouchActionAuto) !=
          (newStyle.getTouchAction() == TouchActionAuto)) {
    EventHandlerRegistry& registry =
        document().frameHost()->eventHandlerRegistry();
    if (newStyle.getTouchAction() != TouchActionAuto)
      registry.didAddEventHandler(
          *node(), EventHandlerRegistry::TouchStartOrMoveEventBlocking);
    else
      registry.didRemoveEventHandler(
          *node(), EventHandlerRegistry::TouchStartOrMoveEventBlocking);
  }
}

void LayoutObject::clearBaseComputedStyle() {
  if (!node())
    return;
  if (!node()->isElementNode())
    return;
  if (ElementAnimations* animations = toElement(node())->elementAnimations())
    animations->clearBaseComputedStyle();
}

static bool areNonIdenticalCursorListsEqual(const ComputedStyle* a,
                                            const ComputedStyle* b) {
  ASSERT(a->cursors() != b->cursors());
  return a->cursors() && b->cursors() && *a->cursors() == *b->cursors();
}

static inline bool areCursorsEqual(const ComputedStyle* a,
                                   const ComputedStyle* b) {
  return a->cursor() == b->cursor() && (a->cursors() == b->cursors() ||
                                        areNonIdenticalCursorListsEqual(a, b));
}

void LayoutObject::setScrollAnchorDisablingStyleChangedOnAncestor() {
  // Walk up the parent chain and find the first scrolling block to disable
  // scroll anchoring on.
  LayoutObject* object = parent();
  Element* viewportDefiningElement = document().viewportDefiningElement();
  while (object) {
    if (object->isLayoutBlock()) {
      LayoutBlock* block = toLayoutBlock(object);
      if (block->hasOverflowClip() ||
          block->node() == viewportDefiningElement) {
        block->setScrollAnchorDisablingStyleChanged(true);
        return;
      }
    }
    object = object->parent();
  }
}

void LayoutObject::styleDidChange(StyleDifference diff,
                                  const ComputedStyle* oldStyle) {
  if (s_affectsParentBlock)
    handleDynamicFloatPositionChange(this);

  if (!m_parent)
    return;

  if (diff.needsFullLayout()) {
    LayoutCounter::layoutObjectStyleChanged(*this, oldStyle, *m_style);

    // If the in-flow state of an element is changed, disable scroll
    // anchoring on the containing scroller.
    if (oldStyle->hasOutOfFlowPosition() != m_style->hasOutOfFlowPosition())
      setScrollAnchorDisablingStyleChangedOnAncestor();

    // If the object already needs layout, then setNeedsLayout won't do
    // any work. But if the containing block has changed, then we may need
    // to mark the new containing blocks for layout. The change that can
    // directly affect the containing block of this object is a change to
    // the position style.
    if (needsLayout() && oldStyle->position() != m_style->position())
      markContainerChainForLayout();

    // Ditto.
    if (needsOverflowRecalcAfterStyleChange() &&
        oldStyle->position() != m_style->position())
      markAncestorsForOverflowRecalcIfNeeded();

    setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
  } else if (diff.needsPositionedMovementLayout()) {
    setNeedsPositionedMovementLayout();
  }

  if (diff.scrollAnchorDisablingPropertyChanged())
    setScrollAnchorDisablingStyleChanged(true);

  // Don't check for paint invalidation here; we need to wait until the layer
  // has been updated by subclasses before we know if we have to invalidate
  // paints (in setStyle()).

  if (oldStyle && !areCursorsEqual(oldStyle, style())) {
    if (LocalFrame* frame = this->frame()) {
      // Cursor update scheduling is done by the local root, which is the main
      // frame if there are no RemoteFrame ancestors in the frame tree. Use of
      // localFrameRoot() is discouraged but will change when cursor update
      // scheduling is moved from EventHandler to PageEventHandler.
      frame->localFrameRoot()->eventHandler().scheduleCursorUpdate();
    }
  }

  if (diff.needsPaintInvalidation() && oldStyle) {
    if (resolveColor(*oldStyle, CSSPropertyBackgroundColor) !=
            resolveColor(CSSPropertyBackgroundColor) ||
        oldStyle->backgroundLayers() != styleRef().backgroundLayers())
      setBackgroundChangedSinceLastPaintInvalidation();
  }
}

void LayoutObject::propagateStyleToAnonymousChildren() {
  // FIXME: We could save this call when the change only affected non-inherited
  // properties.
  for (LayoutObject* child = slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->isAnonymous() || child->style()->styleType() != PseudoIdNone)
      continue;

    if (child->anonymousHasStylePropagationOverride())
      continue;

    RefPtr<ComputedStyle> newStyle =
        ComputedStyle::createAnonymousStyleWithDisplay(
            styleRef(), child->style()->display());

    // Preserve the position style of anonymous block continuations as they can
    // have relative position when they contain block descendants of relative
    // positioned inlines.
    if (child->isInFlowPositioned() && child->isLayoutBlockFlow() &&
        toLayoutBlockFlow(child)->isAnonymousBlockContinuation())
      newStyle->setPosition(child->style()->position());

    updateAnonymousChildStyle(*child, *newStyle);

    child->setStyle(newStyle.release());
  }
}

void LayoutObject::setStyleWithWritingModeOf(PassRefPtr<ComputedStyle> style,
                                             LayoutObject* parent) {
  if (parent)
    style->setWritingMode(parent->styleRef().getWritingMode());
  setStyle(std::move(style));
}

void LayoutObject::setStyleWithWritingModeOfParent(
    PassRefPtr<ComputedStyle> style) {
  setStyleWithWritingModeOf(std::move(style), parent());
}

void LayoutObject::addChildWithWritingModeOfParent(LayoutObject* newChild,
                                                   LayoutObject* beforeChild) {
  if (newChild->mutableStyleRef().setWritingMode(styleRef().getWritingMode()) &&
      newChild->isBoxModelObject()) {
    newChild->setHorizontalWritingMode(isHorizontalWritingMode());
  }
  addChild(newChild, beforeChild);
}

void LayoutObject::updateFillImages(const FillLayer* oldLayers,
                                    const FillLayer& newLayers) {
  // Optimize the common case
  if (FillLayer::imagesIdentical(oldLayers, &newLayers))
    return;

  // Go through the new layers and addClients first, to avoid removing all
  // clients of an image.
  for (const FillLayer* currNew = &newLayers; currNew;
       currNew = currNew->next()) {
    if (currNew->image())
      currNew->image()->addClient(this);
  }

  for (const FillLayer* currOld = oldLayers; currOld;
       currOld = currOld->next()) {
    if (currOld->image())
      currOld->image()->removeClient(this);
  }
}

void LayoutObject::updateCursorImages(const CursorList* oldCursors,
                                      const CursorList* newCursors) {
  if (oldCursors && newCursors && *oldCursors == *newCursors)
    return;

  if (newCursors) {
    for (const CursorData& cursorNew : *newCursors) {
      if (cursorNew.image())
        cursorNew.image()->addClient(this);
    }
  }
  removeCursorImageClient(oldCursors);
}

void LayoutObject::updateImage(StyleImage* oldImage, StyleImage* newImage) {
  if (oldImage != newImage) {
    if (oldImage)
      oldImage->removeClient(this);
    if (newImage)
      newImage->addClient(this);
  }
}

void LayoutObject::updateShapeImage(const ShapeValue* oldShapeValue,
                                    const ShapeValue* newShapeValue) {
  if (oldShapeValue || newShapeValue)
    updateImage(oldShapeValue ? oldShapeValue->image() : 0,
                newShapeValue ? newShapeValue->image() : 0);
}

LayoutRect LayoutObject::viewRect() const {
  return view()->viewRect();
}

FloatPoint LayoutObject::localToAbsolute(const FloatPoint& localPoint,
                                         MapCoordinatesFlags mode) const {
  TransformState transformState(TransformState::ApplyTransformDirection,
                                localPoint);
  mapLocalToAncestor(0, transformState, mode | ApplyContainerFlip);
  transformState.flatten();

  return transformState.lastPlanarPoint();
}

FloatPoint LayoutObject::ancestorToLocal(LayoutBoxModelObject* ancestor,
                                         const FloatPoint& containerPoint,
                                         MapCoordinatesFlags mode) const {
  TransformState transformState(
      TransformState::UnapplyInverseTransformDirection, containerPoint);
  mapAncestorToLocal(ancestor, transformState, mode);
  transformState.flatten();

  return transformState.lastPlanarPoint();
}

FloatQuad LayoutObject::ancestorToLocalQuad(LayoutBoxModelObject* ancestor,
                                            const FloatQuad& quad,
                                            MapCoordinatesFlags mode) const {
  TransformState transformState(
      TransformState::UnapplyInverseTransformDirection,
      quad.boundingBox().center(), quad);
  mapAncestorToLocal(ancestor, transformState, mode);
  transformState.flatten();
  return transformState.lastPlanarQuad();
}

void LayoutObject::mapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                      TransformState& transformState,
                                      MapCoordinatesFlags mode) const {
  if (ancestor == this)
    return;

  bool ancestorSkipped;
  const LayoutObject* container = this->container(ancestor, &ancestorSkipped);
  if (!container)
    return;

  if (mode & ApplyContainerFlip) {
    if (isBox()) {
      mode &= ~ApplyContainerFlip;
    } else if (container->isBox()) {
      if (container->style()->isFlippedBlocksWritingMode()) {
        IntPoint centerPoint = roundedIntPoint(transformState.mappedPoint());
        transformState.move(toLayoutBox(container)->flipForWritingMode(
                                LayoutPoint(centerPoint)) -
                            centerPoint);
      }
      mode &= ~ApplyContainerFlip;
    }
  }

  LayoutSize containerOffset = offsetFromContainer(container);
  if (isLayoutFlowThread()) {
    // So far the point has been in flow thread coordinates (i.e. as if
    // everything in the fragmentation context lived in one tall single column).
    // Convert it to a visual point now, since we're about to escape the flow
    // thread.
    containerOffset += columnOffset(LayoutPoint(transformState.mappedPoint()));
  }

  // Text objects just copy their parent's computed style, so we need to ignore
  // them.
  bool preserve3D =
      mode & UseTransforms &&
      ((container->style()->preserves3D() && !container->isText()) ||
       (style()->preserves3D() && !isText()));
  if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    getTransformFromContainer(container, containerOffset, t);
    transformState.applyTransform(t, preserve3D
                                         ? TransformState::AccumulateTransform
                                         : TransformState::FlattenTransform);
  } else {
    transformState.move(containerOffset.width(), containerOffset.height(),
                        preserve3D ? TransformState::AccumulateTransform
                                   : TransformState::FlattenTransform);
  }

  if (ancestorSkipped) {
    // There can't be a transform between |ancestor| and |o|, because transforms
    // create containers, so it should be safe to just subtract the delta
    // between the ancestor and |o|.
    LayoutSize containerOffset =
        ancestor->offsetFromAncestorContainer(container);
    transformState.move(-containerOffset.width(), -containerOffset.height(),
                        preserve3D ? TransformState::AccumulateTransform
                                   : TransformState::FlattenTransform);
    // If the ancestor is fixed, then the rect is already in its coordinates so
    // doesn't need viewport-adjusting.
    if (ancestor->style()->position() != FixedPosition &&
        container->isLayoutView() && styleRef().position() == FixedPosition) {
      LayoutSize adjustment = toLayoutView(container)->offsetForFixedPosition();
      transformState.move(adjustment.width(), adjustment.height());
    }
    return;
  }

  container->mapLocalToAncestor(ancestor, transformState, mode);
}

const LayoutObject* LayoutObject::pushMappingToContainer(
    const LayoutBoxModelObject* ancestorToStopAt,
    LayoutGeometryMap& geometryMap) const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

void LayoutObject::mapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                      TransformState& transformState,
                                      MapCoordinatesFlags mode) const {
  if (this == ancestor)
    return;

  bool ancestorSkipped;
  LayoutObject* container = this->container(ancestor, &ancestorSkipped);
  if (!container)
    return;

  bool applyContainerFlip = false;
  if (mode & ApplyContainerFlip) {
    if (isBox()) {
      mode &= ~ApplyContainerFlip;
    } else if (container->isBox()) {
      applyContainerFlip = container->style()->isFlippedBlocksWritingMode();
      mode &= ~ApplyContainerFlip;
    }
  }

  if (!ancestorSkipped)
    container->mapAncestorToLocal(ancestor, transformState, mode);

  LayoutSize containerOffset = offsetFromContainer(container);
  if (isLayoutFlowThread()) {
    // Descending into a flow thread. Convert to the local coordinate space,
    // i.e. flow thread coordinates.
    LayoutPoint visualPoint = LayoutPoint(transformState.mappedPoint());
    transformState.move(
        visualPoint -
        toLayoutFlowThread(this)->visualPointToFlowThreadPoint(visualPoint));
  }

  bool preserve3D =
      mode & UseTransforms &&
      (container->style()->preserves3D() || style()->preserves3D());
  if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    getTransformFromContainer(container, containerOffset, t);
    transformState.applyTransform(t, preserve3D
                                         ? TransformState::AccumulateTransform
                                         : TransformState::FlattenTransform);
  } else {
    transformState.move(containerOffset.width(), containerOffset.height(),
                        preserve3D ? TransformState::AccumulateTransform
                                   : TransformState::FlattenTransform);
  }

  if (applyContainerFlip) {
    IntPoint centerPoint = roundedIntPoint(transformState.mappedPoint());
    transformState.move(
        centerPoint -
        toLayoutBox(container)->flipForWritingMode(LayoutPoint(centerPoint)));
  }

  if (ancestorSkipped) {
    containerOffset = ancestor->offsetFromAncestorContainer(container);
    transformState.move(-containerOffset.width(), -containerOffset.height());
    // If the ancestor is fixed, then the rect is already in its coordinates so
    // doesn't need viewport-adjusting.
    if (ancestor->style()->position() != FixedPosition &&
        container->isLayoutView() && styleRef().position() == FixedPosition) {
      LayoutSize adjustment = toLayoutView(container)->offsetForFixedPosition();
      transformState.move(adjustment.width(), adjustment.height());
    }
  }
}

bool LayoutObject::shouldUseTransformFromContainer(
    const LayoutObject* containerObject) const {
  // hasTransform() indicates whether the object has transform, transform-style
  // or perspective. We just care about transform, so check the layer's
  // transform directly.
  return (hasLayer() && toLayoutBoxModelObject(this)->layer()->transform()) ||
         (containerObject && containerObject->style()->hasPerspective());
}

void LayoutObject::getTransformFromContainer(
    const LayoutObject* containerObject,
    const LayoutSize& offsetInContainer,
    TransformationMatrix& transform) const {
  transform.makeIdentity();
  transform.translate(offsetInContainer.width().toFloat(),
                      offsetInContainer.height().toFloat());
  PaintLayer* layer = hasLayer() ? toLayoutBoxModelObject(this)->layer() : 0;
  if (layer && layer->transform())
    transform.multiply(layer->currentTransform());

  if (containerObject && containerObject->hasLayer() &&
      containerObject->style()->hasPerspective()) {
    // Perpsective on the container affects us, so we have to factor it in here.
    ASSERT(containerObject->hasLayer());
    FloatPoint perspectiveOrigin =
        toLayoutBoxModelObject(containerObject)->layer()->perspectiveOrigin();

    TransformationMatrix perspectiveMatrix;
    perspectiveMatrix.applyPerspective(containerObject->style()->perspective());

    transform.translateRight3d(-perspectiveOrigin.x(), -perspectiveOrigin.y(),
                               0);
    transform = perspectiveMatrix * transform;
    transform.translateRight3d(perspectiveOrigin.x(), perspectiveOrigin.y(), 0);
  }
}

FloatQuad LayoutObject::localToAncestorQuad(
    const FloatQuad& localQuad,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  // Track the point at the center of the quad's bounding box. As
  // mapLocalToAncestor() calls offsetFromContainer(), it will use that point
  // as the reference point to decide which column's transform to apply in
  // multiple-column blocks.
  TransformState transformState(TransformState::ApplyTransformDirection,
                                localQuad.boundingBox().center(), localQuad);
  mapLocalToAncestor(ancestor, transformState,
                     mode | ApplyContainerFlip | UseTransforms);
  transformState.flatten();

  return transformState.lastPlanarQuad();
}

FloatPoint LayoutObject::localToAncestorPoint(
    const FloatPoint& localPoint,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  TransformState transformState(TransformState::ApplyTransformDirection,
                                localPoint);
  mapLocalToAncestor(ancestor, transformState,
                     mode | ApplyContainerFlip | UseTransforms);
  transformState.flatten();

  return transformState.lastPlanarPoint();
}

void LayoutObject::localToAncestorRects(Vector<LayoutRect>& rects,
                                        const LayoutBoxModelObject* ancestor,
                                        const LayoutPoint& preOffset,
                                        const LayoutPoint& postOffset) const {
  for (size_t i = 0; i < rects.size(); ++i) {
    LayoutRect& rect = rects[i];
    rect.moveBy(preOffset);
    FloatQuad containerQuad =
        localToAncestorQuad(FloatQuad(FloatRect(rect)), ancestor);
    LayoutRect containerRect = LayoutRect(containerQuad.boundingBox());
    if (containerRect.isEmpty()) {
      rects.remove(i--);
      continue;
    }
    containerRect.moveBy(postOffset);
    rects[i] = containerRect;
  }
}

TransformationMatrix LayoutObject::localToAncestorTransform(
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  TransformState transformState(TransformState::ApplyTransformDirection);
  mapLocalToAncestor(ancestor, transformState,
                     mode | ApplyContainerFlip | UseTransforms);
  return transformState.accumulatedTransform();
}

FloatPoint LayoutObject::localToInvalidationBackingPoint(
    const LayoutPoint& localPoint,
    PaintLayer** backingLayer) {
  const LayoutBoxModelObject& paintInvalidationContainer =
      containerForPaintInvalidation();
  ASSERT(paintInvalidationContainer.layer());

  if (backingLayer)
    *backingLayer = paintInvalidationContainer.layer();
  FloatPoint containerPoint =
      localToAncestorPoint(FloatPoint(localPoint), &paintInvalidationContainer,
                           TraverseDocumentBoundaries);

  // A layoutObject can have no invalidation backing if it is from a detached
  // frame, or when forced compositing is disabled.
  if (paintInvalidationContainer.layer()->compositingState() == NotComposited)
    return containerPoint;

  PaintLayer::mapPointInPaintInvalidationContainerToBacking(
      paintInvalidationContainer, containerPoint);
  return containerPoint;
}

LayoutSize LayoutObject::offsetFromContainer(const LayoutObject* o) const {
  ASSERT(o == container());
  return o->hasOverflowClip()
             ? LayoutSize(-toLayoutBox(o)->scrolledContentOffset())
             : LayoutSize();
}

LayoutSize LayoutObject::offsetFromAncestorContainer(
    const LayoutObject* ancestorContainer) const {
  if (ancestorContainer == this)
    return LayoutSize();

  LayoutSize offset;
  LayoutPoint referencePoint;
  const LayoutObject* currContainer = this;
  do {
    const LayoutObject* nextContainer = currContainer->container();
    // This means we reached the top without finding container.
    DCHECK(nextContainer);
    if (!nextContainer)
      break;
    ASSERT(!currContainer->hasTransformRelatedProperty());
    LayoutSize currentOffset =
        currContainer->offsetFromContainer(nextContainer);
    offset += currentOffset;
    referencePoint.move(currentOffset);
    currContainer = nextContainer;
  } while (currContainer != ancestorContainer);

  return offset;
}

LayoutRect LayoutObject::localCaretRect(InlineBox*,
                                        int,
                                        LayoutUnit* extraWidthToEndOfLine) {
  if (extraWidthToEndOfLine)
    *extraWidthToEndOfLine = LayoutUnit();

  return LayoutRect();
}

void LayoutObject::computeLayerHitTestRects(
    LayerHitTestRects& layerRects) const {
  // Figure out what layer our container is in. Any offset (or new layer) for
  // this layoutObject within it's container will be applied in
  // addLayerHitTestRects.
  LayoutPoint layerOffset;
  const PaintLayer* currentLayer = nullptr;

  if (!hasLayer()) {
    LayoutObject* container = this->container();
    currentLayer = container->enclosingLayer();
    if (container && currentLayer->layoutObject() != container) {
      layerOffset.move(
          container->offsetFromAncestorContainer(currentLayer->layoutObject()));
      // If the layer itself is scrolled, we have to undo the subtraction of its
      // scroll offset since we want the offset relative to the scrolling
      // content, not the element itself.
      if (currentLayer->layoutObject()->hasOverflowClip())
        layerOffset.move(currentLayer->layoutBox()->scrolledContentOffset());
    }
  }

  this->addLayerHitTestRects(layerRects, currentLayer, layerOffset,
                             LayoutRect());
}

void LayoutObject::addLayerHitTestRects(LayerHitTestRects& layerRects,
                                        const PaintLayer* currentLayer,
                                        const LayoutPoint& layerOffset,
                                        const LayoutRect& containerRect) const {
  ASSERT(currentLayer);
  ASSERT(currentLayer == this->enclosingLayer());

  // Compute the rects for this layoutObject only and add them to the results.
  // Note that we could avoid passing the offset and instead adjust each result,
  // but this seems slightly simpler.
  Vector<LayoutRect> ownRects;
  LayoutRect newContainerRect;
  computeSelfHitTestRects(ownRects, layerOffset);

  // When we get to have a lot of rects on a layer, the performance cost of
  // tracking those rects outweighs the benefit of doing compositor thread hit
  // testing.
  // FIXME: This limit needs to be low due to the O(n^2) algorithm in
  // WebLayer::setTouchEventHandlerRegion - crbug.com/300282.
  const size_t maxRectsPerLayer = 100;

  LayerHitTestRects::iterator iter = layerRects.find(currentLayer);
  Vector<LayoutRect>* iterValue;
  if (iter == layerRects.end())
    iterValue =
        &layerRects.add(currentLayer, Vector<LayoutRect>()).storedValue->value;
  else
    iterValue = &iter->value;
  for (size_t i = 0; i < ownRects.size(); i++) {
    if (!containerRect.contains(ownRects[i])) {
      iterValue->append(ownRects[i]);
      if (iterValue->size() > maxRectsPerLayer) {
        // Just mark the entire layer instead, and switch to walking the layer
        // tree instead of the layout tree.
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

  // If it's possible for children to have rects outside our bounds, then we
  // need to descend into the children and compute them.
  // Ideally there would be other cases where we could detect that children
  // couldn't have rects outside our bounds and prune the tree walk.
  // Note that we don't use Region here because Union is O(N) - better to just
  // keep a list of partially redundant rectangles. If we find examples where
  // this is expensive, then we could rewrite Region to be more efficient. See
  // https://bugs.webkit.org/show_bug.cgi?id=100814.
  if (!isLayoutView()) {
    for (LayoutObject* curr = slowFirstChild(); curr;
         curr = curr->nextSibling()) {
      curr->addLayerHitTestRects(layerRects, currentLayer, layerOffset,
                                 newContainerRect);
    }
  }
}

bool LayoutObject::isRooted() const {
  const LayoutObject* object = this;
  while (object->parent() && !object->hasLayer())
    object = object->parent();
  if (object->hasLayer())
    return toLayoutBoxModelObject(object)->layer()->root()->isRootLayer();
  return false;
}

RespectImageOrientationEnum LayoutObject::shouldRespectImageOrientation(
    const LayoutObject* layoutObject) {
  if (!layoutObject)
    return DoNotRespectImageOrientation;

  // Respect the image's orientation if it's being used as a full-page image or
  // it's an <img> and the setting to respect it everywhere is set or the <img>
  // has image-orientation: from-image style. FIXME: crbug.com/498233
  if (layoutObject->document().isImageDocument())
    return RespectImageOrientation;

  if (!isHTMLImageElement(layoutObject->node()))
    return DoNotRespectImageOrientation;

  if (layoutObject->document().settings() &&
      layoutObject->document().settings()->shouldRespectImageOrientation())
    return RespectImageOrientation;

  if (layoutObject->style() &&
      layoutObject->style()->respectImageOrientation() ==
          RespectImageOrientation)
    return RespectImageOrientation;

  return DoNotRespectImageOrientation;
}

LayoutObject* LayoutObject::container(const LayoutBoxModelObject* ancestor,
                                      bool* ancestorSkipped,
                                      bool* filterSkipped) const {
  if (ancestorSkipped)
    *ancestorSkipped = false;
  if (filterSkipped)
    *filterSkipped = false;

  LayoutObject* o = parent();

  if (isTextOrSVGChild())
    return o;

  EPosition pos = m_style->position();
  if (pos == FixedPosition)
    return containerForFixedPosition(ancestor, ancestorSkipped, filterSkipped);

  if (pos == AbsolutePosition)
    return containerForAbsolutePosition(ancestor, ancestorSkipped,
                                        filterSkipped);

  if (isColumnSpanAll()) {
    LayoutObject* multicolContainer = spannerPlaceholder()->container();
    if ((ancestorSkipped && ancestor) || filterSkipped) {
      // We jumped directly from the spanner to the multicol container. Need to
      // check if we skipped |ancestor| or filter/reflection on the way.
      for (LayoutObject* walker = parent();
           walker && walker != multicolContainer; walker = walker->parent()) {
        if (ancestorSkipped && walker == ancestor)
          *ancestorSkipped = true;
        if (filterSkipped && walker->hasFilterInducingProperty())
          *filterSkipped = true;
      }
    }
    return multicolContainer;
  }

  return o;
}

LayoutObject* LayoutObject::paintInvalidationParent() const {
  if (isLayoutView())
    return LayoutAPIShim::layoutObjectFrom(frame()->ownerLayoutItem());
  if (isColumnSpanAll())
    return spannerPlaceholder();
  return parent();
}

bool LayoutObject::isSelectionBorder() const {
  SelectionState st = getSelectionState();
  return st == SelectionStart || st == SelectionEnd || st == SelectionBoth;
}

inline void LayoutObject::clearLayoutRootIfNeeded() const {
  if (FrameView* view = frameView()) {
    if (!documentBeingDestroyed())
      view->clearLayoutSubtreeRoot(*this);
  }
}

void LayoutObject::willBeDestroyed() {
  // Destroy any leftover anonymous children.
  LayoutObjectChildList* children = virtualChildren();
  if (children)
    children->destroyLeftoverChildren();

  if (LocalFrame* frame = this->frame()) {
    // If this layoutObject is being autoscrolled, stop the autoscrolling.
    if (frame->page())
      frame->page()->autoscrollController().stopAutoscrollIfNeeded(this);
  }

  // For accessibility management, notify the parent of the imminent change to
  // its child set.
  // We do it now, before remove(), while the parent pointer is still available.
  if (AXObjectCache* cache = document().existingAXObjectCache())
    cache->childrenChanged(this->parent());

  remove();

  // The remove() call above may invoke axObjectCache()->childrenChanged() on
  // the parent, which may require the AX layout object for this layoutObject.
  // So we remove the AX layout object now, after the layoutObject is removed.
  if (AXObjectCache* cache = document().existingAXObjectCache())
    cache->remove(this);

  // If this layoutObject had a parent, remove should have destroyed any
  // counters attached to this layoutObject and marked the affected other
  // counters for reevaluation. This apparently redundant check is here for the
  // case when this layoutObject had no parent at the time remove() was called.

  if (hasCounterNodeMap())
    LayoutCounter::destroyCounterNodes(*this);

  // Remove the handler if node had touch-action set. Handlers are not added
  // for text nodes so don't try removing for one too. Need to check if
  // m_style is null in cases of partial construction. Any handler we added
  // previously may have already been removed by the Document independently.
  if (node() && !node()->isTextNode() && m_style &&
      m_style->getTouchAction() != TouchActionAuto) {
    EventHandlerRegistry& registry =
        document().frameHost()->eventHandlerRegistry();
    if (registry
            .eventHandlerTargets(
                EventHandlerRegistry::TouchStartOrMoveEventBlocking)
            ->contains(node()))
      registry.didRemoveEventHandler(
          *node(), EventHandlerRegistry::TouchStartOrMoveEventBlocking);
  }

  setAncestorLineBoxDirty(false);

  ObjectPaintInvalidator::objectWillBeDestroyed(*this);

  clearLayoutRootIfNeeded();

  if (m_style) {
    for (const FillLayer* bgLayer = &m_style->backgroundLayers(); bgLayer;
         bgLayer = bgLayer->next()) {
      if (StyleImage* backgroundImage = bgLayer->image())
        backgroundImage->removeClient(this);
    }

    for (const FillLayer* maskLayer = &m_style->maskLayers(); maskLayer;
         maskLayer = maskLayer->next()) {
      if (StyleImage* maskImage = maskLayer->image())
        maskImage->removeClient(this);
    }

    if (StyleImage* borderImage = m_style->borderImage().image())
      borderImage->removeClient(this);

    if (StyleImage* maskBoxImage = m_style->maskBoxImage().image())
      maskBoxImage->removeClient(this);

    if (m_style->contentData() && m_style->contentData()->isImage())
      toImageContentData(m_style->contentData())->image()->removeClient(this);

    if (m_style->boxReflect() && m_style->boxReflect()->mask().image())
      m_style->boxReflect()->mask().image()->removeClient(this);

    removeShapeImageClient(m_style->shapeOutside());
    removeCursorImageClient(m_style->cursors());
  }

  if (frameView())
    setIsBackgroundAttachmentFixedObject(false);
}

DISABLE_CFI_PERF
void LayoutObject::insertedIntoTree() {
  // FIXME: We should ASSERT(isRooted()) here but generated content makes some
  // out-of-order insertion.

  // Keep our layer hierarchy updated. Optimize for the common case where we
  // don't have any children and don't have a layer attached to ourselves.
  PaintLayer* layer = nullptr;
  if (slowFirstChild() || hasLayer()) {
    layer = parent()->enclosingLayer();
    addLayers(layer);
  }

  // If |this| is visible but this object was not, tell the layer it has some
  // visible content that needs to be drawn and layer visibility optimization
  // can't be used
  if (parent()->style()->visibility() != EVisibility::Visible &&
      style()->visibility() == EVisibility::Visible && !hasLayer()) {
    if (!layer)
      layer = parent()->enclosingLayer();
    if (layer)
      layer->dirtyVisibleContentStatus();
  }

  if (parent()->childrenInline())
    parent()->dirtyLinesFromChangedChild(this);

  if (LayoutFlowThread* flowThread = flowThreadContainingBlock())
    flowThread->flowThreadDescendantWasInserted(this);
}

enum FindReferencingScrollAnchorsBehavior { DontClear, Clear };

static bool findReferencingScrollAnchors(
    LayoutObject* layoutObject,
    FindReferencingScrollAnchorsBehavior behavior) {
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layoutObject->parent())
    layer = parent->enclosingLayer();
  bool found = false;

  // Walk up the layer tree to clear any scroll anchors that reference us.
  while (layer) {
    if (PaintLayerScrollableArea* scrollableArea = layer->getScrollableArea()) {
      ScrollAnchor* anchor = scrollableArea->scrollAnchor();
      DCHECK(anchor);
      if (anchor->refersTo(layoutObject)) {
        found = true;
        if (behavior == Clear)
          anchor->notifyRemoved(layoutObject);
        else
          return true;
      }
    }
    layer = layer->parent();
  }
  if (FrameView* view = layoutObject->frameView()) {
    ScrollAnchor* anchor = view->scrollAnchor();
    DCHECK(anchor);
    if (anchor->refersTo(layoutObject)) {
      found = true;
      if (behavior == Clear)
        anchor->notifyRemoved(layoutObject);
    }
  }
  return found;
}

void LayoutObject::willBeRemovedFromTree() {
  // FIXME: We should ASSERT(isRooted()) but we have some out-of-order removals
  // which would need to be fixed first.

  // If we remove a visible child from an invisible parent, we don't know the
  // layer visibility any more.
  PaintLayer* layer = nullptr;
  if (parent()->style()->visibility() != EVisibility::Visible &&
      style()->visibility() == EVisibility::Visible && !hasLayer()) {
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

  removeFromLayoutFlowThread();

  // Update cached boundaries in SVG layoutObjects if a child is removed.
  if (parent()->isSVG())
    parent()->setNeedsBoundariesUpdate();

  if (RuntimeEnabledFeatures::scrollAnchoringEnabled() &&
      m_bitfields.isScrollAnchorObject()) {
    // Clear the bit first so that anchor.clear() doesn't recurse into
    // findReferencingScrollAnchors.
    m_bitfields.setIsScrollAnchorObject(false);
    findReferencingScrollAnchors(this, Clear);
  }
}

void LayoutObject::maybeClearIsScrollAnchorObject() {
  if (!m_bitfields.isScrollAnchorObject())
    return;
  m_bitfields.setIsScrollAnchorObject(
      findReferencingScrollAnchors(this, DontClear));
}

void LayoutObject::removeFromLayoutFlowThread() {
  if (!isInsideFlowThread())
    return;

  // Sometimes we remove the element from the flow, but it's not destroyed at
  // that time.
  // It's only until later when we actually destroy it and remove all the
  // children from it.
  // Currently, that happens for firstLetter elements and list markers.
  // Pass in the flow thread so that we don't have to look it up for all the
  // children.
  // If we're a column spanner, we need to use our parent to find the flow
  // thread, since a spanner doesn't have the flow thread in its containing
  // block chain. We still need to notify the flow thread when the layoutObject
  // removed happens to be a spanner, so that we get rid of the spanner
  // placeholder, and column sets around the placeholder get merged.
  LayoutFlowThread* flowThread = isColumnSpanAll()
                                     ? parent()->flowThreadContainingBlock()
                                     : flowThreadContainingBlock();
  removeFromLayoutFlowThreadRecursive(flowThread);
}

void LayoutObject::removeFromLayoutFlowThreadRecursive(
    LayoutFlowThread* layoutFlowThread) {
  if (const LayoutObjectChildList* children = virtualChildren()) {
    for (LayoutObject* child = children->firstChild(); child;
         child = child->nextSibling()) {
      if (child->isLayoutFlowThread())
        continue;  // Don't descend into inner fragmentation contexts.
      child->removeFromLayoutFlowThreadRecursive(child->isLayoutFlowThread()
                                                     ? toLayoutFlowThread(child)
                                                     : layoutFlowThread);
    }
  }

  if (layoutFlowThread && layoutFlowThread != this)
    layoutFlowThread->flowThreadDescendantWillBeRemoved(this);
  setIsInsideFlowThread(false);
  RELEASE_ASSERT(!spannerPlaceholder());
}

void LayoutObject::destroyAndCleanupAnonymousWrappers() {
  // If the tree is destroyed, there is no need for a clean-up phase.
  if (documentBeingDestroyed()) {
    destroy();
    return;
  }

  LayoutObject* destroyRoot = this;
  for (LayoutObject *destroyRootParent = destroyRoot->parent();
       destroyRootParent && destroyRootParent->isAnonymous();
       destroyRoot = destroyRootParent,
                    destroyRootParent = destroyRootParent->parent()) {
    // Anonymous block continuations are tracked and destroyed elsewhere (see
    // the bottom of LayoutBlock::removeChild)
    if (destroyRootParent->isLayoutBlockFlow() &&
        toLayoutBlockFlow(destroyRootParent)->isAnonymousBlockContinuation())
      break;
    // A flow thread is tracked by its containing block. Whether its children
    // are removed or not is irrelevant.
    if (destroyRootParent->isLayoutFlowThread())
      break;

    if (destroyRootParent->slowFirstChild() != destroyRoot ||
        destroyRootParent->slowLastChild() != destroyRoot)
      break;  // Need to keep the anonymous parent, since it won't become empty
              // by the removal of this layoutObject.
  }

  destroyRoot->destroy();

  // WARNING: |this| is deleted here.
}

void LayoutObject::destroy() {
  willBeDestroyed();
  delete this;
}

DISABLE_CFI_PERF
void LayoutObject::removeShapeImageClient(ShapeValue* shapeValue) {
  if (!shapeValue)
    return;
  if (StyleImage* shapeImage = shapeValue->image())
    shapeImage->removeClient(this);
}

void LayoutObject::removeCursorImageClient(const CursorList* cursorList) {
  if (!cursorList)
    return;
  for (const CursorData& cursor : *cursorList) {
    if (cursor.image())
      cursor.image()->removeClient(this);
  }
}

PositionWithAffinity LayoutObject::positionForPoint(const LayoutPoint&) {
  return createPositionWithAffinity(caretMinOffset());
}

CompositingState LayoutObject::compositingState() const {
  return hasLayer() ? toLayoutBoxModelObject(this)->layer()->compositingState()
                    : NotComposited;
}

CompositingReasons LayoutObject::additionalCompositingReasons() const {
  return CompositingReasonNone;
}

bool LayoutObject::hitTest(HitTestResult& result,
                           const HitTestLocation& locationInContainer,
                           const LayoutPoint& accumulatedOffset,
                           HitTestFilter hitTestFilter) {
  bool inside = false;
  if (hitTestFilter != HitTestSelf) {
    // First test the foreground layer (lines and inlines).
    inside = nodeAtPoint(result, locationInContainer, accumulatedOffset,
                         HitTestForeground);

    // Test floats next.
    if (!inside)
      inside = nodeAtPoint(result, locationInContainer, accumulatedOffset,
                           HitTestFloat);

    // Finally test to see if the mouse is in the background (within a child
    // block's background).
    if (!inside)
      inside = nodeAtPoint(result, locationInContainer, accumulatedOffset,
                           HitTestChildBlockBackgrounds);
  }

  // See if the mouse is inside us but not any of our descendants
  if (hitTestFilter != HitTestDescendants && !inside)
    inside = nodeAtPoint(result, locationInContainer, accumulatedOffset,
                         HitTestBlockBackground);

  return inside;
}

void LayoutObject::updateHitTestResult(HitTestResult& result,
                                       const LayoutPoint& point) {
  if (result.innerNode())
    return;

  Node* node = this->node();

  // If we hit the anonymous layoutObjects inside generated content we should
  // actually hit the generated content so walk up to the PseudoElement.
  if (!node && parent() && parent()->isBeforeOrAfterContent()) {
    for (LayoutObject* layoutObject = parent(); layoutObject && !node;
         layoutObject = layoutObject->parent())
      node = layoutObject->node();
  }

  if (node)
    result.setNodeAndPosition(node, point);
}

bool LayoutObject::nodeAtPoint(HitTestResult&,
                               const HitTestLocation& /*locationInContainer*/,
                               const LayoutPoint& /*accumulatedOffset*/,
                               HitTestAction) {
  return false;
}

void LayoutObject::scheduleRelayout() {
  if (isLayoutView()) {
    FrameView* view = toLayoutView(this)->frameView();
    if (view)
      view->scheduleRelayout();
  } else {
    if (isRooted()) {
      if (LayoutView* layoutView = view()) {
        if (FrameView* frameView = layoutView->frameView())
          frameView->scheduleRelayoutOfSubtree(this);
      }
    }
  }
}

void LayoutObject::forceLayout() {
  setSelfNeedsLayout(true);
  setShouldDoFullPaintInvalidation();
  layout();
}

// FIXME: Does this do anything different than forceLayout given that we don't
// walk the containing block chain. If not, we should change all callers to use
// forceLayout.
void LayoutObject::forceChildLayout() {
  setNormalChildNeedsLayout(true);
  layout();
}

enum StyleCacheState { Cached, Uncached };

static PassRefPtr<ComputedStyle> firstLineStyleForCachedUncachedType(
    StyleCacheState type,
    const LayoutObject* layoutObject,
    ComputedStyle* style) {
  const LayoutObject* layoutObjectForFirstLineStyle = layoutObject;
  if (layoutObject->isBeforeOrAfterContent())
    layoutObjectForFirstLineStyle = layoutObject->parent();

  if (layoutObjectForFirstLineStyle->behavesLikeBlockContainer()) {
    if (const LayoutBlock* firstLineBlock =
            toLayoutBlock(layoutObjectForFirstLineStyle)
                ->enclosingFirstLineStyleBlock()) {
      if (type == Cached)
        return firstLineBlock->getCachedPseudoStyle(PseudoIdFirstLine, style);
      return firstLineBlock->getUncachedPseudoStyle(
          PseudoStyleRequest(PseudoIdFirstLine), style,
          firstLineBlock == layoutObject ? style : 0);
    }
  } else if (!layoutObjectForFirstLineStyle->isAnonymous() &&
             layoutObjectForFirstLineStyle->isLayoutInline() &&
             !layoutObjectForFirstLineStyle->node()
                  ->isFirstLetterPseudoElement()) {
    const ComputedStyle* parentStyle =
        layoutObjectForFirstLineStyle->parent()->firstLineStyle();
    if (parentStyle != layoutObjectForFirstLineStyle->parent()->style()) {
      if (type == Cached) {
        // A first-line style is in effect. Cache a first-line style for
        // ourselves.
        layoutObjectForFirstLineStyle->mutableStyleRef().setHasPseudoStyle(
            PseudoIdFirstLineInherited);
        return layoutObjectForFirstLineStyle->getCachedPseudoStyle(
            PseudoIdFirstLineInherited, parentStyle);
      }
      return layoutObjectForFirstLineStyle->getUncachedPseudoStyle(
          PseudoStyleRequest(PseudoIdFirstLineInherited), parentStyle, style);
    }
  }
  return nullptr;
}

PassRefPtr<ComputedStyle> LayoutObject::uncachedFirstLineStyle(
    ComputedStyle* style) const {
  if (!document().styleEngine().usesFirstLineRules())
    return nullptr;

  ASSERT(!isText());

  return firstLineStyleForCachedUncachedType(Uncached, this, style);
}

ComputedStyle* LayoutObject::cachedFirstLineStyle() const {
  ASSERT(document().styleEngine().usesFirstLineRules());

  if (RefPtr<ComputedStyle> style = firstLineStyleForCachedUncachedType(
          Cached, isText() ? parent() : this, m_style.get()))
    return style.get();

  return m_style.get();
}

ComputedStyle* LayoutObject::getCachedPseudoStyle(
    PseudoId pseudo,
    const ComputedStyle* parentStyle) const {
  if (pseudo < FirstInternalPseudoId && !style()->hasPseudoStyle(pseudo))
    return nullptr;

  ComputedStyle* cachedStyle = style()->getCachedPseudoStyle(pseudo);
  if (cachedStyle)
    return cachedStyle;

  RefPtr<ComputedStyle> result =
      getUncachedPseudoStyle(PseudoStyleRequest(pseudo), parentStyle);
  if (result)
    return mutableStyleRef().addCachedPseudoStyle(result.release());
  return nullptr;
}

PassRefPtr<ComputedStyle> LayoutObject::getUncachedPseudoStyle(
    const PseudoStyleRequest& pseudoStyleRequest,
    const ComputedStyle* parentStyle,
    const ComputedStyle* ownStyle) const {
  if (pseudoStyleRequest.pseudoId < FirstInternalPseudoId && !ownStyle &&
      !style()->hasPseudoStyle(pseudoStyleRequest.pseudoId))
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

  if (pseudoStyleRequest.pseudoId == PseudoIdFirstLineInherited) {
    RefPtr<ComputedStyle> result =
        document().ensureStyleResolver().styleForElement(element, parentStyle,
                                                         DisallowStyleSharing);
    result->setStyleType(PseudoIdFirstLineInherited);
    return result.release();
  }

  return document().ensureStyleResolver().pseudoStyleForElement(
      element, pseudoStyleRequest, parentStyle);
}

PassRefPtr<ComputedStyle>
LayoutObject::getUncachedPseudoStyleFromParentOrShadowHost() const {
  if (!node())
    return nullptr;

  if (ShadowRoot* root = node()->containingShadowRoot()) {
    if (root->type() == ShadowRootType::UserAgent) {
      if (Element* shadowHost = node()->ownerShadowHost()) {
        return shadowHost->layoutObject()->getUncachedPseudoStyle(
            PseudoStyleRequest(PseudoIdSelection));
      }
    }
  }

  return getUncachedPseudoStyle(PseudoStyleRequest(PseudoIdSelection));
}

void LayoutObject::getTextDecorations(unsigned decorations,
                                      AppliedTextDecoration& underline,
                                      AppliedTextDecoration& overline,
                                      AppliedTextDecoration& linethrough,
                                      bool quirksMode,
                                      bool firstlineStyle) {
  LayoutObject* curr = this;
  const ComputedStyle* styleToUse = nullptr;
  unsigned currDecs = TextDecorationNone;
  Color resultColor;
  TextDecorationStyle resultStyle;
  do {
    styleToUse = curr->style(firstlineStyle);
    currDecs = styleToUse->getTextDecoration();
    currDecs &= decorations;
    resultColor =
        styleToUse->visitedDependentColor(CSSPropertyTextDecorationColor);
    resultStyle = styleToUse->getTextDecorationStyle();
    // Parameter 'decorations' is cast as an int to enable the bitwise
    // operations below.
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
    if (curr && curr->isAnonymousBlock() && curr->isLayoutBlockFlow() &&
        toLayoutBlockFlow(curr)->continuation())
      curr = toLayoutBlockFlow(curr)->continuation();
  } while (curr && decorations && (!quirksMode || !curr->node() ||
                                   (!isHTMLAnchorElement(*curr->node()) &&
                                    !isHTMLFontElement(*curr->node()))));

  // If we bailed out, use the element we bailed out at (typically a <font> or
  // <a> element).
  if (decorations && curr) {
    styleToUse = curr->style(firstlineStyle);
    resultColor =
        styleToUse->visitedDependentColor(CSSPropertyTextDecorationColor);
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

void LayoutObject::addAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  // Convert the style regions to absolute coordinates.
  if (style()->visibility() != EVisibility::Visible || !isBox())
    return;

  if (style()->getDraggableRegionMode() == DraggableRegionNone)
    return;

  LayoutBox* box = toLayoutBox(this);
  FloatRect localBounds(FloatPoint(), FloatSize(box->size()));
  FloatRect absBounds = localToAbsoluteQuad(localBounds).boundingBox();

  AnnotatedRegionValue region;
  region.draggable = style()->getDraggableRegionMode() == DraggableRegionDrag;
  region.bounds = LayoutRect(absBounds);
  regions.append(region);
}

bool LayoutObject::willRenderImage() {
  // Without visibility we won't render (and therefore don't care about
  // animation).
  if (style()->visibility() != EVisibility::Visible)
    return false;

  // We will not render a new image when Active DOM is suspended
  if (document().activeDOMObjectsAreSuspended())
    return false;

  // If we're not in a window (i.e., we're dormant from being in a background
  // tab) then we don't want to render either.
  return document().view()->isVisible();
}

bool LayoutObject::getImageAnimationPolicy(ImageAnimationPolicy& policy) {
  if (!document().settings())
    return false;
  policy = document().settings()->imageAnimationPolicy();
  return true;
}

int LayoutObject::caretMinOffset() const {
  return 0;
}

int LayoutObject::caretMaxOffset() const {
  if (isAtomicInlineLevel())
    return node() ? std::max(1U, node()->countChildren()) : 1;
  if (isHR())
    return 1;
  return 0;
}

bool LayoutObject::isInert() const {
  const LayoutObject* layoutObject = this;
  while (!layoutObject->node())
    layoutObject = layoutObject->parent();
  return layoutObject->node()->isInert();
}

void LayoutObject::imageChanged(ImageResource* image, const IntRect* rect) {
  ASSERT(m_node);

  // Image change notifications should not be received during paint because
  // the resulting invalidations will be cleared following paint. This can also
  // lead to modifying the tree out from under paint(), see: crbug.com/616700.
  DCHECK(document().lifecycle().state() !=
         DocumentLifecycle::LifecycleState::InPaint);

  imageChanged(static_cast<WrappedImagePtr>(image), rect);
}

Element* LayoutObject::offsetParent(const Element* base) const {
  if (isDocumentElement() || isBody())
    return nullptr;

  if (isFixedPositioned())
    return nullptr;

  float effectiveZoom = style()->effectiveZoom();
  Node* node = nullptr;
  for (LayoutObject* ancestor = parent(); ancestor;
       ancestor = ancestor->parent()) {
    // Spec: http://www.w3.org/TR/cssom-view/#offset-attributes

    node = ancestor->node();

    if (!node)
      continue;

    // TODO(kochi): If |base| or |node| is nested deep in shadow roots, this
    // loop may get expensive, as isUnclosedNodeOf() can take up to O(N+M) time
    // (N and M are depths).
    if (base &&
        (node->isClosedShadowHiddenFrom(*base) ||
         (node->isInShadowTree() &&
          node->containingShadowRoot()->type() == ShadowRootType::UserAgent))) {
      // If 'position: fixed' node is found while traversing up, terminate the
      // loop and return null.
      if (ancestor->isFixedPositioned())
        return nullptr;
      continue;
    }

    if (ancestor->canContainAbsolutePositionObjects())
      break;

    if (isHTMLBodyElement(*node))
      break;

    if (!isPositioned() &&
        (isHTMLTableElement(*node) || isHTMLTableCellElement(*node)))
      break;

    // Webkit specific extension where offsetParent stops at zoom level changes.
    if (effectiveZoom != ancestor->style()->effectiveZoom())
      break;
  }

  return node && node->isElementNode() ? toElement(node) : nullptr;
}

PositionWithAffinity LayoutObject::createPositionWithAffinity(
    int offset,
    TextAffinity affinity) {
  // If this is a non-anonymous layoutObject in an editable area, then it's
  // simple.
  if (Node* node = nonPseudoNode()) {
    if (!hasEditableStyle(*node)) {
      // If it can be found, we prefer a visually equivalent position that is
      // editable.
      const Position position = Position(node, offset);
      Position candidate =
          mostForwardCaretPosition(position, CanCrossEditingBoundary);
      if (hasEditableStyle(*candidate.anchorNode()))
        return PositionWithAffinity(candidate, affinity);
      candidate = mostBackwardCaretPosition(position, CanCrossEditingBoundary);
      if (hasEditableStyle(*candidate.anchorNode()))
        return PositionWithAffinity(candidate, affinity);
    }
    // FIXME: Eliminate legacy editing positions
    return PositionWithAffinity(Position::editingPositionOf(node, offset),
                                affinity);
  }

  // We don't want to cross the boundary between editable and non-editable
  // regions of the document, but that is either impossible or at least
  // extremely unlikely in any normal case because we stop as soon as we
  // find a single non-anonymous layoutObject.

  // Find a nearby non-anonymous layoutObject.
  LayoutObject* child = this;
  while (LayoutObject* parent = child->parent()) {
    // Find non-anonymous content after.
    for (LayoutObject* layoutObject = child->nextInPreOrder(parent);
         layoutObject; layoutObject = layoutObject->nextInPreOrder(parent)) {
      if (Node* node = layoutObject->nonPseudoNode())
        return PositionWithAffinity(firstPositionInOrBeforeNode(node));
    }

    // Find non-anonymous content before.
    for (LayoutObject* layoutObject = child->previousInPreOrder(); layoutObject;
         layoutObject = layoutObject->previousInPreOrder()) {
      if (layoutObject == parent)
        break;
      if (Node* node = layoutObject->nonPseudoNode())
        return PositionWithAffinity(lastPositionInOrAfterNode(node));
    }

    // Use the parent itself unless it too is anonymous.
    if (Node* node = parent->nonPseudoNode())
      return PositionWithAffinity(firstPositionInOrBeforeNode(node));

    // Repeat at the next level up.
    child = parent;
  }

  // Everything was anonymous. Give up.
  return PositionWithAffinity();
}

PositionWithAffinity LayoutObject::createPositionWithAffinity(int offset) {
  return createPositionWithAffinity(offset, TextAffinity::Downstream);
}

PositionWithAffinity LayoutObject::createPositionWithAffinity(
    const Position& position) {
  if (position.isNotNull())
    return PositionWithAffinity(position);

  ASSERT(!node());
  return createPositionWithAffinity(0);
}

CursorDirective LayoutObject::getCursor(const LayoutPoint&, Cursor&) const {
  return SetCursorBasedOnStyle;
}

bool LayoutObject::canUpdateSelectionOnRootLineBoxes() const {
  if (needsLayout())
    return false;

  const LayoutBlock* containingBlock = this->containingBlock();
  return containingBlock ? !containingBlock->needsLayout() : false;
}

void LayoutObject::setNeedsBoundariesUpdate() {
  if (LayoutObject* layoutObject = parent())
    layoutObject->setNeedsBoundariesUpdate();
}

FloatRect LayoutObject::objectBoundingBox() const {
  NOTREACHED();
  return FloatRect();
}

FloatRect LayoutObject::strokeBoundingBox() const {
  NOTREACHED();
  return FloatRect();
}

FloatRect LayoutObject::visualRectInLocalSVGCoordinates() const {
  NOTREACHED();
  return FloatRect();
}

AffineTransform LayoutObject::localSVGTransform() const {
  static const AffineTransform identity;
  return identity;
}

const AffineTransform& LayoutObject::localToSVGParentTransform() const {
  static const AffineTransform identity;
  return identity;
}

bool LayoutObject::nodeAtFloatPoint(HitTestResult&,
                                    const FloatPoint&,
                                    HitTestAction) {
  ASSERT_NOT_REACHED();
  return false;
}

bool LayoutObject::isRelayoutBoundaryForInspector() const {
  return objectIsRelayoutBoundary(this);
}

static PaintInvalidationReason documentLifecycleBasedPaintInvalidationReason(
    const DocumentLifecycle& documentLifecycle) {
  switch (documentLifecycle.state()) {
    case DocumentLifecycle::InStyleRecalc:
      return PaintInvalidationStyleChange;
    case DocumentLifecycle::InPreLayout:
    case DocumentLifecycle::InPerformLayout:
    case DocumentLifecycle::AfterPerformLayout:
      return PaintInvalidationForcedByLayout;
    case DocumentLifecycle::InCompositingUpdate:
      return PaintInvalidationCompositingUpdate;
    default:
      return PaintInvalidationFull;
  }
}

inline void LayoutObject::markAncestorsForPaintInvalidation() {
  for (
      LayoutObject* parent = this->paintInvalidationParent();
      parent &&
      !parent
           ->shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState();
      parent = parent->paintInvalidationParent())
    parent->m_bitfields.setChildShouldCheckForPaintInvalidation(true);
}

void LayoutObject::setShouldInvalidateSelection() {
  if (!canUpdateSelectionOnRootLineBoxes())
    return;
  m_bitfields.setShouldInvalidateSelection(true);
  markAncestorsForPaintInvalidation();
  frameView()->scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void LayoutObject::setShouldDoFullPaintInvalidation(
    PaintInvalidationReason reason) {
  // Only full invalidation reasons are allowed.
  ASSERT(isFullPaintInvalidationReason(reason));

  bool isUpgradingDelayedFullToFull =
      m_bitfields.fullPaintInvalidationReason() ==
          PaintInvalidationDelayedFull &&
      reason != PaintInvalidationDelayedFull;

  if (m_bitfields.fullPaintInvalidationReason() == PaintInvalidationNone ||
      isUpgradingDelayedFullToFull) {
    if (reason == PaintInvalidationFull)
      reason =
          documentLifecycleBasedPaintInvalidationReason(document().lifecycle());
    m_bitfields.setFullPaintInvalidationReason(reason);
    if (!isUpgradingDelayedFullToFull)
      markAncestorsForPaintInvalidation();
  }

  frameView()->scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void LayoutObject::setMayNeedPaintInvalidation() {
  if (mayNeedPaintInvalidation())
    return;
  m_bitfields.setMayNeedPaintInvalidation(true);
  markAncestorsForPaintInvalidation();
  frameView()->scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void LayoutObject::setMayNeedPaintInvalidationSubtree() {
  if (mayNeedPaintInvalidationSubtree())
    return;
  m_bitfields.setMayNeedPaintInvalidationSubtree(true);
  setMayNeedPaintInvalidation();
}

void LayoutObject::setMayNeedPaintInvalidationAnimatedBackgroundImage() {
  if (mayNeedPaintInvalidationAnimatedBackgroundImage())
    return;
  m_bitfields.setMayNeedPaintInvalidationAnimatedBackgroundImage(true);
  setMayNeedPaintInvalidation();
}

void LayoutObject::clearPaintInvalidationFlags() {
  // paintInvalidationStateIsDirty should be kept in sync with the
  // booleans that are cleared below.
  ASSERT(!shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState() ||
         paintInvalidationStateIsDirty());
  clearShouldDoFullPaintInvalidation();
  m_bitfields.setChildShouldCheckForPaintInvalidation(false);
  m_bitfields.setMayNeedPaintInvalidation(false);
  m_bitfields.setMayNeedPaintInvalidationSubtree(false);
  m_bitfields.setMayNeedPaintInvalidationAnimatedBackgroundImage(false);
  m_bitfields.setShouldInvalidateSelection(false);
  m_bitfields.setBackgroundChangedSinceLastPaintInvalidation(false);
}

bool LayoutObject::isAllowedToModifyLayoutTreeStructure(Document& document) {
  return DeprecatedDisableModifyLayoutTreeStructureAsserts::
             canModifyLayoutTreeStateInAnyState() ||
         document.lifecycle().stateAllowsLayoutTreeMutations();
}

DeprecatedDisableModifyLayoutTreeStructureAsserts::
    DeprecatedDisableModifyLayoutTreeStructureAsserts()
    : m_disabler(&gModifyLayoutTreeStructureAnyState, true) {}

bool DeprecatedDisableModifyLayoutTreeStructureAsserts::
    canModifyLayoutTreeStateInAnyState() {
  return gModifyLayoutTreeStructureAnyState;
}

void LayoutObject::
    setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants() {
  // Clear first because PaintInvalidationSubtree overrides other full paint
  // invalidation reasons.
  clearShouldDoFullPaintInvalidation();
  setShouldDoFullPaintInvalidation(PaintInvalidationSubtree);
}

void LayoutObject::setIsBackgroundAttachmentFixedObject(
    bool isBackgroundAttachmentFixedObject) {
  ASSERT(frameView());
  if (m_bitfields.isBackgroundAttachmentFixedObject() ==
      isBackgroundAttachmentFixedObject)
    return;
  m_bitfields.setIsBackgroundAttachmentFixedObject(
      isBackgroundAttachmentFixedObject);
  if (isBackgroundAttachmentFixedObject)
    frameView()->addBackgroundAttachmentFixedObject(this);
  else
    frameView()->removeBackgroundAttachmentFixedObject(this);
}

const ObjectPaintProperties* LayoutObject::paintProperties() const {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  return m_paintProperties.get();
}

ObjectPaintProperties& LayoutObject::ensurePaintProperties() {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  if (!m_paintProperties)
    m_paintProperties = ObjectPaintProperties::create();
  return *m_paintProperties;
}

LayoutRect LayoutObject::debugRect() const {
  LayoutRect rect;
  LayoutBlock* block = containingBlock();
  if (block)
    block->adjustChildDebugRect(rect);

  return rect;
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::LayoutObject* object) {
  if (object)
    object->showTreeForThis();
  else
    WTFLogAlways("%s", "Cannot showTree. Root is (nil)");
}

void showLineTree(const blink::LayoutObject* object) {
  if (object)
    object->showLineTreeForThis();
  else
    WTFLogAlways("%s", "Cannot showLineTree. Root is (nil)");
}

void showLayoutTree(const blink::LayoutObject* object1) {
  showLayoutTree(object1, 0);
}

void showLayoutTree(const blink::LayoutObject* object1,
                    const blink::LayoutObject* object2) {
  if (object1) {
    const blink::LayoutObject* root = object1;
    while (root->parent())
      root = root->parent();
    root->showLayoutTreeAndMark(object1, "*", object2, "-", 0);
  } else {
    WTFLogAlways("%s", "Cannot showLayoutTree. Root is (nil)");
  }
}

#endif
