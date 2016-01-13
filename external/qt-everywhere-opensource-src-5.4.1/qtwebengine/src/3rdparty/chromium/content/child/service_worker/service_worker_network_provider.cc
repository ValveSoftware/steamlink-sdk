// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

namespace {

const char kUserDataKey[] = "SWProviderKey";

// Must be unique in the child process.
int GetNextProviderId() {
  static base::StaticAtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

}  // namespace

void ServiceWorkerNetworkProvider::AttachToDocumentState(
    base::SupportsUserData* datasource_userdata,
    scoped_ptr<ServiceWorkerNetworkProvider> network_provider) {
  datasource_userdata->SetUserData(&kUserDataKey, network_provider.release());
}

ServiceWorkerNetworkProvider* ServiceWorkerNetworkProvider::FromDocumentState(
    base::SupportsUserData* datasource_userdata) {
  return static_cast<ServiceWorkerNetworkProvider*>(
      datasource_userdata->GetUserData(&kUserDataKey));
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider()
    : provider_id_(GetNextProviderId()),
      context_(new ServiceWorkerProviderContext(provider_id_)) {
  if (!ChildThread::current())
    return;  // May be null in some tests.
  ChildThread::current()->Send(
      new ServiceWorkerHostMsg_ProviderCreated(provider_id_));
}

ServiceWorkerNetworkProvider::~ServiceWorkerNetworkProvider() {
  if (!ChildThread::current())
    return;  // May be null in some tests.
  ChildThread::current()->Send(
      new ServiceWorkerHostMsg_ProviderDestroyed(provider_id_));
}

void ServiceWorkerNetworkProvider::SetServiceWorkerVersionId(
    int64 version_id) {
  if (!ChildThread::current())
    return;  // May be null in some tests.
  ChildThread::current()->Send(
      new ServiceWorkerHostMsg_SetVersionId(provider_id_, version_id));
}

}  // namespace content
