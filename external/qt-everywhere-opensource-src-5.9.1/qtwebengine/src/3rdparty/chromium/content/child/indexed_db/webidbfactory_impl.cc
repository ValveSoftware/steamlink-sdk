// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbfactory_impl.h"

#include "content/child/child_thread_impl.h"
#include "content/child/indexed_db/indexed_db_callbacks_impl.h"
#include "content/child/indexed_db/indexed_db_database_callbacks_impl.h"
#include "content/child/storage_util.h"
#include "content/child/thread_safe_sender.h"
#include "content/public/child/worker_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebSecurityOrigin;
using blink::WebString;
using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo;
using indexed_db::mojom::FactoryAssociatedPtr;

namespace content {

class WebIDBFactoryImpl::IOThreadHelper {
 public:
  IOThreadHelper(scoped_refptr<IPC::SyncMessageFilter> sync_message_filter);
  ~IOThreadHelper();

  FactoryAssociatedPtr& GetService();
  CallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  DatabaseCallbacksAssociatedPtrInfo GetDatabaseCallbacksProxy(
      std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks);

  void GetDatabaseNames(std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
                        const url::Origin& origin);
  void Open(int32_t worker_thread,
            const base::string16& name,
            int64_t version,
            int64_t transaction_id,
            std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
            std::unique_ptr<IndexedDBDatabaseCallbacksImpl> database_callbacks,
            const url::Origin& origin);
  void DeleteDatabase(const base::string16& name,
                      std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
                      const url::Origin& origin);

 private:
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  FactoryAssociatedPtr service_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

WebIDBFactoryImpl::WebIDBFactoryImpl(
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
    scoped_refptr<ThreadSafeSender> thread_safe_sender,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : io_helper_(new IOThreadHelper(std::move(sync_message_filter))),
      thread_safe_sender_(std::move(thread_safe_sender)),
      io_runner_(std::move(io_runner)) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() {
  io_runner_->DeleteSoon(FROM_HERE, io_helper_);
}

void WebIDBFactoryImpl::getDatabaseNames(WebIDBCallbacks* callbacks,
                                         const WebSecurityOrigin& origin) {
  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      io_runner_, thread_safe_sender_);
  io_runner_->PostTask(FROM_HERE, base::Bind(&IOThreadHelper::GetDatabaseNames,
                                             base::Unretained(io_helper_),
                                             base::Passed(&callbacks_impl),
                                             url::Origin(origin)));
}

void WebIDBFactoryImpl::open(const WebString& name,
                             long long version,
                             long long transaction_id,
                             WebIDBCallbacks* callbacks,
                             WebIDBDatabaseCallbacks* database_callbacks,
                             const WebSecurityOrigin& origin) {
  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  auto database_callbacks_impl =
      base::MakeUnique<IndexedDBDatabaseCallbacksImpl>(
          base::WrapUnique(database_callbacks), thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::Open, base::Unretained(io_helper_),
                 WorkerThread::GetCurrentId(), base::string16(name), version,
                 transaction_id, base::Passed(&callbacks_impl),
                 base::Passed(&database_callbacks_impl), url::Origin(origin)));
}

void WebIDBFactoryImpl::deleteDatabase(const WebString& name,
                                       WebIDBCallbacks* callbacks,
                                       const WebSecurityOrigin& origin) {
  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      io_runner_, thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::DeleteDatabase, base::Unretained(io_helper_),
                 base::string16(name), base::Passed(&callbacks_impl),
                 url::Origin(origin)));
}

WebIDBFactoryImpl::IOThreadHelper::IOThreadHelper(
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter)
    : sync_message_filter_(std::move(sync_message_filter)) {}

WebIDBFactoryImpl::IOThreadHelper::~IOThreadHelper() {}

FactoryAssociatedPtr& WebIDBFactoryImpl::IOThreadHelper::GetService() {
  if (!service_)
    sync_message_filter_->GetRemoteAssociatedInterface(&service_);
  return service_;
}

CallbacksAssociatedPtrInfo WebIDBFactoryImpl::IOThreadHelper::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  CallbacksAssociatedPtrInfo ptr_info;
  indexed_db::mojom::CallbacksAssociatedRequest request;
  GetService().associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

DatabaseCallbacksAssociatedPtrInfo
WebIDBFactoryImpl::IOThreadHelper::GetDatabaseCallbacksProxy(
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks) {
  DatabaseCallbacksAssociatedPtrInfo ptr_info;
  indexed_db::mojom::DatabaseCallbacksAssociatedRequest request;
  GetService().associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

void WebIDBFactoryImpl::IOThreadHelper::GetDatabaseNames(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
    const url::Origin& origin) {
  GetService()->GetDatabaseNames(GetCallbacksProxy(std::move(callbacks)),
                                 origin);
}

void WebIDBFactoryImpl::IOThreadHelper::Open(
    int32_t worker_thread,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> database_callbacks,
    const url::Origin& origin) {
  GetService()->Open(worker_thread, GetCallbacksProxy(std::move(callbacks)),
                     GetDatabaseCallbacksProxy(std::move(database_callbacks)),
                     origin, name, version, transaction_id);
}

void WebIDBFactoryImpl::IOThreadHelper::DeleteDatabase(
    const base::string16& name,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
    const url::Origin& origin) {
  GetService()->DeleteDatabase(GetCallbacksProxy(std::move(callbacks)), origin,
                               name);
}

}  // namespace content
