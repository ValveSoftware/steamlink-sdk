// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/ServiceWorkerRegistrationSync.h"

#include "modules/background_sync/SyncManager.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

ServiceWorkerRegistrationSync::ServiceWorkerRegistrationSync(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {}

ServiceWorkerRegistrationSync::~ServiceWorkerRegistrationSync() {}

const char* ServiceWorkerRegistrationSync::supplementName() {
  return "ServiceWorkerRegistrationSync";
}

ServiceWorkerRegistrationSync& ServiceWorkerRegistrationSync::from(
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationSync* supplement =
      static_cast<ServiceWorkerRegistrationSync*>(
          Supplement<ServiceWorkerRegistration>::from(registration,
                                                      supplementName()));
  if (!supplement) {
    supplement = new ServiceWorkerRegistrationSync(&registration);
    provideTo(registration, supplementName(), supplement);
  }
  return *supplement;
}

SyncManager* ServiceWorkerRegistrationSync::sync(
    ServiceWorkerRegistration& registration) {
  return ServiceWorkerRegistrationSync::from(registration).sync();
}

SyncManager* ServiceWorkerRegistrationSync::sync() {
  if (!m_syncManager)
    m_syncManager = SyncManager::create(m_registration);
  return m_syncManager.get();
}

DEFINE_TRACE(ServiceWorkerRegistrationSync) {
  visitor->trace(m_registration);
  visitor->trace(m_syncManager);
  Supplement<ServiceWorkerRegistration>::trace(visitor);
}

}  // namespace blink
