// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://discourse.wicg.io/t/proposal-staticrange-to-be-used-instead-of-range-for-new-apis/1472

#ifndef StaticRange_h
#define StaticRange_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class ExceptionState;
class Range;

class CORE_EXPORT StaticRange final : public GarbageCollected<StaticRange>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static StaticRange* create(Document& document)
    {
        return new StaticRange(document);
    }
    static StaticRange* create(Document& document, Node* startContainer, int startOffset, Node* endContainer, int endOffset)
    {
        return new StaticRange(document, startContainer, startOffset, endContainer, endOffset);
    }

    Node* startContainer() const { return m_startContainer.get(); }
    void setStartContainer(Node* startContainer) { m_startContainer = startContainer; }

    int startOffset() const { return m_startOffset; }
    void setStartOffset(int startOffset) { m_startOffset = startOffset; }

    Node* endContainer() const { return m_endContainer.get(); }
    void setEndContainer(Node* endContainer) { m_endContainer = endContainer; }

    int endOffset() const { return m_endOffset; }
    void setEndOffset(int endOffset) { m_endOffset = endOffset; }

    bool collapsed() const { return m_startContainer == m_endContainer && m_startOffset == m_endOffset; }

    void setStart(Node* container, int offset);
    void setEnd(Node* container, int offset);

    Range* toRange(ExceptionState&) const;

    DECLARE_TRACE();

private:
    explicit StaticRange(Document&);
    StaticRange(Document&, Node* startContainer, int startOffset, Node* endContainer, int endOffset);

    Member<Document> m_ownerDocument; // Required by |toRange()|.
    Member<Node> m_startContainer;
    int m_startOffset;
    Member<Node> m_endContainer;
    int m_endOffset;
};

using StaticRangeVector = HeapVector<Member<StaticRange>>;

} // namespace blink

#endif // StaticRange_h
