// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"

namespace content {

class DOMStorageContextWrapper;
class DOMStorageSession;

class SessionStorageNamespaceImpl
    : NON_EXPORTED_BASE(public SessionStorageNamespace) {
 public:
  // Constructs a |SessionStorageNamespaceImpl| and allocates new IDs for it.
  //
  // The CONTENT_EXPORT allows TestRenderViewHost to instantiate these.
  CONTENT_EXPORT explicit SessionStorageNamespaceImpl(
      DOMStorageContextWrapper* context);

  // Constructs a |SessionStorageNamespaceImpl| by cloning
  // |namespace_to_clone|. Allocates new IDs for it.
  SessionStorageNamespaceImpl(DOMStorageContextWrapper* context,
                              int64 namepace_id_to_clone);

  // Constructs a |SessionStorageNamespaceImpl| and assigns |persistent_id|
  // to it. Allocates a new non-persistent ID.
  SessionStorageNamespaceImpl(DOMStorageContextWrapper* context,
                              const std::string& persistent_id);

  // Creates an alias of |master_session_storage_namespace|. This will allocate
  // a new non-persistent ID.
  explicit SessionStorageNamespaceImpl(
      SessionStorageNamespaceImpl* master_session_storage_namespace);

  // SessionStorageNamespace implementation.
  virtual int64 id() const OVERRIDE;
  virtual const std::string& persistent_id() const OVERRIDE;
  virtual void SetShouldPersist(bool should_persist) OVERRIDE;
  virtual bool should_persist() const OVERRIDE;

  SessionStorageNamespaceImpl* Clone();
  bool IsFromContext(DOMStorageContextWrapper* context);

  virtual void AddTransactionLogProcessId(int process_id) OVERRIDE;
  virtual void RemoveTransactionLogProcessId(int process_id) OVERRIDE;
  virtual void Merge(bool actually_merge,
                     int process_id,
                     SessionStorageNamespace* other,
                     const MergeResultCallback& callback) OVERRIDE;
  virtual bool IsAliasOf(SessionStorageNamespace* other) OVERRIDE;
  virtual SessionStorageNamespace* CreateAlias() OVERRIDE;

 private:
  explicit SessionStorageNamespaceImpl(DOMStorageSession* clone);
  virtual ~SessionStorageNamespaceImpl();

  scoped_refptr<DOMStorageSession> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
