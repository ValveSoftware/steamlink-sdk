// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/dom_storage_context.h"
#include "url/origin.h"

namespace base {
class FilePath;
}

namespace shell {
class Connector;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class DOMStorageContextImpl;
class LevelDBWrapperImpl;

// This is owned by Storage Partition and encapsulates all its dom storage
// state.
class CONTENT_EXPORT DOMStorageContextWrapper :
    NON_EXPORTED_BASE(public DOMStorageContext),
    public base::RefCountedThreadSafe<DOMStorageContextWrapper> {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextWrapper(
      shell::Connector* connector,
      const base::FilePath& data_path,
      const base::FilePath& local_partition_path,
      storage::SpecialStoragePolicy* special_storage_policy);

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(
      const GetLocalStorageUsageCallback& callback) override;
  void GetSessionStorageUsage(
      const GetSessionStorageUsageCallback& callback) override;
  void DeleteLocalStorage(const GURL& origin) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info) override;
  void SetSaveSessionStorageOnDisk() override;
  scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& persistent_id) override;
  void StartScavengingUnusedSessionStorage() override;

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

  void Flush();

  // See mojom::StoragePartitionService interface.
  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBObserverPtr observer,
                        mojom::LevelDBWrapperRequest request);

  // Called on UI thread when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;

  ~DOMStorageContextWrapper() override;
  DOMStorageContextImpl* context() const { return context_.get(); }

  // An inner class to keep all mojo-ish details together and not bleed them
  // through the public interface.
  class MojoState;
  std::unique_ptr<MojoState> mojo_state_;

  // To receive memory pressure signals.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  scoped_refptr<DOMStorageContextImpl> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
