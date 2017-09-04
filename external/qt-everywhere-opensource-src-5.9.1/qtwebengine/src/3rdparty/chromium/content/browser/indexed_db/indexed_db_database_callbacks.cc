// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_observer_changes.h"
#include "content/common/indexed_db/indexed_db_messages.h"

using ::indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo;

namespace content {

class IndexedDBDatabaseCallbacks::IOThreadHelper {
 public:
  explicit IOThreadHelper(DatabaseCallbacksAssociatedPtrInfo callbacks_info);
  ~IOThreadHelper();

  void SendForcedClose();
  void SendVersionChange(int64_t old_version, int64_t new_version);
  void SendAbort(int64_t transaction_id, const IndexedDBDatabaseError& error);
  void SendComplete(int64_t transaction_id);

 private:
  ::indexed_db::mojom::DatabaseCallbacksAssociatedPtr callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    int32_t ipc_thread_id,
    DatabaseCallbacksAssociatedPtrInfo callbacks_info)
    : dispatcher_host_(std::move(dispatcher_host)),
      ipc_thread_id_(ipc_thread_id),
      io_helper_(new IOThreadHelper(std::move(callbacks_info))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  thread_checker_.DetachFromThread();
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void IndexedDBDatabaseCallbacks::OnForcedClose() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dispatcher_host_)
    return;

  DCHECK(io_helper_);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&IOThreadHelper::SendForcedClose,
                                     base::Unretained(io_helper_.get())));
  dispatcher_host_ = NULL;
}

void IndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                 int64_t new_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dispatcher_host_)
    return;

  DCHECK(io_helper_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendVersionChange,
                 base::Unretained(io_helper_.get()), old_version, new_version));
}

void IndexedDBDatabaseCallbacks::OnAbort(int64_t host_transaction_id,
                                         const IndexedDBDatabaseError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dispatcher_host_)
    return;

  dispatcher_host_->FinishTransaction(host_transaction_id, false);
  DCHECK(io_helper_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendAbort, base::Unretained(io_helper_.get()),
                 dispatcher_host_->RendererTransactionId(host_transaction_id),
                 error));
}

void IndexedDBDatabaseCallbacks::OnComplete(int64_t host_transaction_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dispatcher_host_)
    return;

  dispatcher_host_->FinishTransaction(host_transaction_id, true);
  DCHECK(io_helper_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendComplete,
                 base::Unretained(io_helper_.get()),
                 dispatcher_host_->RendererTransactionId(host_transaction_id)));
}

void IndexedDBDatabaseCallbacks::OnDatabaseChange(
    std::unique_ptr<IndexedDBObserverChanges> changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(io_helper_);
  dispatcher_host_->Send(new IndexedDBMsg_DatabaseCallbacksChanges(
      ipc_thread_id_,
      IndexedDBDispatcherHost::ConvertObserverChanges(std::move(changes))));
}

IndexedDBDatabaseCallbacks::IOThreadHelper::IOThreadHelper(
    DatabaseCallbacksAssociatedPtrInfo callbacks_info) {
  callbacks_.Bind(std::move(callbacks_info));
}

IndexedDBDatabaseCallbacks::IOThreadHelper::~IOThreadHelper() {}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendForcedClose() {
  callbacks_->ForcedClose();
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendVersionChange(
    int64_t old_version,
    int64_t new_version) {
  callbacks_->VersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendAbort(
    int64_t transaction_id,
    const IndexedDBDatabaseError& error) {
  callbacks_->Abort(transaction_id, error.code(), error.message());
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendComplete(
    int64_t transaction_id) {
  callbacks_->Complete(transaction_id);
}

}  // namespace content
