// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://discourse.wicg.io/t/proposal-staticrange-to-be-used-instead-of-range-for-new-apis/1472

#include "core/dom/StaticRange.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Range.h"
#include "core/frame/LocalDOMWindow.h"

namespace blink {

StaticRange::StaticRange(Document& document)
    : m_ownerDocument(document)
    , m_startContainer(nullptr)
    , m_startOffset(0)
    , m_endContainer(nullptr)
    , m_endOffset(0)
{
}

StaticRange::StaticRange(Document& document, Node* startContainer, int startOffset, Node* endContainer, int endOffset)
    : m_ownerDocument(document)
    , m_startContainer(startContainer)
    , m_startOffset(startOffset)
    , m_endContainer(endContainer)
    , m_endOffset(endOffset)
{
}

void StaticRange::setStart(Node* container, int offset)
{
    m_startContainer = container;
    m_startOffset = offset;
}

void StaticRange::setEnd(Node* container, int offset)
{
    m_endContainer = container;
    m_endOffset = offset;
}

Range* StaticRange::toRange(ExceptionState& exceptionState) const
{
    Range* range = Range::create(*m_ownerDocument.get());
    // Do the offset checking.
    range->setStart(m_startContainer, m_startOffset, exceptionState);
    range->setEnd(m_endContainer, m_endOffset, exceptionState);
    return range;
}

DEFINE_TRACE(StaticRange)
{
    visitor->trace(m_ownerDocument);
    visitor->trace(m_startContainer);
    visitor->trace(m_endContainer);
}

} // namespace blink
