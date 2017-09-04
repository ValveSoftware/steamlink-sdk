// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BlobBytesConsumer.h"

#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

BlobBytesConsumer::BlobBytesConsumer(ExecutionContext* executionContext,
                                     PassRefPtr<BlobDataHandle> blobDataHandle,
                                     ThreadableLoader* loader)
    : ContextLifecycleObserver(executionContext),
      m_blobDataHandle(blobDataHandle),
      m_loader(loader) {
  ThreadState::current()->registerPreFinalizer(this);
  if (!m_blobDataHandle) {
    // Note that |m_loader| is non-null only in tests.
    if (m_loader) {
      m_loader->cancel();
      m_loader = nullptr;
    }
    m_state = PublicState::Closed;
  }
}

BlobBytesConsumer::BlobBytesConsumer(ExecutionContext* executionContext,
                                     PassRefPtr<BlobDataHandle> blobDataHandle)
    : BlobBytesConsumer(executionContext, std::move(blobDataHandle), nullptr) {}

BlobBytesConsumer::~BlobBytesConsumer() {}

BytesConsumer::Result BlobBytesConsumer::beginRead(const char** buffer,
                                                   size_t* available) {
  *buffer = nullptr;
  *available = 0;

  if (m_state == PublicState::Closed) {
    // It's possible that |cancel| has been called before the first
    // |beginRead| call. That's why we need to check this condition
    // before checking |isClean()|.
    return Result::Done;
  }

  if (isClean()) {
    KURL m_blobURL =
        BlobURL::createPublicURL(getExecutionContext()->getSecurityOrigin());
    if (m_blobURL.isEmpty()) {
      error();
    } else {
      BlobRegistry::registerPublicBlobURL(
          getExecutionContext()->getSecurityOrigin(), m_blobURL,
          m_blobDataHandle);

      // m_loader is non-null only in tests.
      if (!m_loader)
        m_loader = createLoader();

      ResourceRequest request(m_blobURL);
      request.setRequestContext(WebURLRequest::RequestContextInternal);
      request.setUseStreamOnResponse(true);
      // We intentionally skip
      // 'setExternalRequestStateFromRequestorAddressSpace', as 'blob:'
      // can never be external.
      m_loader->start(request);
    }
    m_blobDataHandle = nullptr;
  }
  DCHECK_NE(m_state, PublicState::Closed);

  if (m_state == PublicState::Errored)
    return Result::Error;

  if (!m_body) {
    // The response has not arrived.
    return Result::ShouldWait;
  }

  auto result = m_body->beginRead(buffer, available);
  switch (result) {
    case Result::Ok:
    case Result::ShouldWait:
      break;
    case Result::Done:
      m_hasSeenEndOfData = true;
      if (m_hasFinishedLoading)
        close();
      return m_state == PublicState::Closed ? Result::Done : Result::ShouldWait;
    case Result::Error:
      error();
      break;
  }
  return result;
}

BytesConsumer::Result BlobBytesConsumer::endRead(size_t read) {
  DCHECK(m_body);
  return m_body->endRead(read);
}

PassRefPtr<BlobDataHandle> BlobBytesConsumer::drainAsBlobDataHandle(
    BlobSizePolicy policy) {
  if (!isClean())
    return nullptr;
  DCHECK(m_blobDataHandle);
  if (policy == BlobSizePolicy::DisallowBlobWithInvalidSize &&
      m_blobDataHandle->size() == UINT64_MAX)
    return nullptr;
  close();
  return m_blobDataHandle.release();
}

PassRefPtr<EncodedFormData> BlobBytesConsumer::drainAsFormData() {
  RefPtr<BlobDataHandle> handle =
      drainAsBlobDataHandle(BlobSizePolicy::AllowBlobWithInvalidSize);
  if (!handle)
    return nullptr;
  RefPtr<EncodedFormData> formData = EncodedFormData::create();
  formData->appendBlob(handle->uuid(), handle);
  return formData.release();
}

void BlobBytesConsumer::setClient(BytesConsumer::Client* client) {
  DCHECK(!m_client);
  DCHECK(client);
  m_client = client;
}

void BlobBytesConsumer::clearClient() {
  m_client = nullptr;
}

void BlobBytesConsumer::cancel() {
  if (m_state == PublicState::Closed || m_state == PublicState::Errored)
    return;
  close();
  if (m_body) {
    m_body->cancel();
    m_body = nullptr;
  }
  if (!m_blobURL.isEmpty()) {
    BlobRegistry::revokePublicBlobURL(m_blobURL);
    m_blobURL = KURL();
  }
  m_blobDataHandle = nullptr;
}

BytesConsumer::Error BlobBytesConsumer::getError() const {
  DCHECK_EQ(PublicState::Errored, m_state);
  return Error("Failed to load a blob.");
}

BytesConsumer::PublicState BlobBytesConsumer::getPublicState() const {
  return m_state;
}

void BlobBytesConsumer::contextDestroyed() {
  if (m_state != PublicState::ReadableOrWaiting)
    return;

  BytesConsumer::Client* client = m_client;
  error();
  if (client)
    client->onStateChange();
}

void BlobBytesConsumer::onStateChange() {
  if (m_state != PublicState::ReadableOrWaiting)
    return;
  DCHECK(m_body);

  BytesConsumer::Client* client = m_client;
  switch (m_body->getPublicState()) {
    case PublicState::ReadableOrWaiting:
      break;
    case PublicState::Closed:
      m_hasSeenEndOfData = true;
      if (m_hasFinishedLoading)
        close();
      break;
    case PublicState::Errored:
      error();
      break;
  }
  if (client)
    client->onStateChange();
}

void BlobBytesConsumer::didReceiveResponse(
    unsigned long identifier,
    const ResourceResponse&,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(handle);
  DCHECK(!m_body);
  DCHECK_EQ(PublicState::ReadableOrWaiting, m_state);

  m_body = new BytesConsumerForDataConsumerHandle(getExecutionContext(),
                                                  std::move(handle));
  m_body->setClient(this);

  if (isClean()) {
    // This function is called synchronously in ThreadableLoader::start.
    return;
  }
  onStateChange();
}

void BlobBytesConsumer::didFinishLoading(unsigned long identifier,
                                         double finishTime) {
  DCHECK_EQ(PublicState::ReadableOrWaiting, m_state);
  m_hasFinishedLoading = true;
  m_loader = nullptr;
  if (!m_hasSeenEndOfData)
    return;
  DCHECK(!isClean());
  BytesConsumer::Client* client = m_client;
  close();
  if (client)
    client->onStateChange();
}

void BlobBytesConsumer::didFail(const ResourceError& e) {
  if (e.isCancellation()) {
    DCHECK_EQ(PublicState::Closed, m_state);
    return;
  }
  DCHECK_EQ(PublicState::ReadableOrWaiting, m_state);
  m_loader = nullptr;
  BytesConsumer::Client* client = m_client;
  error();
  if (isClean()) {
    // This function is called synchronously in ThreadableLoader::start.
    return;
  }
  if (client) {
    client->onStateChange();
    client = nullptr;
  }
}

void BlobBytesConsumer::didFailRedirectCheck() {
  NOTREACHED();
}

DEFINE_TRACE(BlobBytesConsumer) {
  visitor->trace(m_body);
  visitor->trace(m_client);
  visitor->trace(m_loader);
  BytesConsumer::trace(visitor);
  BytesConsumer::Client::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

BlobBytesConsumer* BlobBytesConsumer::createForTesting(
    ExecutionContext* executionContext,
    PassRefPtr<BlobDataHandle> blobDataHandle,
    ThreadableLoader* loader) {
  return new BlobBytesConsumer(executionContext, std::move(blobDataHandle),
                               loader);
}

ThreadableLoader* BlobBytesConsumer::createLoader() {
  ThreadableLoaderOptions options;
  options.preflightPolicy = ConsiderPreflight;
  options.crossOriginRequestPolicy = DenyCrossOriginRequests;
  options.contentSecurityPolicyEnforcement = DoNotEnforceContentSecurityPolicy;
  options.initiator = FetchInitiatorTypeNames::internal;

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;

  return ThreadableLoader::create(*getExecutionContext(), this, options,
                                  resourceLoaderOptions);
}

void BlobBytesConsumer::close() {
  DCHECK_EQ(m_state, PublicState::ReadableOrWaiting);
  m_state = PublicState::Closed;
  clear();
}

void BlobBytesConsumer::error() {
  DCHECK_EQ(m_state, PublicState::ReadableOrWaiting);
  m_state = PublicState::Errored;
  clear();
}

void BlobBytesConsumer::clear() {
  DCHECK_NE(m_state, PublicState::ReadableOrWaiting);
  if (m_loader) {
    m_loader->cancel();
    m_loader = nullptr;
  }
  m_client = nullptr;
}

}  // namespace blink
