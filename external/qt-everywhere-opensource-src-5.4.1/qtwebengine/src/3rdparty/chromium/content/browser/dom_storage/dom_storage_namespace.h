// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"
#include "url/gurl.h"

namespace content {

class DOMStorageArea;
class DOMStorageContextImpl;
class DOMStorageTaskRunner;
class SessionStorageDatabase;

// Container for the set of per-origin Areas.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageNamespace
    : public base::RefCountedThreadSafe<DOMStorageNamespace> {
 public:
  // Option for PurgeMemory.
  enum PurgeOption {
    // Purge unopened areas only.
    PURGE_UNOPENED,

    // Purge aggressively, i.e. discard cache even for areas that have
    // non-zero open count.
    PURGE_AGGRESSIVE,
  };

  // Constructor for a LocalStorage namespace with id of 0
  // and an optional backing directory on disk.
  DOMStorageNamespace(const base::FilePath& directory,  // may be empty
                      DOMStorageTaskRunner* task_runner);

  // Constructor for a SessionStorage namespace with a non-zero id and an
  // optional backing on disk via |session_storage_database| (may be NULL).
  DOMStorageNamespace(int64 namespace_id,
                      const std::string& persistent_namespace_id,
                      SessionStorageDatabase* session_storage_database,
                      DOMStorageTaskRunner* task_runner);

  int64 namespace_id() const { return namespace_id_; }
  const std::string& persistent_namespace_id() const {
    return persistent_namespace_id_;
  }

  // Returns the storage area for the given origin,
  // creating instance if needed. Each call to open
  // must be balanced with a call to CloseStorageArea.
  DOMStorageArea* OpenStorageArea(const GURL& origin);
  void CloseStorageArea(DOMStorageArea* area);

  // Returns the area for |origin| if it's open, otherwise NULL.
  DOMStorageArea* GetOpenStorageArea(const GURL& origin);

  // Creates a clone of |this| namespace including
  // shallow copies of all contained areas.
  // Should only be called for session storage namespaces.
  DOMStorageNamespace* Clone(int64 clone_namespace_id,
                             const std::string& clone_persistent_namespace_id);

  // Creates an alias of |this| namespace.
  // Should only be called for session storage namespaces.
  DOMStorageNamespace* CreateAlias(int64 alias_namespace_id);

  void DeleteLocalStorageOrigin(const GURL& origin);
  void DeleteSessionStorageOrigin(const GURL& origin);
  void PurgeMemory(PurgeOption purge);
  void Shutdown();

  unsigned int CountInMemoryAreas() const;

  void AddTransactionLogProcessId(int process_id);
  void RemoveTransactionLogProcessId(int process_id);
  SessionStorageNamespace::MergeResult Merge(
      bool actually_merge,
      int process_id,
      DOMStorageNamespace* other,
      DOMStorageContextImpl* context);
  DOMStorageNamespace* alias_master_namespace() {
    return alias_master_namespace_.get();
  }
  int num_aliases() const { return num_aliases_; }
  bool ready_for_deletion_pending_aliases() const {
    return ready_for_deletion_pending_aliases_; }
  void set_ready_for_deletion_pending_aliases(bool value) {
    ready_for_deletion_pending_aliases_ = value;
  }
  bool must_persist_at_shutdown() const { return must_persist_at_shutdown_; }
  void set_must_persist_at_shutdown(bool value) {
    must_persist_at_shutdown_ = value;
  }

  enum LogType {
    TRANSACTION_READ,
    TRANSACTION_WRITE,
    TRANSACTION_REMOVE,
    TRANSACTION_CLEAR
  };

  struct CONTENT_EXPORT TransactionRecord {
    LogType transaction_type;
    GURL origin;
    GURL page_url;
    base::string16 key;
    base::NullableString16 value;
    TransactionRecord();
    ~TransactionRecord();
  };

  void AddTransaction(int process_id, const TransactionRecord& transaction);
  bool IsLoggingRenderer(int process_id);
  // Decrements the count of aliases owned by the master, and returns true
  // if the new count is 0.
  bool DecrementMasterAliasCount();

 private:
  friend class base::RefCountedThreadSafe<DOMStorageNamespace>;

  // Struct to hold references to our contained areas and
  // to keep track of how many tabs have a given area open.
  struct AreaHolder {
    scoped_refptr<DOMStorageArea> area_;
    int open_count_;
    AreaHolder();
    AreaHolder(DOMStorageArea* area, int count);
    ~AreaHolder();
  };
  typedef std::map<GURL, AreaHolder> AreaMap;

  struct TransactionData {
    bool max_log_size_exceeded;
    std::vector<TransactionRecord> log;
    TransactionData();
    ~TransactionData();
  };

  ~DOMStorageNamespace();

  // Returns a pointer to the area holder in our map or NULL.
  AreaHolder* GetAreaHolder(const GURL& origin);

  // Switches the current alias DOM storage namespace to a new alias master.
  void SwitchToNewAliasMaster(DOMStorageNamespace* new_master,
                              DOMStorageContextImpl* context);

  int64 namespace_id_;
  std::string persistent_namespace_id_;
  base::FilePath directory_;
  AreaMap areas_;
  scoped_refptr<DOMStorageTaskRunner> task_runner_;
  scoped_refptr<SessionStorageDatabase> session_storage_database_;
  std::map<int, TransactionData*> transactions_;
  int num_aliases_;
  scoped_refptr<DOMStorageNamespace> alias_master_namespace_;
  DOMStorageNamespace* old_master_for_close_area_;
  // Indicates whether we have already decremented |num_aliases_| for this
  // namespace in its alias master. We may only decrement it once, and around
  // deletion, this instance will stick around a bit longer until its refcount
  // drops to 0. Therefore, we want to make sure we don't decrement the master's
  // alias count a second time.
  bool master_alias_count_decremented_;
  // This indicates, for an alias master, that the master itself is ready
  // for deletion, but there are aliases outstanding that we have to wait for
  // before we can start cleaning up the master.
  bool ready_for_deletion_pending_aliases_;
  bool must_persist_at_shutdown_;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
