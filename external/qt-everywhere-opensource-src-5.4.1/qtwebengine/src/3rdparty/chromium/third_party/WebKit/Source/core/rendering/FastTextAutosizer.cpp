/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/FastTextAutosizer.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/page/Page.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderListMarker.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderView.h"

#ifdef AUTOSIZING_DOM_DEBUG_INFO
#include "core/dom/ExecutionContextTask.h"
#endif

using namespace std;

namespace WebCore {

#ifdef AUTOSIZING_DOM_DEBUG_INFO
class WriteDebugInfoTask : public ExecutionContextTask {
public:
    WriteDebugInfoTask(PassRefPtrWillBeRawPtr<Element> element, AtomicString value)
        : m_element(element)
        , m_value(value)
    {
    }

    virtual void performTask(ExecutionContext*)
    {
        m_element->setAttribute("data-autosizing", m_value, ASSERT_NO_EXCEPTION);
    }

private:
    RefPtrWillBePersistent<Element> m_element;
    AtomicString m_value;
};

static void writeDebugInfo(RenderObject* renderer, const AtomicString& output)
{
    Node* node = renderer->node();
    if (!node)
        return;
    if (node->isDocumentNode())
        node = toDocument(node)->documentElement();
    if (!node->isElementNode())
        return;
    node->document().postTask(adoptPtr(new WriteDebugInfoTask(toElement(node), output)));
}

void FastTextAutosizer::writeClusterDebugInfo(Cluster* cluster)
{
    String explanation = "";
    if (cluster->m_flags & SUPPRESSING) {
        explanation = "[suppressed]";
    } else if (!(cluster->m_flags & (INDEPENDENT | WIDER_OR_NARROWER))) {
        explanation = "[inherited]";
    } else if (cluster->m_supercluster) {
        explanation = "[supercluster]";
    } else if (!clusterHasEnoughTextToAutosize(cluster)) {
        explanation = "[insufficient-text]";
    } else {
        const RenderBlock* widthProvider = clusterWidthProvider(cluster->m_root);
        if (cluster->m_hasTableAncestor && cluster->m_multiplier < multiplierFromBlock(widthProvider)) {
            explanation = "[table-ancestor-limited]";
        } else {
            explanation = String::format("[from width %d of %s]",
                static_cast<int>(widthFromBlock(widthProvider)), widthProvider->debugName().utf8().data());
        }
    }
    String pageInfo = "";
    if (cluster->m_root->isRenderView()) {
        pageInfo = String::format("; pageinfo: bm %f * (lw %d / fw %d)",
            m_pageInfo.m_baseMultiplier, m_pageInfo.m_layoutWidth, m_pageInfo.m_frameWidth);
    }
    float multiplier = cluster->m_flags & SUPPRESSING ? 1.0 : cluster->m_multiplier;
    writeDebugInfo(const_cast<RenderBlock*>(cluster->m_root),
        AtomicString(String::format("cluster: %f %s%s", multiplier,
            explanation.utf8().data(), pageInfo.utf8().data())));
}
#endif

static const RenderObject* parentElementRenderer(const RenderObject* renderer)
{
    // At style recalc, the renderer's parent may not be attached,
    // so we need to obtain this from the DOM tree.
    const Node* node = renderer->node();
    if (!node)
        return 0;

    while ((node = node->parentNode())) {
        if (node->isElementNode())
            return node->renderer();
    }
    return 0;
}

static bool isNonTextAreaFormControl(const RenderObject* renderer)
{
    const Node* node = renderer ? renderer->node() : 0;
    if (!node || !node->isElementNode())
        return false;
    const Element* element = toElement(node);

    return (element->isFormControlElement() && !isHTMLTextAreaElement(element));
}

static bool isPotentialClusterRoot(const RenderObject* renderer)
{
    // "Potential cluster roots" are the smallest unit for which we can
    // enable/disable text autosizing.
    // - Must not be inline, as different multipliers on one line looks terrible.
    //   Exceptions are inline-block and alike elements (inline-table, -webkit-inline-*),
    //   as they often contain entire multi-line columns of text.
    // - Must not be normal list items, as items in the same list should look
    //   consistent, unless they are floating or position:absolute/fixed.
    Node* node = renderer->generatingNode();
    if (node && !node->hasChildren())
        return false;
    if (!renderer->isRenderBlock())
        return false;
    if (renderer->isInline() && !renderer->style()->isDisplayReplacedType())
        return false;
    if (renderer->isListItem())
        return (renderer->isFloating() || renderer->isOutOfFlowPositioned());

    return true;
}

static bool isIndependentDescendant(const RenderBlock* renderer)
{
    ASSERT(isPotentialClusterRoot(renderer));

    RenderBlock* containingBlock = renderer->containingBlock();
    return renderer->isRenderView()
        || renderer->isFloating()
        || renderer->isOutOfFlowPositioned()
        || renderer->isTableCell()
        || renderer->isTableCaption()
        || renderer->isFlexibleBoxIncludingDeprecated()
        || renderer->hasColumns()
        || (containingBlock && containingBlock->isHorizontalWritingMode() != renderer->isHorizontalWritingMode())
        || renderer->style()->isDisplayReplacedType()
        || renderer->isTextArea()
        || renderer->style()->userModify() != READ_ONLY;
}

static bool blockIsRowOfLinks(const RenderBlock* block)
{
    // A "row of links" is a block for which:
    //  1. It does not contain non-link text elements longer than 3 characters
    //  2. It contains a minimum of 3 inline links and all links should
    //     have the same specified font size.
    //  3. It should not contain <br> elements.
    //  4. It should contain only inline elements unless they are containers,
    //     children of link elements or children of sub-containers.
    int linkCount = 0;
    RenderObject* renderer = block->firstChild();
    float matchingFontSize = -1;

    while (renderer) {
        if (!isPotentialClusterRoot(renderer)) {
            if (renderer->isText() && toRenderText(renderer)->text().impl()->stripWhiteSpace()->length() > 3)
                return false;
            if (!renderer->isInline() || renderer->isBR())
                return false;
        }
        if (renderer->style()->isLink()) {
            linkCount++;
            if (matchingFontSize < 0)
                matchingFontSize = renderer->style()->specifiedFontSize();
            else if (matchingFontSize != renderer->style()->specifiedFontSize())
                return false;

            // Skip traversing descendants of the link.
            renderer = renderer->nextInPreOrderAfterChildren(block);
            continue;
        }
        renderer = renderer->nextInPreOrder(block);
    }

    return (linkCount >= 3);
}

static bool blockHeightConstrained(const RenderBlock* block)
{
    // FIXME: Propagate constrainedness down the tree, to avoid inefficiently walking back up from each box.
    // FIXME: This code needs to take into account vertical writing modes.
    // FIXME: Consider additional heuristics, such as ignoring fixed heights if the content is already overflowing before autosizing kicks in.
    for (; block; block = block->containingBlock()) {
        RenderStyle* style = block->style();
        if (style->overflowY() >= OSCROLL)
            return false;
        if (style->height().isSpecified() || style->maxHeight().isSpecified() || block->isOutOfFlowPositioned()) {
            // Some sites (e.g. wikipedia) set their html and/or body elements to height:100%,
            // without intending to constrain the height of the content within them.
            return !block->isDocumentElement() && !block->isBody();
        }
        if (block->isFloating())
            return false;
    }
    return false;
}

static bool blockOrImmediateChildrenAreFormControls(const RenderBlock* block)
{
    if (isNonTextAreaFormControl(block))
        return true;
    const RenderObject* renderer = block->firstChild();
    while (renderer) {
        if (isNonTextAreaFormControl(renderer))
            return true;
        renderer = renderer->nextSibling();
    }

    return false;
}

// Some blocks are not autosized even if their parent cluster wants them to.
static bool blockSuppressesAutosizing(const RenderBlock* block)
{
    if (blockOrImmediateChildrenAreFormControls(block))
        return true;

    if (blockIsRowOfLinks(block))
        return true;

    // Don't autosize block-level text that can't wrap (as it's likely to
    // expand sideways and break the page's layout).
    if (!block->style()->autoWrap())
        return true;

    if (blockHeightConstrained(block))
        return true;

    return false;
}

static bool hasExplicitWidth(const RenderBlock* block)
{
    // FIXME: This heuristic may need to be expanded to other ways a block can be wider or narrower
    //        than its parent containing block.
    return block->style() && block->style()->width().isSpecified();
}

FastTextAutosizer::FastTextAutosizer(const Document* document)
    : m_document(document)
    , m_firstBlockToBeginLayout(0)
#ifndef NDEBUG
    , m_blocksThatHaveBegunLayout()
#endif
    , m_superclusters()
    , m_clusterStack()
    , m_fingerprintMapper()
    , m_pageInfo()
    , m_updatePageInfoDeferred(false)
{
}

void FastTextAutosizer::record(const RenderBlock* block)
{
    if (!m_pageInfo.m_settingEnabled)
        return;

    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    if (!classifyBlock(block, INDEPENDENT | EXPLICIT_WIDTH))
        return;

    if (Fingerprint fingerprint = computeFingerprint(block))
        m_fingerprintMapper.addTentativeClusterRoot(block, fingerprint);
}

void FastTextAutosizer::destroy(const RenderBlock* block)
{
    if (!m_pageInfo.m_settingEnabled)
        return;

    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    if (m_fingerprintMapper.remove(block) && m_firstBlockToBeginLayout) {
        // RenderBlock with a fingerprint was destroyed during layout.
        // Clear the cluster stack and the supercluster map to avoid stale pointers.
        // Speculative fix for http://crbug.com/369485.
        m_firstBlockToBeginLayout = 0;
        m_clusterStack.clear();
        m_superclusters.clear();
    }
}

FastTextAutosizer::BeginLayoutBehavior FastTextAutosizer::prepareForLayout(const RenderBlock* block)
{
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(block);
#endif

    if (!m_firstBlockToBeginLayout) {
        m_firstBlockToBeginLayout = block;
        prepareClusterStack(block->parent());
    } else if (block == currentCluster()->m_root) {
        // Ignore beginLayout on the same block twice.
        // This can happen with paginated overflow.
        return StopLayout;
    }

    return ContinueLayout;
}

void FastTextAutosizer::prepareClusterStack(const RenderObject* renderer)
{
    if (!renderer)
        return;
    prepareClusterStack(renderer->parent());

    if (renderer->isRenderBlock()) {
        const RenderBlock* block = toRenderBlock(renderer);
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.add(block);
#endif
        if (Cluster* cluster = maybeCreateCluster(block))
            m_clusterStack.append(adoptPtr(cluster));
    }
}

void FastTextAutosizer::beginLayout(RenderBlock* block)
{
    ASSERT(shouldHandleLayout());

    if (prepareForLayout(block) == StopLayout)
        return;

    if (Cluster* cluster = maybeCreateCluster(block))
        m_clusterStack.append(adoptPtr(cluster));

    // Cells in auto-layout tables are handled separately by inflateAutoTable.
    bool isAutoTableCell = block->isTableCell() && !toRenderTableCell(block)->table()->style()->isFixedTableLayout();
    if (!isAutoTableCell && !m_clusterStack.isEmpty())
        inflate(block);
}

void FastTextAutosizer::inflateListItem(RenderListItem* listItem, RenderListMarker* listItemMarker)
{
    if (!shouldHandleLayout())
        return;
    ASSERT(listItem && listItemMarker);

    if (prepareForLayout(listItem) == StopLayout)
        return;

    // Force the LI to be inside the DBCAT when computing the multiplier.
    // This guarantees that the DBCAT has entered layout, so we can ask for its width.
    // It also makes sense because the list marker is autosized like a text node.
    float multiplier = clusterMultiplier(currentCluster());

    applyMultiplier(listItem, multiplier);
    applyMultiplier(listItemMarker, multiplier);
}

void FastTextAutosizer::inflateAutoTable(RenderTable* table)
{
    ASSERT(table);
    ASSERT(!table->style()->isFixedTableLayout());
    ASSERT(table->containingBlock());

    Cluster* cluster = currentCluster();
    if (cluster->m_root != table)
        return;

    // Pre-inflate cells that have enough text so that their inflated preferred widths will be used
    // for column sizing.
    for (RenderObject* section = table->firstChild(); section; section = section->nextSibling()) {
        if (!section->isTableSection())
            continue;
        for (RenderTableRow* row = toRenderTableSection(section)->firstRow(); row; row = row->nextRow()) {
            for (RenderTableCell* cell = row->firstCell(); cell; cell = cell->nextCell()) {
                if (!cell->needsLayout())
                    continue;

                beginLayout(cell);
                inflate(cell, DescendToInnerBlocks);
                endLayout(cell);
            }
        }
    }
}

void FastTextAutosizer::endLayout(RenderBlock* block)
{
    ASSERT(shouldHandleLayout());

    if (block == m_firstBlockToBeginLayout) {
        m_firstBlockToBeginLayout = 0;
        m_clusterStack.clear();
        m_superclusters.clear();
        m_stylesRetainedDuringLayout.clear();
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.clear();
#endif
    // Tables can create two layout scopes for the same block so the isEmpty
    // check below is needed to guard against endLayout being called twice.
    } else if (!m_clusterStack.isEmpty() && currentCluster()->m_root == block) {
        m_clusterStack.removeLast();
    }
}

float FastTextAutosizer::inflate(RenderObject* parent, InflateBehavior behavior, float multiplier)
{
    Cluster* cluster = currentCluster();
    bool hasTextChild = false;

    RenderObject* child = 0;
    if (parent->isRenderBlock() && (parent->childrenInline() || behavior == DescendToInnerBlocks))
        child = toRenderBlock(parent)->firstChild();
    else if (parent->isRenderInline())
        child = toRenderInline(parent)->firstChild();

    while (child) {
        if (child->isText()) {
            hasTextChild = true;
            // We only calculate this multiplier on-demand to ensure the parent block of this text
            // has entered layout.
            if (!multiplier)
                multiplier = cluster->m_flags & SUPPRESSING ? 1.0f : clusterMultiplier(cluster);
            applyMultiplier(child, multiplier);
            // FIXME: Investigate why MarkOnlyThis is sufficient.
            if (parent->isRenderInline())
                child->setPreferredLogicalWidthsDirty(MarkOnlyThis);
        } else if (child->isRenderInline()) {
            multiplier = inflate(child, behavior, multiplier);
        } else if (child->isRenderBlock() && behavior == DescendToInnerBlocks
            && !classifyBlock(child, INDEPENDENT | EXPLICIT_WIDTH | SUPPRESSING)) {
            multiplier = inflate(child, behavior, multiplier);
        }
        child = child->nextSibling();
    }

    if (hasTextChild) {
        applyMultiplier(parent, multiplier); // Parent handles line spacing.
    } else if (!parent->isListItem()) {
        // For consistency, a block with no immediate text child should always have a
        // multiplier of 1 (except for list items which are handled in inflateListItem).
        applyMultiplier(parent, 1);
    }
    return multiplier;
}

bool FastTextAutosizer::shouldHandleLayout() const
{
    return m_pageInfo.m_settingEnabled && m_pageInfo.m_pageNeedsAutosizing && !m_updatePageInfoDeferred;
}

void FastTextAutosizer::updatePageInfoInAllFrames()
{
    ASSERT(!m_document->frame() || m_document->frame()->isMainFrame());

    for (Frame* frame = m_document->frame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        if (FastTextAutosizer* textAutosizer = toLocalFrame(frame)->document()->fastTextAutosizer())
            textAutosizer->updatePageInfo();
    }
}

void FastTextAutosizer::updatePageInfo()
{
    if (m_updatePageInfoDeferred || !m_document->page() || !m_document->settings())
        return;

    PageInfo previousPageInfo(m_pageInfo);
    m_pageInfo.m_settingEnabled = m_document->settings()->textAutosizingEnabled();

    if (!m_pageInfo.m_settingEnabled || m_document->printing()) {
        m_pageInfo.m_pageNeedsAutosizing = false;
    } else {
        RenderView* renderView = m_document->renderView();
        bool horizontalWritingMode = isHorizontalWritingMode(renderView->style()->writingMode());

        LocalFrame* mainFrame = m_document->page()->deprecatedLocalMainFrame();
        IntSize frameSize = m_document->settings()->textAutosizingWindowSizeOverride();
        if (frameSize.isEmpty())
            frameSize = mainFrame->view()->unscaledVisibleContentSize(IncludeScrollbars);
        m_pageInfo.m_frameWidth = horizontalWritingMode ? frameSize.width() : frameSize.height();

        IntSize layoutSize = mainFrame->view()->layoutSize();
        m_pageInfo.m_layoutWidth = horizontalWritingMode ? layoutSize.width() : layoutSize.height();

        // Compute the base font scale multiplier based on device and accessibility settings.
        m_pageInfo.m_baseMultiplier = m_document->settings()->accessibilityFontScaleFactor();
        // If the page has a meta viewport or @viewport, don't apply the device scale adjustment.
        const ViewportDescription& viewportDescription = mainFrame->document()->viewportDescription();
        if (!viewportDescription.isSpecifiedByAuthor()) {
            float deviceScaleAdjustment = m_document->settings()->deviceScaleAdjustment();
            m_pageInfo.m_baseMultiplier *= deviceScaleAdjustment;
        }

        m_pageInfo.m_pageNeedsAutosizing = !!m_pageInfo.m_frameWidth
            && (m_pageInfo.m_baseMultiplier * (static_cast<float>(m_pageInfo.m_layoutWidth) / m_pageInfo.m_frameWidth) > 1.0f);
    }

    if (m_pageInfo.m_pageNeedsAutosizing) {
        // If page info has changed, multipliers may have changed. Force a layout to recompute them.
        if (m_pageInfo.m_frameWidth != previousPageInfo.m_frameWidth
            || m_pageInfo.m_layoutWidth != previousPageInfo.m_layoutWidth
            || m_pageInfo.m_baseMultiplier != previousPageInfo.m_baseMultiplier
            || m_pageInfo.m_settingEnabled != previousPageInfo.m_settingEnabled)
            setAllTextNeedsLayout();
    } else if (previousPageInfo.m_hasAutosized) {
        // If we are no longer autosizing the page, we won't do anything during the next layout.
        // Set all the multipliers back to 1 now.
        resetMultipliers();
        m_pageInfo.m_hasAutosized = false;
    }
}

void FastTextAutosizer::resetMultipliers()
{
    RenderObject* renderer = m_document->renderView();
    while (renderer) {
        if (RenderStyle* style = renderer->style()) {
            if (style->textAutosizingMultiplier() != 1)
                applyMultiplier(renderer, 1, LayoutNeeded);
        }
        renderer = renderer->nextInPreOrder();
    }
}

void FastTextAutosizer::setAllTextNeedsLayout()
{
    RenderObject* renderer = m_document->renderView();
    while (renderer) {
        if (renderer->isText())
            renderer->setNeedsLayoutAndFullPaintInvalidation();
        renderer = renderer->nextInPreOrder();
    }
}

FastTextAutosizer::BlockFlags FastTextAutosizer::classifyBlock(const RenderObject* renderer, BlockFlags mask)
{
    if (!renderer->isRenderBlock())
        return 0;

    const RenderBlock* block = toRenderBlock(renderer);
    BlockFlags flags = 0;

    if (isPotentialClusterRoot(block)) {
        if (mask & POTENTIAL_ROOT)
            flags |= POTENTIAL_ROOT;

        if ((mask & INDEPENDENT) && (isIndependentDescendant(block) || block->isTable()))
            flags |= INDEPENDENT;

        if ((mask & EXPLICIT_WIDTH) && hasExplicitWidth(block))
            flags |= EXPLICIT_WIDTH;

        if ((mask & SUPPRESSING) && blockSuppressesAutosizing(block))
            flags |= SUPPRESSING;
    }
    return flags;
}

bool FastTextAutosizer::clusterWouldHaveEnoughTextToAutosize(const RenderBlock* root, const RenderBlock* widthProvider)
{
    Cluster hypotheticalCluster(root, classifyBlock(root), 0);
    return clusterHasEnoughTextToAutosize(&hypotheticalCluster, widthProvider);
}

bool FastTextAutosizer::clusterHasEnoughTextToAutosize(Cluster* cluster, const RenderBlock* widthProvider)
{
    if (cluster->m_hasEnoughTextToAutosize != UnknownAmountOfText)
        return cluster->m_hasEnoughTextToAutosize == HasEnoughText;

    const RenderBlock* root = cluster->m_root;
    if (!widthProvider)
        widthProvider = clusterWidthProvider(root);

    // TextAreas and user-modifiable areas get a free pass to autosize regardless of text content.
    if (root->isTextArea() || (root->style() && root->style()->userModify() != READ_ONLY)) {
        cluster->m_hasEnoughTextToAutosize = HasEnoughText;
        return true;
    }

    if (cluster->m_flags & SUPPRESSING) {
        cluster->m_hasEnoughTextToAutosize = NotEnoughText;
        return false;
    }

    // 4 lines of text is considered enough to autosize.
    float minimumTextLengthToAutosize = widthFromBlock(widthProvider) * 4;

    float length = 0;
    RenderObject* descendant = root->firstChild();
    while (descendant) {
        if (descendant->isRenderBlock()) {
            if (classifyBlock(descendant, INDEPENDENT | SUPPRESSING)) {
                descendant = descendant->nextInPreOrderAfterChildren(root);
                continue;
            }
        } else if (descendant->isText()) {
            // Note: Using text().stripWhiteSpace().length() instead of renderedTextLength() because
            // the lineboxes will not be built until layout. These values can be different.
            // Note: This is an approximation assuming each character is 1em wide.
            length += toRenderText(descendant)->text().stripWhiteSpace().length() * descendant->style()->specifiedFontSize();

            if (length >= minimumTextLengthToAutosize) {
                cluster->m_hasEnoughTextToAutosize = HasEnoughText;
                return true;
            }
        }
        descendant = descendant->nextInPreOrder(root);
    }

    cluster->m_hasEnoughTextToAutosize = NotEnoughText;
    return false;
}

FastTextAutosizer::Fingerprint FastTextAutosizer::getFingerprint(const RenderObject* renderer)
{
    Fingerprint result = m_fingerprintMapper.get(renderer);
    if (!result) {
        result = computeFingerprint(renderer);
        m_fingerprintMapper.add(renderer, result);
    }
    return result;
}

FastTextAutosizer::Fingerprint FastTextAutosizer::computeFingerprint(const RenderObject* renderer)
{
    Node* node = renderer->generatingNode();
    if (!node || !node->isElementNode())
        return 0;

    FingerprintSourceData data;
    if (const RenderObject* parent = parentElementRenderer(renderer))
        data.m_parentHash = getFingerprint(parent);

    data.m_qualifiedNameHash = QualifiedNameHash::hash(toElement(node)->tagQName());

    if (RenderStyle* style = renderer->style()) {
        data.m_packedStyleProperties = style->direction();
        data.m_packedStyleProperties |= (style->position() << 1);
        data.m_packedStyleProperties |= (style->floating() << 4);
        data.m_packedStyleProperties |= (style->display() << 6);
        data.m_packedStyleProperties |= (style->width().type() << 11);
        // packedStyleProperties effectively using 15 bits now.

        // consider for adding: writing mode, padding.

        data.m_width = style->width().getFloatValue();
    }

    // Use nodeIndex as a rough approximation of column number
    // (it's too early to call RenderTableCell::col).
    // FIXME: account for colspan
    if (renderer->isTableCell())
        data.m_column = renderer->node()->nodeIndex();

    return StringHasher::computeHash<UChar>(
        static_cast<const UChar*>(static_cast<const void*>(&data)),
        sizeof data / sizeof(UChar));
}

FastTextAutosizer::Cluster* FastTextAutosizer::maybeCreateCluster(const RenderBlock* block)
{
    BlockFlags flags = classifyBlock(block);
    if (!(flags & POTENTIAL_ROOT))
        return 0;

    Cluster* parentCluster = m_clusterStack.isEmpty() ? 0 : currentCluster();
    ASSERT(parentCluster || block->isRenderView());

    // If a non-independent block would not alter the SUPPRESSING flag, it doesn't need to be a cluster.
    bool parentSuppresses = parentCluster && (parentCluster->m_flags & SUPPRESSING);
    if (!(flags & INDEPENDENT) && !(flags & EXPLICIT_WIDTH) && !!(flags & SUPPRESSING) == parentSuppresses)
        return 0;

    Cluster* cluster = new Cluster(block, flags, parentCluster, getSupercluster(block));
#ifdef AUTOSIZING_DOM_DEBUG_INFO
    // Non-SUPPRESSING clusters are annotated in clusterMultiplier.
    if (flags & SUPPRESSING)
        writeClusterDebugInfo(cluster);
#endif
    return cluster;
}

FastTextAutosizer::Supercluster* FastTextAutosizer::getSupercluster(const RenderBlock* block)
{
    Fingerprint fingerprint = m_fingerprintMapper.get(block);
    if (!fingerprint)
        return 0;

    BlockSet* roots = &m_fingerprintMapper.getTentativeClusterRoots(fingerprint);
    if (!roots || roots->size() < 2 || !roots->contains(block))
        return 0;

    SuperclusterMap::AddResult addResult = m_superclusters.add(fingerprint, PassOwnPtr<Supercluster>());
    if (!addResult.isNewEntry)
        return addResult.storedValue->value.get();

    Supercluster* supercluster = new Supercluster(roots);
    addResult.storedValue->value = adoptPtr(supercluster);
    return supercluster;
}

float FastTextAutosizer::clusterMultiplier(Cluster* cluster)
{
    if (cluster->m_multiplier)
        return cluster->m_multiplier;

    // FIXME: why does isWiderOrNarrowerDescendant crash on independent clusters?
    if (!(cluster->m_flags & INDEPENDENT) && isWiderOrNarrowerDescendant(cluster))
        cluster->m_flags |= WIDER_OR_NARROWER;

    if (cluster->m_flags & (INDEPENDENT | WIDER_OR_NARROWER)) {
        if (cluster->m_supercluster)
            cluster->m_multiplier = superclusterMultiplier(cluster);
        else if (clusterHasEnoughTextToAutosize(cluster))
            cluster->m_multiplier = multiplierFromBlock(clusterWidthProvider(cluster->m_root));
        else
            cluster->m_multiplier = 1.0f;
    } else {
        cluster->m_multiplier = cluster->m_parent ? clusterMultiplier(cluster->m_parent) : 1.0f;
    }

#ifdef AUTOSIZING_DOM_DEBUG_INFO
    writeClusterDebugInfo(cluster);
#endif

    ASSERT(cluster->m_multiplier);
    return cluster->m_multiplier;
}

bool FastTextAutosizer::superclusterHasEnoughTextToAutosize(Supercluster* supercluster, const RenderBlock* widthProvider)
{
    if (supercluster->m_hasEnoughTextToAutosize != UnknownAmountOfText)
        return supercluster->m_hasEnoughTextToAutosize == HasEnoughText;

    BlockSet::iterator end = supercluster->m_roots->end();
    for (BlockSet::iterator it = supercluster->m_roots->begin(); it != end; ++it) {
        if (clusterWouldHaveEnoughTextToAutosize(*it, widthProvider)) {
            supercluster->m_hasEnoughTextToAutosize = HasEnoughText;
            return true;
        }
    }
    supercluster->m_hasEnoughTextToAutosize = NotEnoughText;
    return false;
}

float FastTextAutosizer::superclusterMultiplier(Cluster* cluster)
{
    Supercluster* supercluster = cluster->m_supercluster;
    if (!supercluster->m_multiplier) {
        const RenderBlock* widthProvider = maxClusterWidthProvider(cluster->m_supercluster, cluster->m_root);
        supercluster->m_multiplier = superclusterHasEnoughTextToAutosize(supercluster, widthProvider)
            ? multiplierFromBlock(widthProvider) : 1.0f;
    }
    ASSERT(supercluster->m_multiplier);
    return supercluster->m_multiplier;
}

const RenderBlock* FastTextAutosizer::clusterWidthProvider(const RenderBlock* root)
{
    if (root->isTable() || root->isTableCell())
        return root;

    return deepestBlockContainingAllText(root);
}

const RenderBlock* FastTextAutosizer::maxClusterWidthProvider(const Supercluster* supercluster, const RenderBlock* currentRoot)
{
    const RenderBlock* result = clusterWidthProvider(currentRoot);
    float maxWidth = widthFromBlock(result);

    const BlockSet* roots = supercluster->m_roots;
    for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it) {
        const RenderBlock* widthProvider = clusterWidthProvider(*it);
        if (widthProvider->needsLayout())
            continue;
        float width = widthFromBlock(widthProvider);
        if (width > maxWidth) {
            maxWidth = width;
            result = widthProvider;
        }
    }
    RELEASE_ASSERT(result);
    return result;
}

float FastTextAutosizer::widthFromBlock(const RenderBlock* block)
{
    RELEASE_ASSERT(block);
    RELEASE_ASSERT(block->style());

    if (!(block->isTable() || block->isTableCell() || block->isListItem()))
        return block->contentLogicalWidth().toFloat();

    if (!block->containingBlock())
        return 0;

    // Tables may be inflated before computing their preferred widths. Try several methods to
    // obtain a width, and fall back on a containing block's width.
    do {
        float width;
        Length specifiedWidth = block->isTableCell()
            ? toRenderTableCell(block)->styleOrColLogicalWidth() : block->style()->logicalWidth();
        if (specifiedWidth.isFixed()) {
            if ((width = specifiedWidth.value()) > 0)
                return width;
        }
        if (specifiedWidth.isPercent()) {
            if (float containerWidth = block->containingBlock()->contentLogicalWidth().toFloat()) {
                if ((width = floatValueForLength(specifiedWidth, containerWidth)) > 0)
                    return width;
            }
        }
        if ((width = block->contentLogicalWidth().toFloat()) > 0)
            return width;
    } while ((block = block->containingBlock()));
    return 0;
}

float FastTextAutosizer::multiplierFromBlock(const RenderBlock* block)
{
    // If block->needsLayout() is false, it does not need to be in m_blocksThatHaveBegunLayout.
    // This can happen during layout of a positioned object if the cluster's DBCAT is deeper
    // than the positioned object's containing block, and wasn't marked as needing layout.
    ASSERT(m_blocksThatHaveBegunLayout.contains(block) || !block->needsLayout());

    // Block width, in CSS pixels.
    float blockWidth = widthFromBlock(block);
    float multiplier = m_pageInfo.m_frameWidth ? min(blockWidth, static_cast<float>(m_pageInfo.m_layoutWidth)) / m_pageInfo.m_frameWidth : 1.0f;

    return max(m_pageInfo.m_baseMultiplier * multiplier, 1.0f);
}

const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(Cluster* cluster)
{
    if (!cluster->m_deepestBlockContainingAllText)
        cluster->m_deepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_root);

    return cluster->m_deepestBlockContainingAllText;
}

// FIXME: Refactor this to look more like FastTextAutosizer::deepestCommonAncestor. This is copied
//        from TextAutosizer::findDeepestBlockContainingAllText.
const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(const RenderBlock* root)
{
    size_t firstDepth = 0;
    const RenderObject* firstTextLeaf = findTextLeaf(root, firstDepth, First);
    if (!firstTextLeaf)
        return root;

    size_t lastDepth = 0;
    const RenderObject* lastTextLeaf = findTextLeaf(root, lastDepth, Last);
    ASSERT(lastTextLeaf);

    // Equalize the depths if necessary. Only one of the while loops below will get executed.
    const RenderObject* firstNode = firstTextLeaf;
    const RenderObject* lastNode = lastTextLeaf;
    while (firstDepth > lastDepth) {
        firstNode = firstNode->parent();
        --firstDepth;
    }
    while (lastDepth > firstDepth) {
        lastNode = lastNode->parent();
        --lastDepth;
    }

    // Go up from both nodes until the parent is the same. Both pointers will point to the LCA then.
    while (firstNode != lastNode) {
        firstNode = firstNode->parent();
        lastNode = lastNode->parent();
    }

    if (firstNode->isRenderBlock())
        return toRenderBlock(firstNode);

    // containingBlock() should never leave the cluster, since it only skips ancestors when finding
    // the container of position:absolute/fixed blocks, and those cannot exist between a cluster and
    // its text node's lowest common ancestor as isAutosizingCluster would have made them into their
    // own independent cluster.
    const RenderBlock* containingBlock = firstNode->containingBlock();
    if (!containingBlock)
        return root;

    ASSERT(containingBlock->isDescendantOf(root));
    return containingBlock;
}

const RenderObject* FastTextAutosizer::findTextLeaf(const RenderObject* parent, size_t& depth, TextLeafSearch firstOrLast)
{
    // List items are treated as text due to the marker.
    // The actual renderer for the marker (RenderListMarker) may not be in the tree yet since it is added during layout.
    if (parent->isListItem())
        return parent;

    if (parent->isText())
        return parent;

    ++depth;
    const RenderObject* child = (firstOrLast == First) ? parent->slowFirstChild() : parent->slowLastChild();
    while (child) {
        // Note: At this point clusters may not have been created for these blocks so we cannot rely
        //       on m_clusters. Instead, we use a best-guess about whether the block will become a cluster.
        if (!classifyBlock(child, INDEPENDENT)) {
            if (const RenderObject* leaf = findTextLeaf(child, depth, firstOrLast))
                return leaf;
        }
        child = (firstOrLast == First) ? child->nextSibling() : child->previousSibling();
    }
    --depth;

    return 0;
}

void FastTextAutosizer::applyMultiplier(RenderObject* renderer, float multiplier, RelayoutBehavior relayoutBehavior)
{
    ASSERT(renderer && renderer->style());
    RenderStyle* currentStyle = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();

    switch (relayoutBehavior) {
    case AlreadyInLayout:
        // Don't free currentStyle until the end of the layout pass. This allows other parts of the system
        // to safely hold raw RenderStyle* pointers during layout, e.g. BreakingContext::m_currentStyle.
        m_stylesRetainedDuringLayout.append(currentStyle);

        renderer->setStyleInternal(style.release());
        renderer->setNeedsLayoutAndFullPaintInvalidation();
        break;

    case LayoutNeeded:
        renderer->setStyle(style.release());
        break;
    }

    if (multiplier != 1)
        m_pageInfo.m_hasAutosized = true;
}

bool FastTextAutosizer::isWiderOrNarrowerDescendant(Cluster* cluster)
{
    // FIXME: Why do we return true when hasExplicitWidth returns false??
    if (!cluster->m_parent || !hasExplicitWidth(cluster->m_root))
        return true;

    const RenderBlock* parentDeepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_parent);
    ASSERT(m_blocksThatHaveBegunLayout.contains(cluster->m_root));
    ASSERT(m_blocksThatHaveBegunLayout.contains(parentDeepestBlockContainingAllText));

    float contentWidth = cluster->m_root->contentLogicalWidth().toFloat();
    float clusterTextWidth = parentDeepestBlockContainingAllText->contentLogicalWidth().toFloat();

    // Clusters with a root that is wider than the deepestBlockContainingAllText of their parent
    // autosize independently of their parent.
    if (contentWidth > clusterTextWidth)
        return true;

    // Clusters with a root that is significantly narrower than the deepestBlockContainingAllText of
    // their parent autosize independently of their parent.
    static float narrowWidthDifference = 200;
    if (clusterTextWidth - contentWidth > narrowWidthDifference)
        return true;

    return false;
}

FastTextAutosizer::Cluster* FastTextAutosizer::currentCluster() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_clusterStack.isEmpty());
    return m_clusterStack.last().get();
}

#ifndef NDEBUG
void FastTextAutosizer::FingerprintMapper::assertMapsAreConsistent()
{
    // For each fingerprint -> block mapping in m_blocksForFingerprint we should have an associated
    // map from block -> fingerprint in m_fingerprints.
    ReverseFingerprintMap::iterator end = m_blocksForFingerprint.end();
    for (ReverseFingerprintMap::iterator fingerprintIt = m_blocksForFingerprint.begin(); fingerprintIt != end; ++fingerprintIt) {
        Fingerprint fingerprint = fingerprintIt->key;
        BlockSet* blocks = fingerprintIt->value.get();
        for (BlockSet::iterator blockIt = blocks->begin(); blockIt != blocks->end(); ++blockIt) {
            const RenderBlock* block = (*blockIt);
            ASSERT(m_fingerprints.get(block) == fingerprint);
        }
    }
}
#endif

void FastTextAutosizer::FingerprintMapper::add(const RenderObject* renderer, Fingerprint fingerprint)
{
    remove(renderer);

    m_fingerprints.set(renderer, fingerprint);
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
}

void FastTextAutosizer::FingerprintMapper::addTentativeClusterRoot(const RenderBlock* block, Fingerprint fingerprint)
{
    add(block, fingerprint);

    ReverseFingerprintMap::AddResult addResult = m_blocksForFingerprint.add(fingerprint, PassOwnPtr<BlockSet>());
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new BlockSet);
    addResult.storedValue->value->add(block);
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
}

bool FastTextAutosizer::FingerprintMapper::remove(const RenderObject* renderer)
{
    Fingerprint fingerprint = m_fingerprints.take(renderer);
    if (!fingerprint || !renderer->isRenderBlock())
        return false;

    ReverseFingerprintMap::iterator blocksIter = m_blocksForFingerprint.find(fingerprint);
    if (blocksIter == m_blocksForFingerprint.end())
        return false;

    BlockSet& blocks = *blocksIter->value;
    blocks.remove(toRenderBlock(renderer));
    if (blocks.isEmpty())
        m_blocksForFingerprint.remove(blocksIter);
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
    return true;
}

FastTextAutosizer::Fingerprint FastTextAutosizer::FingerprintMapper::get(const RenderObject* renderer)
{
    return m_fingerprints.get(renderer);
}

FastTextAutosizer::BlockSet& FastTextAutosizer::FingerprintMapper::getTentativeClusterRoots(Fingerprint fingerprint)
{
    return *m_blocksForFingerprint.get(fingerprint);
}

FastTextAutosizer::LayoutScope::LayoutScope(RenderBlock* block)
    : m_textAutosizer(block->document().fastTextAutosizer())
    , m_block(block)
{
    if (!m_textAutosizer)
        return;

    if (m_textAutosizer->shouldHandleLayout())
        m_textAutosizer->beginLayout(m_block);
    else
        m_textAutosizer = 0;
}

FastTextAutosizer::LayoutScope::~LayoutScope()
{
    if (m_textAutosizer)
        m_textAutosizer->endLayout(m_block);
}


FastTextAutosizer::TableLayoutScope::TableLayoutScope(RenderTable* table)
    : LayoutScope(table)
{
    if (m_textAutosizer) {
        ASSERT(m_textAutosizer->shouldHandleLayout());
        m_textAutosizer->inflateAutoTable(table);
    }
}

FastTextAutosizer::DeferUpdatePageInfo::DeferUpdatePageInfo(Page* page)
    : m_mainFrame(page->deprecatedLocalMainFrame())
{
    if (FastTextAutosizer* textAutosizer = m_mainFrame->document()->fastTextAutosizer()) {
        ASSERT(!textAutosizer->m_updatePageInfoDeferred);
        textAutosizer->m_updatePageInfoDeferred = true;
    }
}

FastTextAutosizer::DeferUpdatePageInfo::~DeferUpdatePageInfo()
{
    if (FastTextAutosizer* textAutosizer = m_mainFrame->document()->fastTextAutosizer()) {
        ASSERT(textAutosizer->m_updatePageInfoDeferred);
        textAutosizer->m_updatePageInfoDeferred = false;
        textAutosizer->updatePageInfoInAllFrames();
    }
}

} // namespace WebCore
