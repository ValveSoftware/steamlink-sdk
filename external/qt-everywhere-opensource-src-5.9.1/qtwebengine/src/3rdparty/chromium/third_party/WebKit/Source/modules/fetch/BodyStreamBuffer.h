// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMException.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include <memory>

namespace blink {

class EncodedFormData;
class ScriptState;

class MODULES_EXPORT BodyStreamBuffer final : public UnderlyingSourceBase,
                                              public BytesConsumer::Client {
  WTF_MAKE_NONCOPYABLE(BodyStreamBuffer);
  USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);

 public:
  // |consumer| must not have a client.
  // This function must be called with entering an appropriate V8 context.
  BodyStreamBuffer(ScriptState*, BytesConsumer* /* consumer */);
  // |ReadableStreamOperations::isReadableStream(stream)| must hold.
  // This function must be called with entering an appropriate V8 context.
  BodyStreamBuffer(ScriptState*, ScriptValue stream);

  ScriptValue stream();

  // Callable only when neither locked nor disturbed.
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy);
  PassRefPtr<EncodedFormData> drainAsFormData();
  void startLoading(FetchDataLoader*, FetchDataLoader::Client* /* client */);
  void tee(BodyStreamBuffer**, BodyStreamBuffer**);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise cancel(ScriptState*, ScriptValue reason) override;
  bool hasPendingActivity() const override;
  void contextDestroyed() override;

  // BytesConsumer::Client
  void onStateChange() override;

  bool isStreamReadable();
  bool isStreamClosed();
  bool isStreamErrored();
  bool isStreamLocked();
  bool isStreamDisturbed();
  void closeAndLockAndDisturb();
  ScriptState* scriptState() { return m_scriptState.get(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_loader);
    UnderlyingSourceBase::trace(visitor);
  }

 private:
  class LoaderClient;

  BytesConsumer* releaseHandle();
  void close();
  void error();
  void cancelConsumer();
  void processData();
  void endLoading();
  void stopLoading();

  RefPtr<ScriptState> m_scriptState;
  Member<BytesConsumer> m_consumer;
  // We need this member to keep it alive while loading.
  Member<FetchDataLoader> m_loader;
  bool m_streamNeedsMore = false;
  bool m_madeFromReadableStream;
};

}  // namespace blink

#endif  // BodyStreamBuffer_h
