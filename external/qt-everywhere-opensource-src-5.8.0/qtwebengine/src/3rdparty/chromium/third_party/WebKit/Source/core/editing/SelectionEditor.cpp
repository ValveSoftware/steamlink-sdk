/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/SelectionEditor.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/SelectionAdjuster.h"
#include "core/frame/LocalFrame.h"

namespace blink {

SelectionEditor::SelectionEditor(FrameSelection& frameSelection)
    : m_frameSelection(frameSelection)
    , m_observingVisibleSelection(false)
{
    clearVisibleSelection();
}

SelectionEditor::~SelectionEditor()
{
}

void SelectionEditor::clearVisibleSelection()
{
    m_selection = VisibleSelection();
    m_selectionInFlatTree = VisibleSelectionInFlatTree();
    if (!shouldAlwaysUseDirectionalSelection())
        return;
    m_selection.setIsDirectional(true);
    m_selectionInFlatTree.setIsDirectional(true);
}

void SelectionEditor::dispose()
{
    resetLogicalRange();
    clearVisibleSelection();
}

const Document& SelectionEditor::document() const
{
    DCHECK(m_document);
    return *m_document;
}

LocalFrame* SelectionEditor::frame() const
{
    return m_frameSelection->frame();
}

template <>
const VisibleSelection& SelectionEditor::visibleSelection<EditingStrategy>() const
{
    DCHECK_EQ(frame()->document(), document());
    DCHECK_EQ(frame(), document().frame());
    if (m_selection.isNone())
        return m_selection;
    DCHECK_EQ(m_selection.base().document(), document());
    return m_selection;
}

template <>
const VisibleSelectionInFlatTree& SelectionEditor::visibleSelection<EditingInFlatTreeStrategy>() const
{
    DCHECK_EQ(frame()->document(), document());
    DCHECK_EQ(frame(), document().frame());
    if (m_selectionInFlatTree.isNone())
        return m_selectionInFlatTree;
    DCHECK_EQ(m_selectionInFlatTree.base().document(), document());
    return m_selectionInFlatTree;
}

void SelectionEditor::setVisibleSelection(const VisibleSelection& newSelection, FrameSelection::SetSelectionOptions options)
{
    DCHECK(newSelection.isValidFor(document())) << newSelection;
    resetLogicalRange();
    m_selection = newSelection;
    if (options & FrameSelection::DoNotAdjustInFlatTree) {
        m_selectionInFlatTree.setWithoutValidation(toPositionInFlatTree(m_selection.base()), toPositionInFlatTree(m_selection.extent()));
        return;
    }

    SelectionAdjuster::adjustSelectionInFlatTree(&m_selectionInFlatTree, m_selection);
}

void SelectionEditor::setVisibleSelection(const VisibleSelectionInFlatTree& newSelection, FrameSelection::SetSelectionOptions options)
{
    DCHECK(newSelection.isValidFor(document())) << newSelection;
    DCHECK(!(options & FrameSelection::DoNotAdjustInFlatTree));
    resetLogicalRange();
    m_selectionInFlatTree = newSelection;
    SelectionAdjuster::adjustSelectionInDOMTree(&m_selection, m_selectionInFlatTree);
}

void SelectionEditor::setWithoutValidation(const Position& base, const Position& extent)
{
    resetLogicalRange();
    if (base.isNotNull())
        DCHECK_EQ(base.document(), document());
    if (extent.isNotNull())
        DCHECK_EQ(extent.document(), document());
    m_selection.setWithoutValidation(base, extent);
    m_selectionInFlatTree.setWithoutValidation(toPositionInFlatTree(base), toPositionInFlatTree(extent));
}

void SelectionEditor::documentAttached(Document* document)
{
    DCHECK(document);
    DCHECK(!m_document) << m_document;
    m_document = document;
}

void SelectionEditor::documentDetached(const Document& document)
{
    DCHECK_EQ(m_document, &document);
    dispose();
    m_document = nullptr;
}

void SelectionEditor::resetLogicalRange()
{
    // Non-collapsed ranges are not allowed to start at the end of a line that
    // is wrapped, they start at the beginning of the next line instead
    if (!m_logicalRange)
        return;
    m_logicalRange->dispose();
    m_logicalRange = nullptr;
}

void SelectionEditor::setLogicalRange(Range* range)
{
    DCHECK_EQ(range->ownerDocument(), document());
    DCHECK(!m_logicalRange) << "A logical range should be one.";
    m_logicalRange = range;
}

Range* SelectionEditor::firstRange() const
{
    if (m_logicalRange)
        return m_logicalRange->cloneRange();
    return firstRangeOf(m_selection);
}

bool SelectionEditor::shouldAlwaysUseDirectionalSelection() const
{
    return frame()->editor().behavior().shouldConsiderSelectionAsDirectional();
}

void SelectionEditor::updateIfNeeded()
{
    DCHECK(m_selection.isValidFor(document())) << m_selection;
    DCHECK(m_selectionInFlatTree.isValidFor(document())) << m_selection;
    m_selection.updateIfNeeded();
    m_selectionInFlatTree.updateIfNeeded();
}

DEFINE_TRACE(SelectionEditor)
{
    visitor->trace(m_document);
    visitor->trace(m_frameSelection);
    visitor->trace(m_selection);
    visitor->trace(m_selectionInFlatTree);
    visitor->trace(m_logicalRange);
}

} // namespace blink
