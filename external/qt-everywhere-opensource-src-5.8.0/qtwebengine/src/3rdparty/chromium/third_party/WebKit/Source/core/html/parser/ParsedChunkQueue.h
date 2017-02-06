// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParsedChunkQueue_h
#define ParsedChunkQueue_h

#include "core/html/parser/HTMLDocumentParser.h"
#include "wtf/Deque.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

// ParsedChunkQueue is used to transfer parsed HTML token chunks
// from BackgroundHTMLParser thread to Blink main thread without
// spamming task queue.
// ParsedChunkQueue is accessed from both BackgroundHTMLParser
// thread (producer) and Blink main thread (consumer).
// Access to the backend queue vector is protected by a mutex.
// If enqueue is done against empty queue, BackgroundHTMLParser
// thread kicks a consumer task on Blink main thread.
class ParsedChunkQueue : public ThreadSafeRefCounted<ParsedChunkQueue> {
public:
    static PassRefPtr<ParsedChunkQueue> create()
    {
        return adoptRef(new ParsedChunkQueue);
    }

    ~ParsedChunkQueue();

    bool enqueue(std::unique_ptr<HTMLDocumentParser::ParsedChunk>);
    void clear();

    void takeAll(Vector<std::unique_ptr<HTMLDocumentParser::ParsedChunk>>&);
    size_t peakPendingChunkCount();
    size_t peakPendingTokenCount();

private:
    ParsedChunkQueue();

    std::unique_ptr<Mutex> m_mutex;
    Vector<std::unique_ptr<HTMLDocumentParser::ParsedChunk>> m_pendingChunks;
    size_t m_peakPendingChunkCount = 0;
    size_t m_peakPendingTokenCount = 0;
    size_t m_pendingTokenCount = 0;
};

} // namespace blink

#endif
