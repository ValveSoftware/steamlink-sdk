// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_

#include <memory>
#include <set>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
class DictionaryValue;
}

namespace content {

class StoragePartition;
class ServiceWorkerContextWrapper;
class ServiceWorkerVersion;

class ServiceWorkerInternalsUI
    : public WebUIController,
      public base::SupportsWeakPtr<ServiceWorkerInternalsUI> {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  typedef void (ServiceWorkerVersion::*ServiceWorkerVersionMethod)(
      const StatusCallback& callback);

  explicit ServiceWorkerInternalsUI(WebUI* web_ui);

 private:
  class OperationProxy;
  class PartitionObserver;

  ~ServiceWorkerInternalsUI() override;
  void AddContextFromStoragePartition(StoragePartition* partition);

  void RemoveObserverFromStoragePartition(StoragePartition* partition);

  // Called from Javascript.
  void GetOptions(const base::ListValue* args);
  void SetOption(const base::ListValue* args);
  void GetAllRegistrations(const base::ListValue* args);
  void CallServiceWorkerVersionMethod(ServiceWorkerVersionMethod method,
                                      const base::ListValue* args);
  void InspectWorker(const base::ListValue* args);
  void Unregister(const base::ListValue* args);
  void StartWorker(const base::ListValue* args);

  bool GetServiceWorkerContext(
      int partition_id,
      scoped_refptr<ServiceWorkerContextWrapper>* context) const;
  void FindContext(int partition_id,
                   StoragePartition** result_partition,
                   StoragePartition* storage_partition) const;

  void UnregisterWithScope(scoped_refptr<ServiceWorkerContextWrapper> context,
                           const GURL& scope,
                           const StatusCallback& callback) const;

  base::ScopedPtrHashMap<uintptr_t, std::unique_ptr<PartitionObserver>>
      observers_;
  int next_partition_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
