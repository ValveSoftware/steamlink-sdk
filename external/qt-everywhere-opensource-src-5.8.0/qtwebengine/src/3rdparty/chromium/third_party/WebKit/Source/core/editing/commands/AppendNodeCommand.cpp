/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/editing/commands/AppendNodeCommand.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

AppendNodeCommand::AppendNodeCommand(ContainerNode* parent, Node* node)
    : SimpleEditCommand(parent->document())
    , m_parent(parent)
    , m_node(node)
{
    DCHECK(m_parent);
    DCHECK(m_node);
    DCHECK(!m_node->parentNode()) << m_node;

    DCHECK(m_parent->hasEditableStyle() || !m_parent->inActiveDocument()) << m_parent;
}

void AppendNodeCommand::doApply(EditingState*)
{
    if (!m_parent->hasEditableStyle() && m_parent->inActiveDocument())
        return;

    m_parent->appendChild(m_node.get(), IGNORE_EXCEPTION);
}

void AppendNodeCommand::doUnapply()
{
    if (!m_parent->hasEditableStyle())
        return;

    m_node->remove(IGNORE_EXCEPTION);
}

DEFINE_TRACE(AppendNodeCommand)
{
    visitor->trace(m_parent);
    visitor->trace(m_node);
    SimpleEditCommand::trace(visitor);
}

} // namespace blink
