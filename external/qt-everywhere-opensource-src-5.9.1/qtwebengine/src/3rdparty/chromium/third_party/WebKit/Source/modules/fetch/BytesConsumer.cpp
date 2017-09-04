// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumer.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "platform/WebTaskRunner.h"
#include "platform/blob/BlobData.h"
#include "wtf/Functional.h"
#include "wtf/RefPtr.h"
#include <algorithm>
#include <string.h>

namespace blink {

namespace {

class NoopClient final : public GarbageCollectedFinalized<NoopClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(NoopClient);

 public:
  void onStateChange() override {}
};

class Tee final : public GarbageCollectedFinalized<Tee>,
                  public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(Tee);

 public:
  Tee(ExecutionContext* executionContext, BytesConsumer* consumer)
      : m_src(consumer),
        m_destination1(new Destination(executionContext, this)),
        m_destination2(new Destination(executionContext, this)) {
    consumer->setClient(this);
    // As no client is set to either destinations, Destination::notify() is
    // no-op in this function.
    onStateChange();
  }

  void onStateChange() override {
    bool destination1WasEmpty = m_destination1->isEmpty();
    bool destination2WasEmpty = m_destination2->isEmpty();
    bool hasEnqueued = false;

    while (true) {
      const char* buffer = nullptr;
      size_t available = 0;
      auto result = m_src->beginRead(&buffer, &available);
      if (result == Result::ShouldWait) {
        if (hasEnqueued && destination1WasEmpty)
          m_destination1->notify();
        if (hasEnqueued && destination2WasEmpty)
          m_destination2->notify();
        return;
      }
      Chunk* chunk = nullptr;
      if (result == Result::Ok) {
        chunk = new Chunk(buffer, available);
        result = m_src->endRead(available);
      }
      switch (result) {
        case Result::Ok:
          DCHECK(chunk);
          m_destination1->enqueue(chunk);
          m_destination2->enqueue(chunk);
          hasEnqueued = true;
          break;
        case Result::ShouldWait:
          NOTREACHED();
          return;
        case Result::Done:
          if (destination1WasEmpty)
            m_destination1->notify();
          if (destination2WasEmpty)
            m_destination2->notify();
          return;
        case Result::Error:
          clearAndNotify();
          return;
      }
    }
  }

  BytesConsumer::PublicState getPublicState() const {
    return m_src->getPublicState();
  }

  BytesConsumer::Error getError() const { return m_src->getError(); }

  void cancel() {
    if (!m_destination1->isCancelled() || !m_destination2->isCancelled())
      return;
    m_src->cancel();
  }

  BytesConsumer* destination1() const { return m_destination1; }
  BytesConsumer* destination2() const { return m_destination2; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_src);
    visitor->trace(m_destination1);
    visitor->trace(m_destination2);
    BytesConsumer::Client::trace(visitor);
  }

 private:
  using Result = BytesConsumer::Result;
  class Chunk final : public GarbageCollectedFinalized<Chunk> {
   public:
    Chunk(const char* data, size_t size) {
      m_buffer.reserveInitialCapacity(size);
      m_buffer.append(data, size);
    }
    const char* data() const { return m_buffer.data(); }
    size_t size() const { return m_buffer.size(); }

    DEFINE_INLINE_TRACE() {}

   private:
    Vector<char> m_buffer;
  };

  class Destination final : public BytesConsumer {
   public:
    Destination(ExecutionContext* executionContext, Tee* tee)
        : m_executionContext(executionContext), m_tee(tee) {}

    Result beginRead(const char** buffer, size_t* available) override {
      DCHECK(!m_chunkInUse);
      *buffer = nullptr;
      *available = 0;
      if (m_isCancelled || m_isClosed)
        return Result::Done;
      if (!m_chunks.isEmpty()) {
        Chunk* chunk = m_chunks[0];
        DCHECK_LE(m_offset, chunk->size());
        *buffer = chunk->data() + m_offset;
        *available = chunk->size() - m_offset;
        m_chunkInUse = chunk;
        return Result::Ok;
      }
      switch (m_tee->getPublicState()) {
        case PublicState::ReadableOrWaiting:
          return Result::ShouldWait;
        case PublicState::Closed:
          m_isClosed = true;
          clearClient();
          return Result::Done;
        case PublicState::Errored:
          clearClient();
          return Result::Error;
      }
      NOTREACHED();
      return Result::Error;
    }

    Result endRead(size_t read) override {
      DCHECK(m_chunkInUse);
      DCHECK(m_chunks.isEmpty() || m_chunkInUse == m_chunks[0]);
      m_chunkInUse = nullptr;
      if (m_chunks.isEmpty()) {
        // This object becomes errored during the two-phase read.
        DCHECK_EQ(PublicState::Errored, getPublicState());
        return Result::Ok;
      }
      Chunk* chunk = m_chunks[0];
      DCHECK_LE(m_offset + read, chunk->size());
      m_offset += read;
      if (chunk->size() == m_offset) {
        m_offset = 0;
        m_chunks.removeFirst();
      }
      if (m_chunks.isEmpty() &&
          m_tee->getPublicState() == PublicState::Closed) {
        // All data has been consumed.
        TaskRunnerHelper::get(TaskType::Networking, m_executionContext)
            ->postTask(BLINK_FROM_HERE,
                       WTF::bind(&Destination::close, wrapPersistent(this)));
      }
      return Result::Ok;
    }

    void setClient(BytesConsumer::Client* client) override {
      DCHECK(!m_client);
      DCHECK(client);
      auto state = getPublicState();
      if (state == PublicState::Closed || state == PublicState::Errored)
        return;
      m_client = client;
    }

    void clearClient() override { m_client = nullptr; }

    void cancel() override {
      DCHECK(!m_chunkInUse);
      auto state = getPublicState();
      if (state == PublicState::Closed || state == PublicState::Errored)
        return;
      m_isCancelled = true;
      clearChunks();
      clearClient();
      m_tee->cancel();
    }

    PublicState getPublicState() const override {
      if (m_isCancelled || m_isClosed)
        return PublicState::Closed;
      auto state = m_tee->getPublicState();
      // We don't say this object is closed unless m_isCancelled or
      // m_isClosed is set.
      return state == PublicState::Closed ? PublicState::ReadableOrWaiting
                                          : state;
    }

    Error getError() const override { return m_tee->getError(); }

    String debugName() const override { return "Tee::Destination"; }

    void enqueue(Chunk* chunk) {
      if (m_isCancelled)
        return;
      m_chunks.append(chunk);
    }

    bool isEmpty() const { return m_chunks.isEmpty(); }

    void clearChunks() {
      m_chunks.clear();
      m_offset = 0;
    }

    void notify() {
      if (m_isCancelled || m_isClosed)
        return;
      if (m_chunks.isEmpty() &&
          m_tee->getPublicState() == PublicState::Closed) {
        close();
        return;
      }
      if (m_client) {
        m_client->onStateChange();
        if (getPublicState() == PublicState::Errored)
          clearClient();
      }
    }

    bool isCancelled() const { return m_isCancelled; }

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_executionContext);
      visitor->trace(m_tee);
      visitor->trace(m_client);
      visitor->trace(m_chunks);
      visitor->trace(m_chunkInUse);
      BytesConsumer::trace(visitor);
    }

   private:
    void close() {
      DCHECK_EQ(PublicState::Closed, m_tee->getPublicState());
      DCHECK(m_chunks.isEmpty());
      if (m_isClosed || m_isCancelled) {
        // It's possible to reach here because this function can be
        // called asynchronously.
        return;
      }
      DCHECK_EQ(PublicState::ReadableOrWaiting, getPublicState());
      m_isClosed = true;
      if (m_client) {
        m_client->onStateChange();
        clearClient();
      }
    }

    Member<ExecutionContext> m_executionContext;
    Member<Tee> m_tee;
    Member<BytesConsumer::Client> m_client;
    HeapDeque<Member<Chunk>> m_chunks;
    Member<Chunk> m_chunkInUse;
    size_t m_offset = 0;
    bool m_isCancelled = false;
    bool m_isClosed = false;
  };

  void clearAndNotify() {
    m_destination1->clearChunks();
    m_destination2->clearChunks();
    m_destination1->notify();
    m_destination2->notify();
  }

  Member<BytesConsumer> m_src;
  Member<Destination> m_destination1;
  Member<Destination> m_destination2;
};

class ErroredBytesConsumer final : public BytesConsumer {
 public:
  explicit ErroredBytesConsumer(const Error& error) : m_error(error) {}

  Result beginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::Error;
  }
  Result endRead(size_t readSize) override {
    NOTREACHED();
    return Result::Error;
  }
  void setClient(BytesConsumer::Client*) override {}
  void clearClient() override {}

  void cancel() override {}
  PublicState getPublicState() const override { return PublicState::Errored; }
  Error getError() const override { return m_error; }
  String debugName() const override { return "ErroredBytesConsumer"; }

 private:
  const Error m_error;
};

class ClosedBytesConsumer final : public BytesConsumer {
 public:
  Result beginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::Done;
  }
  Result endRead(size_t readSize) override {
    NOTREACHED();
    return Result::Error;
  }
  void setClient(BytesConsumer::Client*) override {}
  void clearClient() override {}

  void cancel() override {}
  PublicState getPublicState() const override { return PublicState::Closed; }
  Error getError() const override {
    NOTREACHED();
    return Error();
  }
  String debugName() const override { return "ClosedBytesConsumer"; }
};

}  // namespace

void BytesConsumer::tee(ExecutionContext* executionContext,
                        BytesConsumer* src,
                        BytesConsumer** dest1,
                        BytesConsumer** dest2) {
  RefPtr<BlobDataHandle> blobDataHandle =
      src->drainAsBlobDataHandle(BlobSizePolicy::AllowBlobWithInvalidSize);
  if (blobDataHandle) {
    // Register a client in order to be consistent.
    src->setClient(new NoopClient);
    *dest1 = new BlobBytesConsumer(executionContext, blobDataHandle);
    *dest2 = new BlobBytesConsumer(executionContext, blobDataHandle);
    return;
  }

  Tee* tee = new Tee(executionContext, src);
  *dest1 = tee->destination1();
  *dest2 = tee->destination2();
}

BytesConsumer* BytesConsumer::createErrored(const BytesConsumer::Error& error) {
  return new ErroredBytesConsumer(error);
}

BytesConsumer* BytesConsumer::createClosed() {
  return new ClosedBytesConsumer();
}

}  // namespace blink
