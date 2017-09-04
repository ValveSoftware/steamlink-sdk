// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FormDataBytesConsumer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Vector.h"
#include "wtf/text/TextCodec.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

bool isSimple(const EncodedFormData* formData) {
  for (const auto& element : formData->elements()) {
    if (element.m_type != FormDataElement::data)
      return false;
  }
  return true;
}

class SimpleFormDataBytesConsumer : public BytesConsumer {
 public:
  explicit SimpleFormDataBytesConsumer(PassRefPtr<EncodedFormData> formData)
      : m_formData(formData) {}

  // BytesConsumer implementation
  Result beginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    if (m_formData) {
      m_formData->flatten(m_flattenFormData);
      m_formData = nullptr;
      DCHECK_EQ(m_flattenFormDataOffset, 0u);
    }
    if (m_flattenFormDataOffset == m_flattenFormData.size())
      return Result::Done;
    *buffer = m_flattenFormData.data() + m_flattenFormDataOffset;
    *available = m_flattenFormData.size() - m_flattenFormDataOffset;
    return Result::Ok;
  }
  Result endRead(size_t readSize) override {
    DCHECK(!m_formData);
    DCHECK_LE(m_flattenFormDataOffset + readSize, m_flattenFormData.size());
    m_flattenFormDataOffset += readSize;
    if (m_flattenFormDataOffset == m_flattenFormData.size()) {
      m_state = PublicState::Closed;
      return Result::Done;
    }
    return Result::Ok;
  }
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (!m_formData)
      return nullptr;

    Vector<char> data;
    m_formData->flatten(data);
    m_formData = nullptr;
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->appendBytes(data.data(), data.size());
    auto length = blobData->length();
    m_state = PublicState::Closed;
    return BlobDataHandle::create(std::move(blobData), length);
  }
  PassRefPtr<EncodedFormData> drainAsFormData() override {
    if (!m_formData)
      return nullptr;

    m_state = PublicState::Closed;
    return m_formData.release();
  }
  void setClient(BytesConsumer::Client* client) override { DCHECK(client); }
  void clearClient() override {}
  void cancel() override {
    m_state = PublicState::Closed;
    m_formData = nullptr;
    m_flattenFormData.clear();
    m_flattenFormDataOffset = 0;
  }
  PublicState getPublicState() const override { return m_state; }
  Error getError() const override {
    NOTREACHED();
    return Error();
  }
  String debugName() const override { return "SimpleFormDataBytesConsumer"; }

 private:
  // either one of |m_formData| and |m_flattenFormData| is usable at a time.
  RefPtr<EncodedFormData> m_formData;
  Vector<char> m_flattenFormData;
  size_t m_flattenFormDataOffset = 0;
  PublicState m_state = PublicState::ReadableOrWaiting;
};

class ComplexFormDataBytesConsumer final : public BytesConsumer {
 public:
  ComplexFormDataBytesConsumer(ExecutionContext* executionContext,
                               PassRefPtr<EncodedFormData> formData,
                               BytesConsumer* consumer)
      : m_formData(formData) {
    if (consumer) {
      // For testing.
      m_blobBytesConsumer = consumer;
      return;
    }

    std::unique_ptr<BlobData> blobData = BlobData::create();
    for (const auto& element : m_formData->elements()) {
      switch (element.m_type) {
        case FormDataElement::data:
          blobData->appendBytes(element.m_data.data(), element.m_data.size());
          break;
        case FormDataElement::encodedFile:
          blobData->appendFile(element.m_filename, element.m_fileStart,
                               element.m_fileLength,
                               element.m_expectedFileModificationTime);
          break;
        case FormDataElement::encodedBlob:
          if (element.m_optionalBlobDataHandle)
            blobData->appendBlob(element.m_optionalBlobDataHandle, 0,
                                 element.m_optionalBlobDataHandle->size());
          break;
        case FormDataElement::encodedFileSystemURL:
          blobData->appendFileSystemURL(
              element.m_fileSystemURL, element.m_fileStart,
              element.m_fileLength, element.m_expectedFileModificationTime);
          break;
      }
    }
    // Here we handle m_formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    blobData->setContentType(AtomicString("multipart/form-data; boundary=") +
                             m_formData->boundary().data());
    auto size = blobData->length();
    m_blobBytesConsumer = new BlobBytesConsumer(
        executionContext, BlobDataHandle::create(std::move(blobData), size));
  }

  // BytesConsumer implementation
  Result beginRead(const char** buffer, size_t* available) override {
    m_formData = nullptr;
    // Delegate the operation to the underlying consumer. This relies on
    // the fact that we appropriately notify the draining information to
    // the underlying consumer.
    return m_blobBytesConsumer->beginRead(buffer, available);
  }
  Result endRead(size_t readSize) override {
    return m_blobBytesConsumer->endRead(readSize);
  }
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    RefPtr<BlobDataHandle> handle =
        m_blobBytesConsumer->drainAsBlobDataHandle(policy);
    if (handle)
      m_formData = nullptr;
    return handle.release();
  }
  PassRefPtr<EncodedFormData> drainAsFormData() override {
    if (!m_formData)
      return nullptr;
    m_blobBytesConsumer->cancel();
    return m_formData.release();
  }
  void setClient(BytesConsumer::Client* client) override {
    m_blobBytesConsumer->setClient(client);
  }
  void clearClient() override { m_blobBytesConsumer->clearClient(); }
  void cancel() override {
    m_formData = nullptr;
    m_blobBytesConsumer->cancel();
  }
  PublicState getPublicState() const override {
    return m_blobBytesConsumer->getPublicState();
  }
  Error getError() const override { return m_blobBytesConsumer->getError(); }
  String debugName() const override { return "ComplexFormDataBytesConsumer"; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_blobBytesConsumer);
    BytesConsumer::trace(visitor);
  }

 private:
  RefPtr<EncodedFormData> m_formData;
  Member<BytesConsumer> m_blobBytesConsumer;
};

}  // namespace

FormDataBytesConsumer::FormDataBytesConsumer(const String& string)
    : m_impl(new SimpleFormDataBytesConsumer(EncodedFormData::create(
          UTF8Encoding().encode(string, WTF::EntitiesForUnencodables)))) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBuffer* buffer)
    : FormDataBytesConsumer(buffer->data(), buffer->byteLength()) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBufferView* view)
    : FormDataBytesConsumer(view->baseAddress(), view->byteLength()) {}

FormDataBytesConsumer::FormDataBytesConsumer(const void* data, size_t size)
    : m_impl(new SimpleFormDataBytesConsumer(
          EncodedFormData::create(data, size))) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* executionContext,
    PassRefPtr<EncodedFormData> formData)
    : FormDataBytesConsumer(executionContext, std::move(formData), nullptr) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* executionContext,
    PassRefPtr<EncodedFormData> formData,
    BytesConsumer* consumer)
    : m_impl(isSimple(formData.get())
                 ? static_cast<BytesConsumer*>(
                       new SimpleFormDataBytesConsumer(std::move(formData)))
                 : static_cast<BytesConsumer*>(
                       new ComplexFormDataBytesConsumer(executionContext,
                                                        std::move(formData),
                                                        consumer))) {}

}  // namespace blink
