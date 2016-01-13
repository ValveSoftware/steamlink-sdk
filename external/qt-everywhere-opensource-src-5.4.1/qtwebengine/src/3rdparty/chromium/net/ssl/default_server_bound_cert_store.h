// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_DEFAULT_SERVER_BOUND_CERT_STORE_H_
#define NET_SSL_DEFAULT_SERVER_BOUND_CERT_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/ssl/server_bound_cert_store.h"

namespace net {

// This class is the system for storing and retrieving server bound certs.
// Modeled after the CookieMonster class, it has an in-memory cert store,
// and synchronizes server bound certs to an optional permanent storage that
// implements the PersistentStore interface. The use case is described in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-00.html
class NET_EXPORT DefaultServerBoundCertStore : public ServerBoundCertStore {
 public:
  class PersistentStore;

  // The key for each ServerBoundCert* in ServerBoundCertMap is the
  // corresponding server.
  typedef std::map<std::string, ServerBoundCert*> ServerBoundCertMap;

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the
  // DefaultServerBoundCertStore's existence. If |store| is NULL, then no
  // backing store will be updated.
  explicit DefaultServerBoundCertStore(PersistentStore* store);

  virtual ~DefaultServerBoundCertStore();

  // ServerBoundCertStore implementation.
  virtual int GetServerBoundCert(
      const std::string& server_identifier,
      base::Time* expiration_time,
      std::string* private_key_result,
      std::string* cert_result,
      const GetCertCallback& callback) OVERRIDE;
  virtual void SetServerBoundCert(
      const std::string& server_identifier,
      base::Time creation_time,
      base::Time expiration_time,
      const std::string& private_key,
      const std::string& cert) OVERRIDE;
  virtual void DeleteServerBoundCert(
      const std::string& server_identifier,
      const base::Closure& callback) OVERRIDE;
  virtual void DeleteAllCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& callback) OVERRIDE;
  virtual void DeleteAll(const base::Closure& callback) OVERRIDE;
  virtual void GetAllServerBoundCerts(
      const GetCertListCallback& callback) OVERRIDE;
  virtual int GetCertCount() OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;

 private:
  class Task;
  class GetServerBoundCertTask;
  class SetServerBoundCertTask;
  class DeleteServerBoundCertTask;
  class DeleteAllCreatedBetweenTask;
  class GetAllServerBoundCertsTask;

  static const size_t kMaxCerts;

  // Deletes all of the certs. Does not delete them from |store_|.
  void DeleteAllInMemory();

  // Called by all non-static functions to ensure that the cert store has
  // been initialized.
  // TODO(mattm): since we load asynchronously now, maybe we should start
  // loading immediately on construction, or provide some method to initiate
  // loading?
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_.get()) {
        InitStore();
      } else {
        loaded_ = true;
      }
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing certs from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  // Callback for backing store loading completion.
  void OnLoaded(scoped_ptr<ScopedVector<ServerBoundCert> > certs);

  // Syncronous methods which do the actual work. Can only be called after
  // initialization is complete.
  void SyncSetServerBoundCert(
      const std::string& server_identifier,
      base::Time creation_time,
      base::Time expiration_time,
      const std::string& private_key,
      const std::string& cert);
  void SyncDeleteServerBoundCert(const std::string& server_identifier);
  void SyncDeleteAllCreatedBetween(base::Time delete_begin,
                                   base::Time delete_end);
  void SyncGetAllServerBoundCerts(ServerBoundCertList* cert_list);

  // Add |task| to |waiting_tasks_|.
  void EnqueueTask(scoped_ptr<Task> task);
  // If already initialized, run |task| immediately. Otherwise add it to
  // |waiting_tasks_|.
  void RunOrEnqueueTask(scoped_ptr<Task> task);

  // Deletes the cert for the specified server, if such a cert exists, from the
  // in-memory store. Deletes it from |store_| if |store_| is not NULL.
  void InternalDeleteServerBoundCert(const std::string& server);

  // Takes ownership of *cert.
  // Adds the cert for the specified server to the in-memory store. Deletes it
  // from |store_| if |store_| is not NULL.
  void InternalInsertServerBoundCert(const std::string& server_identifier,
                                     ServerBoundCert* cert);

  // Indicates whether the cert store has been initialized. This happens
  // lazily in InitIfNecessary().
  bool initialized_;

  // Indicates whether loading from the backend store is completed and
  // calls may be immediately processed.
  bool loaded_;

  // Tasks that are waiting to be run once we finish loading.
  ScopedVector<Task> waiting_tasks_;
  base::TimeTicks waiting_tasks_start_time_;

  scoped_refptr<PersistentStore> store_;

  ServerBoundCertMap server_bound_certs_;

  base::WeakPtrFactory<DefaultServerBoundCertStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultServerBoundCertStore);
};

typedef base::RefCountedThreadSafe<DefaultServerBoundCertStore::PersistentStore>
    RefcountedPersistentStore;

class NET_EXPORT DefaultServerBoundCertStore::PersistentStore
    : public RefcountedPersistentStore {
 public:
  typedef base::Callback<void(scoped_ptr<ScopedVector<ServerBoundCert> >)>
      LoadedCallback;

  // Initializes the store and retrieves the existing certs. This will be
  // called only once at startup. Note that the certs are individually allocated
  // and that ownership is transferred to the caller upon return.
  // The |loaded_callback| must not be called synchronously.
  virtual void Load(const LoadedCallback& loaded_callback) = 0;

  virtual void AddServerBoundCert(const ServerBoundCert& cert) = 0;

  virtual void DeleteServerBoundCert(const ServerBoundCert& cert) = 0;

  // When invoked, instructs the store to keep session related data on
  // destruction.
  virtual void SetForceKeepSessionState() = 0;

 protected:
  friend class base::RefCountedThreadSafe<PersistentStore>;

  PersistentStore();
  virtual ~PersistentStore();

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentStore);
};

}  // namespace net

#endif  // NET_SSL_DEFAULT_SERVER_BOUND_CERT_STORE_H_
