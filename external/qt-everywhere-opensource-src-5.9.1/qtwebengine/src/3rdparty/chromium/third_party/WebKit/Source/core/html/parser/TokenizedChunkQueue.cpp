// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TokenizedChunkQueue.h"

#include "platform/RuntimeEnabledFeatures.h"
#include <algorithm>
#include <memory>

namespace blink {

namespace {

// TODO(csharrison): Remove this temporary class when the ParseHTMLOnMainThread
// experiment ends.
class MaybeLocker {
 public:
  MaybeLocker(Mutex* mutex) : m_mutex(mutex) {
    if (m_mutex)
      m_mutex->lock();
  }
  ~MaybeLocker() {
    if (m_mutex)
      m_mutex->unlock();
  }

 private:
  Mutex* m_mutex;
};

}  // namespace

TokenizedChunkQueue::TokenizedChunkQueue()
    : m_mutex(RuntimeEnabledFeatures::parseHTMLOnMainThreadEnabled()
                  ? nullptr
                  : new Mutex) {}

TokenizedChunkQueue::~TokenizedChunkQueue() {}

bool TokenizedChunkQueue::enqueue(
    std::unique_ptr<HTMLDocumentParser::TokenizedChunk> chunk) {
  MaybeLocker locker(m_mutex.get());

  m_pendingTokenCount += chunk->tokens->size();
  m_peakPendingTokenCount =
      std::max(m_peakPendingTokenCount, m_pendingTokenCount);

  bool wasEmpty = m_pendingChunks.isEmpty();
  m_pendingChunks.append(std::move(chunk));
  m_peakPendingChunkCount =
      std::max(m_peakPendingChunkCount, m_pendingChunks.size());

  return wasEmpty;
}

void TokenizedChunkQueue::clear() {
  MaybeLocker locker(m_mutex.get());

  m_pendingTokenCount = 0;
  m_pendingChunks.clear();
}

void TokenizedChunkQueue::takeAll(
    Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>& vector) {
  MaybeLocker locker(m_mutex.get());

  DCHECK(vector.isEmpty());
  m_pendingChunks.swap(vector);
}

size_t TokenizedChunkQueue::peakPendingChunkCount() {
  MaybeLocker locker(m_mutex.get());
  return m_peakPendingChunkCount;
}

size_t TokenizedChunkQueue::peakPendingTokenCount() {
  MaybeLocker locker(m_mutex.get());
  return m_peakPendingTokenCount;
}

}  // namespace blink
