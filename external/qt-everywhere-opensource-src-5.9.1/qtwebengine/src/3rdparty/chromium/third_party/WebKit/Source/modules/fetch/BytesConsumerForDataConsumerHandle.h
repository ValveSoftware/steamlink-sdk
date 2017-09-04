// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumerForDataConsumerHandle_h
#define BytesConsumerForDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

class ExecutionContext;

class MODULES_EXPORT BytesConsumerForDataConsumerHandle final
    : public BytesConsumer,
      public WebDataConsumerHandle::Client {
  EAGERLY_FINALIZE();
  DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();

 public:
  BytesConsumerForDataConsumerHandle(ExecutionContext*,
                                     std::unique_ptr<WebDataConsumerHandle>);
  ~BytesConsumerForDataConsumerHandle() override;

  Result beginRead(const char** buffer, size_t* available) override;
  Result endRead(size_t readSize) override;
  void setClient(BytesConsumer::Client*) override;
  void clearClient() override;

  void cancel() override;
  PublicState getPublicState() const override;
  Error getError() const override {
    DCHECK(m_state == InternalState::Errored);
    return m_error;
  }
  String debugName() const override {
    return "BytesConsumerForDataConsumerHandle";
  }

  // WebDataConsumerHandle::Client
  void didGetReadable() override;

  DECLARE_TRACE();

 private:
  void close();
  void error();
  void notify();

  Member<ExecutionContext> m_executionContext;
  std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
  Member<BytesConsumer::Client> m_client;
  InternalState m_state = InternalState::Waiting;
  Error m_error;
  bool m_isInTwoPhaseRead = false;
  bool m_hasPendingNotification = false;
};

}  // namespace blink

#endif  // BytesConsumerForDataConsumerHandle_h
