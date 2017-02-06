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

#include "core/editing/commands/InsertNodeBeforeCommand.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"

namespace blink {

InsertNodeBeforeCommand::InsertNodeBeforeCommand(Node* insertChild, Node* refChild,
    ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
    : SimpleEditCommand(refChild->document())
    , m_insertChild(insertChild)
    , m_refChild(refChild)
    , m_shouldAssumeContentIsAlwaysEditable(shouldAssumeContentIsAlwaysEditable)
{
    DCHECK(m_insertChild);
    DCHECK(!m_insertChild->parentNode()) << m_insertChild;
    DCHECK(m_refChild);
    DCHECK(m_refChild->parentNode()) << m_refChild;

    DCHECK(m_refChild->parentNode()->hasEditableStyle() || !m_refChild->parentNode()->inActiveDocument()) << m_refChild->parentNode();
}

void InsertNodeBeforeCommand::doApply(EditingState*)
{
    ContainerNode* parent = m_refChild->parentNode();
    if (!parent || (m_shouldAssumeContentIsAlwaysEditable == DoNotAssumeContentIsAlwaysEditable && !parent->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable)))
        return;
    DCHECK(parent->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable)) << parent;

    parent->insertBefore(m_insertChild.get(), m_refChild.get(), IGNORE_EXCEPTION);
}

void InsertNodeBeforeCommand::doUnapply()
{
    if (!m_insertChild->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable))
        return;

    m_insertChild->remove(IGNORE_EXCEPTION);
}

DEFINE_TRACE(InsertNodeBeforeCommand)
{
    visitor->trace(m_insertChild);
    visitor->trace(m_refChild);
    SimpleEditCommand::trace(visitor);
}

} // namespace blink
