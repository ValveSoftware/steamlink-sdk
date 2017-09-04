// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlobBytesConsumer_h
#define BlobBytesConsumer_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class BlobDataHandle;
class EncodedFormData;
class ExecutionContext;
class ThreadableLoader;
class WebDataConsumerHandle;

// A BlobBytesConsumer is created from a blob handle and it will
// return a valid handle from drainAsBlobDataHandle as much as possible.
class MODULES_EXPORT BlobBytesConsumer final : public BytesConsumer,
                                               public ContextLifecycleObserver,
                                               public BytesConsumer::Client,
                                               public ThreadableLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(BlobBytesConsumer);
  USING_PRE_FINALIZER(BlobBytesConsumer, cancel);

 public:
  // |handle| can be null. In that case this consumer gets closed.
  BlobBytesConsumer(ExecutionContext*, PassRefPtr<BlobDataHandle> /* handle */);
  ~BlobBytesConsumer() override;

  // BytesConsumer implementation
  Result beginRead(const char** buffer, size_t* available) override;
  Result endRead(size_t readSize) override;
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy) override;
  PassRefPtr<EncodedFormData> drainAsFormData() override;
  void setClient(BytesConsumer::Client*) override;
  void clearClient() override;
  void cancel() override;
  PublicState getPublicState() const override;
  Error getError() const override;
  String debugName() const override { return "BlobBytesConsumer"; }

  // ContextLifecycleObserver implementation
  void contextDestroyed() override;

  // BytesConsumer::Client implementation
  void onStateChange() override;

  // ThreadableLoaderClient implementation
  void didReceiveResponse(unsigned long identifier,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void didFinishLoading(unsigned long identifier, double finishTime) override;
  void didFail(const ResourceError&) override;
  void didFailRedirectCheck() override;

  DECLARE_TRACE();

  static BlobBytesConsumer* createForTesting(ExecutionContext*,
                                             PassRefPtr<BlobDataHandle>,
                                             ThreadableLoader*);

 private:
  BlobBytesConsumer(ExecutionContext*,
                    PassRefPtr<BlobDataHandle>,
                    ThreadableLoader*);
  ThreadableLoader* createLoader();
  void didFailInternal();
  bool isClean() const { return m_blobDataHandle.get(); }
  void close();
  void error();
  void clear();

  KURL m_blobURL;
  RefPtr<BlobDataHandle> m_blobDataHandle;
  Member<BytesConsumer> m_body;
  Member<BytesConsumer::Client> m_client;
  Member<ThreadableLoader> m_loader;

  PublicState m_state = PublicState::ReadableOrWaiting;
  // These two booleans are meaningful only when m_state is ReadableOrWaiting.
  bool m_hasSeenEndOfData = false;
  bool m_hasFinishedLoading = false;
};

}  // namespace blink

#endif  // BlobBytesConsumer_h
