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

#include "core/editing/commands/SplitTextNodeCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "wtf/Assertions.h"

namespace blink {

SplitTextNodeCommand::SplitTextNodeCommand(Text* text, int offset)
    : SimpleEditCommand(text->document())
    , m_text2(text)
    , m_offset(offset)
{
    // NOTE: Various callers rely on the fact that the original node becomes
    // the second node (i.e. the new node is inserted before the existing one).
    // That is not a fundamental dependency (i.e. it could be re-coded), but
    // rather is based on how this code happens to work.
    DCHECK(m_text2);
    DCHECK_GT(m_text2->length(), 0u);
    DCHECK_GT(m_offset, 0u);
    DCHECK_LT(m_offset, m_text2->length());
}

void SplitTextNodeCommand::doApply(EditingState*)
{
    ContainerNode* parent = m_text2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;

    String prefixText = m_text2->substringData(0, m_offset, IGNORE_EXCEPTION);
    if (prefixText.isEmpty())
        return;

    m_text1 = Text::create(document(), prefixText);
    DCHECK(m_text1);
    document().markers().copyMarkers(m_text2.get(), 0, m_offset, m_text1.get(), 0);

    insertText1AndTrimText2();
}

void SplitTextNodeCommand::doUnapply()
{
    if (!m_text1 || !m_text1->hasEditableStyle())
        return;

    DCHECK_EQ(m_text1->document(), document());

    String prefixText = m_text1->data();

    m_text2->insertData(0, prefixText, ASSERT_NO_EXCEPTION);
    document().updateStyleAndLayout();

    document().markers().copyMarkers(m_text1.get(), 0, prefixText.length(), m_text2.get(), 0);
    m_text1->remove(ASSERT_NO_EXCEPTION);
}

void SplitTextNodeCommand::doReapply()
{
    if (!m_text1 || !m_text2)
        return;

    ContainerNode* parent = m_text2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;

    insertText1AndTrimText2();
}

void SplitTextNodeCommand::insertText1AndTrimText2()
{
    TrackExceptionState exceptionState;
    m_text2->parentNode()->insertBefore(m_text1.get(), m_text2.get(), exceptionState);
    if (exceptionState.hadException())
        return;
    m_text2->deleteData(0, m_offset, exceptionState);
    document().updateStyleAndLayout();
}

DEFINE_TRACE(SplitTextNodeCommand)
{
    visitor->trace(m_text1);
    visitor->trace(m_text2);
    SimpleEditCommand::trace(visitor);
}

} // namespace blink
