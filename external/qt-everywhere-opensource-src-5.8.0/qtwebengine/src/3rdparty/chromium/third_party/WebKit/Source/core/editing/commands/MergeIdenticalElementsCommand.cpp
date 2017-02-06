/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#include "core/editing/commands/MergeIdenticalElementsCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Element.h"

namespace blink {

MergeIdenticalElementsCommand::MergeIdenticalElementsCommand(Element* first, Element* second)
    : SimpleEditCommand(first->document())
    , m_element1(first)
    , m_element2(second)
{
    DCHECK(m_element1);
    DCHECK(m_element2);
    DCHECK_EQ(m_element1->nextSibling(), m_element2);
}

void MergeIdenticalElementsCommand::doApply(EditingState*)
{
    if (m_element1->nextSibling() != m_element2 || !m_element1->hasEditableStyle() || !m_element2->hasEditableStyle())
        return;

    m_atChild = m_element2->firstChild();

    NodeVector children;
    getChildNodes(*m_element1, children);

    for (auto& child : children)
        m_element2->insertBefore(child.release(), m_atChild.get(), IGNORE_EXCEPTION);

    m_element1->remove(IGNORE_EXCEPTION);
}

void MergeIdenticalElementsCommand::doUnapply()
{
    DCHECK(m_element1);
    DCHECK(m_element2);

    Node* atChild = m_atChild.release();

    ContainerNode* parent = m_element2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;

    TrackExceptionState exceptionState;

    parent->insertBefore(m_element1.get(), m_element2.get(), exceptionState);
    if (exceptionState.hadException())
        return;

    HeapVector<Member<Node>> children;
    for (Node* child = m_element2->firstChild(); child && child != atChild; child = child->nextSibling())
        children.append(child);

    for (auto& child : children)
        m_element1->appendChild(child.release(), exceptionState);
}

DEFINE_TRACE(MergeIdenticalElementsCommand)
{
    visitor->trace(m_element1);
    visitor->trace(m_element2);
    visitor->trace(m_atChild);
    SimpleEditCommand::trace(visitor);
}

} // namespace blink
