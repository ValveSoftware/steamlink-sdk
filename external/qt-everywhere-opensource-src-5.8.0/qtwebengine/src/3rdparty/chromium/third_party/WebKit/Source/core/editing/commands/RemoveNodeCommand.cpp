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

#include "core/editing/commands/RemoveNodeCommand.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Node.h"
#include "core/editing/commands/EditingState.h"
#include "wtf/Assertions.h"

namespace blink {

RemoveNodeCommand::RemoveNodeCommand(Node* node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
    : SimpleEditCommand(node->document())
    , m_node(node)
    , m_shouldAssumeContentIsAlwaysEditable(shouldAssumeContentIsAlwaysEditable)
{
    DCHECK(m_node);
    DCHECK(m_node->parentNode());
}

void RemoveNodeCommand::doApply(EditingState* editingState)
{
    ContainerNode* parent = m_node->parentNode();
    if (!parent || (m_shouldAssumeContentIsAlwaysEditable == DoNotAssumeContentIsAlwaysEditable
        && !parent->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable) && parent->inActiveDocument()))
        return;
    DCHECK(parent->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable) || !parent->inActiveDocument()) << parent;

    m_parent = parent;
    m_refChild = m_node->nextSibling();

    m_node->remove(IGNORE_EXCEPTION);
    // Node::remove dispatch synchronous events such as IFRAME unload events,
    // and event handlers may break the document. We check the document state
    // here in order to prevent further processing in bad situation.
    ABORT_EDITING_COMMAND_IF(!m_node->document().frame());
    ABORT_EDITING_COMMAND_IF(!m_node->document().documentElement());
}

void RemoveNodeCommand::doUnapply()
{
    ContainerNode* parent = m_parent.release();
    Node* refChild = m_refChild.release();
    if (!parent || !parent->hasEditableStyle())
        return;

    parent->insertBefore(m_node.get(), refChild, IGNORE_EXCEPTION);
}

DEFINE_TRACE(RemoveNodeCommand)
{
    visitor->trace(m_node);
    visitor->trace(m_parent);
    visitor->trace(m_refChild);
    SimpleEditCommand::trace(visitor);
}

} // namespace blink
