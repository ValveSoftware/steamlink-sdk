// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FormDataBytesConsumer_h
#define FormDataBytesConsumer_h

#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class EncodedFormData;

class FormDataBytesConsumer final : public BytesConsumer {
 public:
  explicit MODULES_EXPORT FormDataBytesConsumer(const String&);
  explicit MODULES_EXPORT FormDataBytesConsumer(DOMArrayBuffer*);
  explicit MODULES_EXPORT FormDataBytesConsumer(DOMArrayBufferView*);
  MODULES_EXPORT FormDataBytesConsumer(const void* data, size_t);
  MODULES_EXPORT FormDataBytesConsumer(ExecutionContext*,
                                       PassRefPtr<EncodedFormData>);
  MODULES_EXPORT static FormDataBytesConsumer* createForTesting(
      ExecutionContext* executionContext,
      PassRefPtr<EncodedFormData> formData,
      BytesConsumer* consumer) {
    return new FormDataBytesConsumer(executionContext, std::move(formData),
                                     consumer);
  }

  // BytesConsumer implementation
  Result beginRead(const char** buffer, size_t* available) override {
    return m_impl->beginRead(buffer, available);
  }
  Result endRead(size_t readSize) override { return m_impl->endRead(readSize); }
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    return m_impl->drainAsBlobDataHandle(policy);
  }
  PassRefPtr<EncodedFormData> drainAsFormData() override {
    return m_impl->drainAsFormData();
  }
  void setClient(BytesConsumer::Client* client) override {
    m_impl->setClient(client);
  }
  void clearClient() override { m_impl->clearClient(); }
  void cancel() override { m_impl->cancel(); }
  PublicState getPublicState() const override {
    return m_impl->getPublicState();
  }
  Error getError() const override { return m_impl->getError(); }
  String debugName() const override { return m_impl->debugName(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_impl);
    BytesConsumer::trace(visitor);
  }

 private:
  MODULES_EXPORT FormDataBytesConsumer(ExecutionContext*,
                                       PassRefPtr<EncodedFormData>,
                                       BytesConsumer*);

  const Member<BytesConsumer> m_impl;
};

}  // namespace blink

#endif  // FormDataBytesConsumer_h
