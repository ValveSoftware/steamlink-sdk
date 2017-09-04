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

#include "core/editing/commands/RemoveNodePreservingChildrenCommand.h"

#include "core/dom/Node.h"
#include "core/editing/EditingUtilities.h"
#include "wtf/Assertions.h"

namespace blink {

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(
    Node* node,
    ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
    : CompositeEditCommand(node->document()),
      m_node(node),
      m_shouldAssumeContentIsAlwaysEditable(
          shouldAssumeContentIsAlwaysEditable) {
  DCHECK(m_node);
}

void RemoveNodePreservingChildrenCommand::doApply(EditingState* editingState) {
  ABORT_EDITING_COMMAND_IF(!m_node->parentNode());
  ABORT_EDITING_COMMAND_IF(!hasEditableStyle(*m_node->parentNode()));
  if (m_node->isContainerNode()) {
    NodeVector children;
    getChildNodes(toContainerNode(*m_node), children);

    for (auto& currentChild : children) {
      Node* child = currentChild.release();
      removeNode(child, editingState, m_shouldAssumeContentIsAlwaysEditable);
      if (editingState->isAborted())
        return;
      insertNodeBefore(child, m_node, editingState,
                       m_shouldAssumeContentIsAlwaysEditable);
      if (editingState->isAborted())
        return;
    }
  }
  removeNode(m_node, editingState, m_shouldAssumeContentIsAlwaysEditable);
}

DEFINE_TRACE(RemoveNodePreservingChildrenCommand) {
  visitor->trace(m_node);
  CompositeEditCommand::trace(visitor);
}

}  // namespace blink
