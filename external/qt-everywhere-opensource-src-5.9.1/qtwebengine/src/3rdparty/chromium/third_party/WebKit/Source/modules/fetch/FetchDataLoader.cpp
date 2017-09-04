// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "modules/fetch/BytesConsumer.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include "wtf/typed_arrays/ArrayBufferBuilder.h"
#include <memory>

namespace blink {

namespace {

class FetchDataLoaderAsBlobHandle final : public FetchDataLoader,
                                          public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsBlobHandle);

 public:
  explicit FetchDataLoaderAsBlobHandle(const String& mimeType)
      : m_mimeType(mimeType) {}

  void start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!m_client);
    DCHECK(!m_consumer);

    m_client = client;
    m_consumer = consumer;

    RefPtr<BlobDataHandle> blobHandle = m_consumer->drainAsBlobDataHandle();
    if (blobHandle) {
      DCHECK_NE(UINT64_MAX, blobHandle->size());
      if (blobHandle->type() != m_mimeType) {
        // A new BlobDataHandle is created to override the Blob's type.
        m_client->didFetchDataLoadedBlobHandle(BlobDataHandle::create(
            blobHandle->uuid(), m_mimeType, blobHandle->size()));
      } else {
        m_client->didFetchDataLoadedBlobHandle(std::move(blobHandle));
      }
      return;
    }

    m_blobData = BlobData::create();
    m_blobData->setContentType(m_mimeType);
    m_consumer->setClient(this);
    onStateChange();
  }

  void cancel() override { m_consumer->cancel(); }

  void onStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = m_consumer->beginRead(&buffer, &available);
      if (result == BytesConsumer::Result::ShouldWait)
        return;
      if (result == BytesConsumer::Result::Ok) {
        m_blobData->appendBytes(buffer, available);
        result = m_consumer->endRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::Ok:
          break;
        case BytesConsumer::Result::ShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::Done: {
          auto size = m_blobData->length();
          m_client->didFetchDataLoadedBlobHandle(
              BlobDataHandle::create(std::move(m_blobData), size));
          return;
        }
        case BytesConsumer::Result::Error:
          m_client->didFetchDataLoadFailed();
          return;
      }
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_client);
    FetchDataLoader::trace(visitor);
    BytesConsumer::Client::trace(visitor);
  }

 private:
  Member<BytesConsumer> m_consumer;
  Member<FetchDataLoader::Client> m_client;

  String m_mimeType;
  std::unique_ptr<BlobData> m_blobData;
};

class FetchDataLoaderAsArrayBuffer final : public FetchDataLoader,
                                           public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsArrayBuffer)
 public:
  void start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!m_client);
    DCHECK(!m_rawData);
    DCHECK(!m_consumer);
    m_client = client;
    m_rawData = makeUnique<ArrayBufferBuilder>();
    m_consumer = consumer;
    m_consumer->setClient(this);
    onStateChange();
  }

  void cancel() override { m_consumer->cancel(); }

  void onStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = m_consumer->beginRead(&buffer, &available);
      if (result == BytesConsumer::Result::ShouldWait)
        return;
      if (result == BytesConsumer::Result::Ok) {
        if (available > 0) {
          unsigned bytesAppended = m_rawData->append(buffer, available);
          if (!bytesAppended) {
            auto unused = m_consumer->endRead(0);
            ALLOW_UNUSED_LOCAL(unused);
            m_consumer->cancel();
            m_client->didFetchDataLoadFailed();
            return;
          }
          DCHECK_EQ(bytesAppended, available);
        }
        result = m_consumer->endRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::Ok:
          break;
        case BytesConsumer::Result::ShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::Done:
          m_client->didFetchDataLoadedArrayBuffer(
              DOMArrayBuffer::create(m_rawData->toArrayBuffer()));
          return;
        case BytesConsumer::Result::Error:
          m_client->didFetchDataLoadFailed();
          return;
      }
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_client);
    FetchDataLoader::trace(visitor);
    BytesConsumer::Client::trace(visitor);
  }

 private:
  Member<BytesConsumer> m_consumer;
  Member<FetchDataLoader::Client> m_client;

  std::unique_ptr<ArrayBufferBuilder> m_rawData;
};

class FetchDataLoaderAsString final : public FetchDataLoader,
                                      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsString);

 public:
  void start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!m_client);
    DCHECK(!m_decoder);
    DCHECK(!m_consumer);
    m_client = client;
    m_decoder = TextResourceDecoder::createAlwaysUseUTF8ForText();
    m_consumer = consumer;
    m_consumer->setClient(this);
    onStateChange();
  }

  void onStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = m_consumer->beginRead(&buffer, &available);
      if (result == BytesConsumer::Result::ShouldWait)
        return;
      if (result == BytesConsumer::Result::Ok) {
        if (available > 0)
          m_builder.append(m_decoder->decode(buffer, available));
        result = m_consumer->endRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::Ok:
          break;
        case BytesConsumer::Result::ShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::Done:
          m_builder.append(m_decoder->flush());
          m_client->didFetchDataLoadedString(m_builder.toString());
          return;
        case BytesConsumer::Result::Error:
          m_client->didFetchDataLoadFailed();
          return;
      }
    }
  }

  void cancel() override { m_consumer->cancel(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_client);
    FetchDataLoader::trace(visitor);
    BytesConsumer::Client::trace(visitor);
  }

 private:
  Member<BytesConsumer> m_consumer;
  Member<FetchDataLoader::Client> m_client;

  std::unique_ptr<TextResourceDecoder> m_decoder;
  StringBuilder m_builder;
};

class FetchDataLoaderAsStream final : public FetchDataLoader,
                                      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsStream);

 public:
  explicit FetchDataLoaderAsStream(Stream* outStream)
      : m_outStream(outStream) {}

  void start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!m_client);
    DCHECK(!m_consumer);
    m_client = client;
    m_consumer = consumer;
    m_consumer->setClient(this);
    onStateChange();
  }

  void onStateChange() override {
    bool needToFlush = false;
    while (true) {
      const char* buffer;
      size_t available;
      auto result = m_consumer->beginRead(&buffer, &available);
      if (result == BytesConsumer::Result::ShouldWait) {
        if (needToFlush)
          m_outStream->flush();
        return;
      }
      if (result == BytesConsumer::Result::Ok) {
        m_outStream->addData(buffer, available);
        needToFlush = true;
        result = m_consumer->endRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::Ok:
          break;
        case BytesConsumer::Result::ShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::Done:
          if (needToFlush)
            m_outStream->flush();
          m_outStream->finalize();
          m_client->didFetchDataLoadedStream();
          return;
        case BytesConsumer::Result::Error:
          // If the stream is aborted soon after the stream is registered
          // to the StreamRegistry, ServiceWorkerURLRequestJob may not
          // notice the error and continue waiting forever.
          // TODO(yhirano): Add new message to report the error to the
          // browser process.
          m_outStream->abort();
          m_client->didFetchDataLoadFailed();
          return;
      }
    }
  }

  void cancel() override { m_consumer->cancel(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_client);
    visitor->trace(m_outStream);
    FetchDataLoader::trace(visitor);
    BytesConsumer::Client::trace(visitor);
  }

  Member<BytesConsumer> m_consumer;
  Member<FetchDataLoader::Client> m_client;
  Member<Stream> m_outStream;
};

}  // namespace

FetchDataLoader* FetchDataLoader::createLoaderAsBlobHandle(
    const String& mimeType) {
  return new FetchDataLoaderAsBlobHandle(mimeType);
}

FetchDataLoader* FetchDataLoader::createLoaderAsArrayBuffer() {
  return new FetchDataLoaderAsArrayBuffer();
}

FetchDataLoader* FetchDataLoader::createLoaderAsString() {
  return new FetchDataLoaderAsString();
}

FetchDataLoader* FetchDataLoader::createLoaderAsStream(Stream* outStream) {
  return new FetchDataLoaderAsStream(outStream);
}

}  // namespace blink
