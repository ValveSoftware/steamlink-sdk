// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TokenizedChunkQueue_h
#define TokenizedChunkQueue_h

#include "core/html/parser/HTMLDocumentParser.h"
#include "wtf/Deque.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

// TokenizedChunkQueue is used to transfer parsed HTML token chunks
// from BackgroundHTMLParser thread to Blink main thread without
// spamming task queue.
// TokenizedChunkQueue is accessed from both BackgroundHTMLParser
// thread (producer) and Blink main thread (consumer).
// Access to the backend queue vector is protected by a mutex.
// If enqueue is done against empty queue, BackgroundHTMLParser
// thread kicks a consumer task on Blink main thread.
class TokenizedChunkQueue : public ThreadSafeRefCounted<TokenizedChunkQueue> {
 public:
  static PassRefPtr<TokenizedChunkQueue> create() {
    return adoptRef(new TokenizedChunkQueue);
  }

  ~TokenizedChunkQueue();

  bool enqueue(std::unique_ptr<HTMLDocumentParser::TokenizedChunk>);
  void clear();

  void takeAll(Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>&);
  size_t peakPendingChunkCount();
  size_t peakPendingTokenCount();

 private:
  TokenizedChunkQueue();

  std::unique_ptr<Mutex> m_mutex;
  Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>> m_pendingChunks;
  size_t m_peakPendingChunkCount = 0;
  size_t m_peakPendingTokenCount = 0;
  size_t m_pendingTokenCount = 0;
};

}  // namespace blink

#endif
