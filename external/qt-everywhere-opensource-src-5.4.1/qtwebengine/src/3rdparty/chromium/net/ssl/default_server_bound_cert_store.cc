// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_server_bound_cert_store.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "net/base/net_errors.h"

namespace net {

// --------------------------------------------------------------------------
// Task
class DefaultServerBoundCertStore::Task {
 public:
  virtual ~Task();

  // Runs the task and invokes the client callback on the thread that
  // originally constructed the task.
  virtual void Run(DefaultServerBoundCertStore* store) = 0;

 protected:
  void InvokeCallback(base::Closure callback) const;
};

DefaultServerBoundCertStore::Task::~Task() {
}

void DefaultServerBoundCertStore::Task::InvokeCallback(
    base::Closure callback) const {
  if (!callback.is_null())
    callback.Run();
}

// --------------------------------------------------------------------------
// GetServerBoundCertTask
class DefaultServerBoundCertStore::GetServerBoundCertTask
    : public DefaultServerBoundCertStore::Task {
 public:
  GetServerBoundCertTask(const std::string& server_identifier,
                         const GetCertCallback& callback);
  virtual ~GetServerBoundCertTask();
  virtual void Run(DefaultServerBoundCertStore* store) OVERRIDE;

 private:
  std::string server_identifier_;
  GetCertCallback callback_;
};

DefaultServerBoundCertStore::GetServerBoundCertTask::GetServerBoundCertTask(
    const std::string& server_identifier,
    const GetCertCallback& callback)
    : server_identifier_(server_identifier),
      callback_(callback) {
}

DefaultServerBoundCertStore::GetServerBoundCertTask::~GetServerBoundCertTask() {
}

void DefaultServerBoundCertStore::GetServerBoundCertTask::Run(
    DefaultServerBoundCertStore* store) {
  base::Time expiration_time;
  std::string private_key_result;
  std::string cert_result;
  int err = store->GetServerBoundCert(
      server_identifier_, &expiration_time, &private_key_result,
      &cert_result, GetCertCallback());
  DCHECK(err != ERR_IO_PENDING);

  InvokeCallback(base::Bind(callback_, err, server_identifier_,
                            expiration_time, private_key_result, cert_result));
}

// --------------------------------------------------------------------------
// SetServerBoundCertTask
class DefaultServerBoundCertStore::SetServerBoundCertTask
    : public DefaultServerBoundCertStore::Task {
 public:
  SetServerBoundCertTask(const std::string& server_identifier,
                         base::Time creation_time,
                         base::Time expiration_time,
                         const std::string& private_key,
                         const std::string& cert);
  virtual ~SetServerBoundCertTask();
  virtual void Run(DefaultServerBoundCertStore* store) OVERRIDE;

 private:
  std::string server_identifier_;
  base::Time creation_time_;
  base::Time expiration_time_;
  std::string private_key_;
  std::string cert_;
};

DefaultServerBoundCertStore::SetServerBoundCertTask::SetServerBoundCertTask(
    const std::string& server_identifier,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert)
    : server_identifier_(server_identifier),
      creation_time_(creation_time),
      expiration_time_(expiration_time),
      private_key_(private_key),
      cert_(cert) {
}

DefaultServerBoundCertStore::SetServerBoundCertTask::~SetServerBoundCertTask() {
}

void DefaultServerBoundCertStore::SetServerBoundCertTask::Run(
    DefaultServerBoundCertStore* store) {
  store->SyncSetServerBoundCert(server_identifier_, creation_time_,
                                expiration_time_, private_key_, cert_);
}

// --------------------------------------------------------------------------
// DeleteServerBoundCertTask
class DefaultServerBoundCertStore::DeleteServerBoundCertTask
    : public DefaultServerBoundCertStore::Task {
 public:
  DeleteServerBoundCertTask(const std::string& server_identifier,
                            const base::Closure& callback);
  virtual ~DeleteServerBoundCertTask();
  virtual void Run(DefaultServerBoundCertStore* store) OVERRIDE;

 private:
  std::string server_identifier_;
  base::Closure callback_;
};

DefaultServerBoundCertStore::DeleteServerBoundCertTask::
    DeleteServerBoundCertTask(
        const std::string& server_identifier,
        const base::Closure& callback)
        : server_identifier_(server_identifier),
          callback_(callback) {
}

DefaultServerBoundCertStore::DeleteServerBoundCertTask::
    ~DeleteServerBoundCertTask() {
}

void DefaultServerBoundCertStore::DeleteServerBoundCertTask::Run(
    DefaultServerBoundCertStore* store) {
  store->SyncDeleteServerBoundCert(server_identifier_);

  InvokeCallback(callback_);
}

// --------------------------------------------------------------------------
// DeleteAllCreatedBetweenTask
class DefaultServerBoundCertStore::DeleteAllCreatedBetweenTask
    : public DefaultServerBoundCertStore::Task {
 public:
  DeleteAllCreatedBetweenTask(base::Time delete_begin,
                              base::Time delete_end,
                              const base::Closure& callback);
  virtual ~DeleteAllCreatedBetweenTask();
  virtual void Run(DefaultServerBoundCertStore* store) OVERRIDE;

 private:
  base::Time delete_begin_;
  base::Time delete_end_;
  base::Closure callback_;
};

DefaultServerBoundCertStore::DeleteAllCreatedBetweenTask::
    DeleteAllCreatedBetweenTask(
        base::Time delete_begin,
        base::Time delete_end,
        const base::Closure& callback)
        : delete_begin_(delete_begin),
          delete_end_(delete_end),
          callback_(callback) {
}

DefaultServerBoundCertStore::DeleteAllCreatedBetweenTask::
    ~DeleteAllCreatedBetweenTask() {
}

void DefaultServerBoundCertStore::DeleteAllCreatedBetweenTask::Run(
    DefaultServerBoundCertStore* store) {
  store->SyncDeleteAllCreatedBetween(delete_begin_, delete_end_);

  InvokeCallback(callback_);
}

// --------------------------------------------------------------------------
// GetAllServerBoundCertsTask
class DefaultServerBoundCertStore::GetAllServerBoundCertsTask
    : public DefaultServerBoundCertStore::Task {
 public:
  explicit GetAllServerBoundCertsTask(const GetCertListCallback& callback);
  virtual ~GetAllServerBoundCertsTask();
  virtual void Run(DefaultServerBoundCertStore* store) OVERRIDE;

 private:
  std::string server_identifier_;
  GetCertListCallback callback_;
};

DefaultServerBoundCertStore::GetAllServerBoundCertsTask::
    GetAllServerBoundCertsTask(const GetCertListCallback& callback)
        : callback_(callback) {
}

DefaultServerBoundCertStore::GetAllServerBoundCertsTask::
    ~GetAllServerBoundCertsTask() {
}

void DefaultServerBoundCertStore::GetAllServerBoundCertsTask::Run(
    DefaultServerBoundCertStore* store) {
  ServerBoundCertList cert_list;
  store->SyncGetAllServerBoundCerts(&cert_list);

  InvokeCallback(base::Bind(callback_, cert_list));
}

// --------------------------------------------------------------------------
// DefaultServerBoundCertStore

// static
const size_t DefaultServerBoundCertStore::kMaxCerts = 3300;

DefaultServerBoundCertStore::DefaultServerBoundCertStore(
    PersistentStore* store)
    : initialized_(false),
      loaded_(false),
      store_(store),
      weak_ptr_factory_(this) {}

int DefaultServerBoundCertStore::GetServerBoundCert(
    const std::string& server_identifier,
    base::Time* expiration_time,
    std::string* private_key_result,
    std::string* cert_result,
    const GetCertCallback& callback) {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (!loaded_) {
    EnqueueTask(scoped_ptr<Task>(
        new GetServerBoundCertTask(server_identifier, callback)));
    return ERR_IO_PENDING;
  }

  ServerBoundCertMap::iterator it = server_bound_certs_.find(server_identifier);

  if (it == server_bound_certs_.end())
    return ERR_FILE_NOT_FOUND;

  ServerBoundCert* cert = it->second;
  *expiration_time = cert->expiration_time();
  *private_key_result = cert->private_key();
  *cert_result = cert->cert();

  return OK;
}

void DefaultServerBoundCertStore::SetServerBoundCert(
    const std::string& server_identifier,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert) {
  RunOrEnqueueTask(scoped_ptr<Task>(new SetServerBoundCertTask(
      server_identifier, creation_time, expiration_time, private_key,
      cert)));
}

void DefaultServerBoundCertStore::DeleteServerBoundCert(
    const std::string& server_identifier,
    const base::Closure& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(
      new DeleteServerBoundCertTask(server_identifier, callback)));
}

void DefaultServerBoundCertStore::DeleteAllCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(
      new DeleteAllCreatedBetweenTask(delete_begin, delete_end, callback)));
}

void DefaultServerBoundCertStore::DeleteAll(
    const base::Closure& callback) {
  DeleteAllCreatedBetween(base::Time(), base::Time(), callback);
}

void DefaultServerBoundCertStore::GetAllServerBoundCerts(
    const GetCertListCallback& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(new GetAllServerBoundCertsTask(callback)));
}

int DefaultServerBoundCertStore::GetCertCount() {
  DCHECK(CalledOnValidThread());

  return server_bound_certs_.size();
}

void DefaultServerBoundCertStore::SetForceKeepSessionState() {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (store_.get())
    store_->SetForceKeepSessionState();
}

DefaultServerBoundCertStore::~DefaultServerBoundCertStore() {
  DeleteAllInMemory();
}

void DefaultServerBoundCertStore::DeleteAllInMemory() {
  DCHECK(CalledOnValidThread());

  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end(); ++it) {
    delete it->second;
  }
  server_bound_certs_.clear();
}

void DefaultServerBoundCertStore::InitStore() {
  DCHECK(CalledOnValidThread());
  DCHECK(store_.get()) << "Store must exist to initialize";
  DCHECK(!loaded_);

  store_->Load(base::Bind(&DefaultServerBoundCertStore::OnLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DefaultServerBoundCertStore::OnLoaded(
    scoped_ptr<ScopedVector<ServerBoundCert> > certs) {
  DCHECK(CalledOnValidThread());

  for (std::vector<ServerBoundCert*>::const_iterator it = certs->begin();
       it != certs->end(); ++it) {
    DCHECK(server_bound_certs_.find((*it)->server_identifier()) ==
           server_bound_certs_.end());
    server_bound_certs_[(*it)->server_identifier()] = *it;
  }
  certs->weak_clear();

  loaded_ = true;

  base::TimeDelta wait_time;
  if (!waiting_tasks_.empty())
    wait_time = base::TimeTicks::Now() - waiting_tasks_start_time_;
  DVLOG(1) << "Task delay " << wait_time.InMilliseconds();
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.TaskMaxWaitTime",
                             wait_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1),
                             50);
  UMA_HISTOGRAM_COUNTS_100("DomainBoundCerts.TaskWaitCount",
                           waiting_tasks_.size());


  for (ScopedVector<Task>::iterator i = waiting_tasks_.begin();
       i != waiting_tasks_.end(); ++i)
    (*i)->Run(this);
  waiting_tasks_.clear();
}

void DefaultServerBoundCertStore::SyncSetServerBoundCert(
    const std::string& server_identifier,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  InternalDeleteServerBoundCert(server_identifier);
  InternalInsertServerBoundCert(
      server_identifier,
      new ServerBoundCert(
          server_identifier, creation_time, expiration_time, private_key,
          cert));
}

void DefaultServerBoundCertStore::SyncDeleteServerBoundCert(
    const std::string& server_identifier) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  InternalDeleteServerBoundCert(server_identifier);
}

void DefaultServerBoundCertStore::SyncDeleteAllCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end();) {
    ServerBoundCertMap::iterator cur = it;
    ++it;
    ServerBoundCert* cert = cur->second;
    if ((delete_begin.is_null() || cert->creation_time() >= delete_begin) &&
        (delete_end.is_null() || cert->creation_time() < delete_end)) {
      if (store_.get())
        store_->DeleteServerBoundCert(*cert);
      delete cert;
      server_bound_certs_.erase(cur);
    }
  }
}

void DefaultServerBoundCertStore::SyncGetAllServerBoundCerts(
    ServerBoundCertList* cert_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end(); ++it)
    cert_list->push_back(*it->second);
}

void DefaultServerBoundCertStore::EnqueueTask(scoped_ptr<Task> task) {
  DCHECK(CalledOnValidThread());
  DCHECK(!loaded_);
  if (waiting_tasks_.empty())
    waiting_tasks_start_time_ = base::TimeTicks::Now();
  waiting_tasks_.push_back(task.release());
}

void DefaultServerBoundCertStore::RunOrEnqueueTask(scoped_ptr<Task> task) {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (!loaded_) {
    EnqueueTask(task.Pass());
    return;
  }

  task->Run(this);
}

void DefaultServerBoundCertStore::InternalDeleteServerBoundCert(
    const std::string& server_identifier) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  ServerBoundCertMap::iterator it = server_bound_certs_.find(server_identifier);
  if (it == server_bound_certs_.end())
    return;  // There is nothing to delete.

  ServerBoundCert* cert = it->second;
  if (store_.get())
    store_->DeleteServerBoundCert(*cert);
  server_bound_certs_.erase(it);
  delete cert;
}

void DefaultServerBoundCertStore::InternalInsertServerBoundCert(
    const std::string& server_identifier,
    ServerBoundCert* cert) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  if (store_.get())
    store_->AddServerBoundCert(*cert);
  server_bound_certs_[server_identifier] = cert;
}

DefaultServerBoundCertStore::PersistentStore::PersistentStore() {}

DefaultServerBoundCertStore::PersistentStore::~PersistentStore() {}

}  // namespace net
