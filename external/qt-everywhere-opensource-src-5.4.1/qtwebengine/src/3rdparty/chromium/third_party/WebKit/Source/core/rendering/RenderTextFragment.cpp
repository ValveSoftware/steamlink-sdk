/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "core/rendering/RenderTextFragment.h"

#include "core/dom/Text.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBlock.h"

namespace WebCore {

RenderTextFragment::RenderTextFragment(Node* node, StringImpl* str, int startOffset, int length)
    : RenderText(node, str ? str->substring(startOffset, length) : PassRefPtr<StringImpl>(nullptr))
    , m_start(startOffset)
    , m_end(length)
    , m_firstLetter(0)
{
}

RenderTextFragment::RenderTextFragment(Node* node, StringImpl* str)
    : RenderText(node, str)
    , m_start(0)
    , m_end(str ? str->length() : 0)
    , m_contentString(str)
    , m_firstLetter(0)
{
}

RenderTextFragment::~RenderTextFragment()
{
}

RenderText* RenderTextFragment::firstRenderTextInFirstLetter() const
{
    for (RenderObject* current = m_firstLetter; current; current = current->nextInPreOrder(m_firstLetter)) {
        if (current->isText())
            return toRenderText(current);
    }
    return 0;
}

PassRefPtr<StringImpl> RenderTextFragment::originalText() const
{
    Node* e = node();
    RefPtr<StringImpl> result = ((e && e->isTextNode()) ? toText(e)->dataImpl() : contentString());
    if (!result)
        return nullptr;
    return result->substring(start(), end());
}

void RenderTextFragment::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);

    if (RenderBlock* block = blockForAccompanyingFirstLetter()) {
        block->style()->removeCachedPseudoStyle(FIRST_LETTER);
        block->updateFirstLetter();
    }
}

void RenderTextFragment::willBeDestroyed()
{
    if (m_firstLetter)
        m_firstLetter->destroy();
    RenderText::willBeDestroyed();
}

void RenderTextFragment::setText(PassRefPtr<StringImpl> text, bool force)
{
    RenderText::setText(text, force);

    m_start = 0;
    m_end = textLength();
    if (m_firstLetter) {
        // FIXME: We should not modify the structure of the render tree during
        // layout. crbug.com/370458
        DeprecatedDisableModifyRenderTreeStructureAsserts disabler;

        ASSERT(!m_contentString);
        m_firstLetter->destroy();
        m_firstLetter = 0;
        if (Node* t = node()) {
            ASSERT(!t->renderer());
            t->setRenderer(this);
        }
    }
}

void RenderTextFragment::transformText()
{
    // Don't reset first-letter here because we are only transforming the truncated fragment.
    if (RefPtr<StringImpl> textToTransform = originalText())
        RenderText::setText(textToTransform.release(), true);
}

UChar RenderTextFragment::previousCharacter() const
{
    if (start()) {
        Node* e = node();
        StringImpl* original = ((e && e->isTextNode()) ? toText(e)->dataImpl() : contentString());
        if (original && start() <= original->length())
            return (*original)[start() - 1];
    }

    return RenderText::previousCharacter();
}

RenderBlock* RenderTextFragment::blockForAccompanyingFirstLetter() const
{
    if (!m_firstLetter)
        return 0;
    for (RenderObject* block = m_firstLetter->parent(); block; block = block->parent()) {
        if (block->style()->hasPseudoStyle(FIRST_LETTER) && block->canHaveChildren() && block->isRenderBlock())
            return toRenderBlock(block);
    }
    return 0;
}

void RenderTextFragment::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    RenderObject::updateHitTestResult(result, point);
    if (m_firstLetter || !node())
        return;
    RenderObject* nodeRenderer = node()->renderer();
    if (!nodeRenderer || !nodeRenderer->isText() || !toRenderText(nodeRenderer)->isTextFragment())
        return;

    if (isDescendantOf(toRenderTextFragment(nodeRenderer)->m_firstLetter))
        result.setIsFirstLetter(true);
}

} // namespace WebCore
