// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_sync/background_sync.mojom.h"

namespace content {

class BackgroundSyncManager;
class BackgroundSyncServiceImpl;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. Most logic is delegated to the owned BackgroundSyncManager
// instance, which is only accessed on the IO thread.
class CONTENT_EXPORT BackgroundSyncContext
    : public base::RefCountedThreadSafe<BackgroundSyncContext> {
 public:
  BackgroundSyncContext();

  // Init and Shutdown are for use on the UI thread when the
  // StoragePartition is being setup and torn down.
  void Init(const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Create a BackgroundSyncServiceImpl that is owned by this. Call on the UI
  // thread.
  void CreateService(
      mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request);

  // Called by BackgroundSyncServiceImpl objects so that they can
  // be deleted. Call on the IO thread.
  void ServiceHadConnectionError(BackgroundSyncServiceImpl* service);

  // Call on the IO thread.
  BackgroundSyncManager* background_sync_manager() const;

 protected:
  friend class base::RefCountedThreadSafe<BackgroundSyncContext>;
  virtual ~BackgroundSyncContext();

  void set_background_sync_manager_for_testing(
      std::unique_ptr<BackgroundSyncManager> manager);

 private:
  friend class BackgroundSyncServiceImplTest;

  virtual void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> context);

  void CreateServiceOnIOThread(
      mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request);

  void ShutdownOnIO();

  // Only accessed on the IO thread.
  std::unique_ptr<BackgroundSyncManager> background_sync_manager_;

  // The services are owned by this. They're either deleted
  // during ShutdownOnIO or when the channel is closed via
  // ServiceHadConnectionError. Only accessed on the IO thread.
  std::set<BackgroundSyncServiceImpl*> services_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_H_
