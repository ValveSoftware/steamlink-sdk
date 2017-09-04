// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbdatabase_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/stl_util.h"
#include "content/child/indexed_db/indexed_db_callbacks_impl.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBMetadata.h"

using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBMetadata;
using blink::WebIDBKey;
using blink::WebIDBKeyPath;
using blink::WebIDBKeyRange;
using blink::WebIDBObserver;
using blink::WebString;
using blink::WebVector;
using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using indexed_db::mojom::DatabaseAssociatedPtrInfo;

namespace content {

namespace {

std::vector<content::IndexedDBIndexKeys> ConvertWebIndexKeys(
    const WebVector<long long>& index_ids,
    const WebVector<WebIDBDatabase::WebIndexKeys>& index_keys) {
  DCHECK_EQ(index_ids.size(), index_keys.size());
  std::vector<content::IndexedDBIndexKeys> result;
  result.resize(index_ids.size());
  for (size_t i = 0, len = index_ids.size(); i < len; ++i) {
    result[i].first = index_ids[i];
    result[i].second.resize(index_keys[i].size());
    for (size_t j = 0; j < index_keys[i].size(); ++j)
      result[i].second[j] = IndexedDBKeyBuilder::Build(index_keys[i][j]);
  }
  return result;
}

}  // namespace

class WebIDBDatabaseImpl::IOThreadHelper {
 public:
  IOThreadHelper();
  ~IOThreadHelper();

  void Bind(DatabaseAssociatedPtrInfo database_info);
  void CreateObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& name,
                         const IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(int64_t transaction_id, int64_t object_store_id);
  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& new_name);
  void CreateTransaction(int64_t transaction_id,
                         const std::vector<int64_t>& object_store_ids,
                         blink::WebIDBTransactionMode mode);
  void Close();
  void VersionChangeIgnored();
  void AddObserver(int64_t transaction_id,
                   int32_t observer_id,
                   bool include_transaction,
                   bool no_records,
                   bool values,
                   uint16_t operation_types);
  void RemoveObservers(const std::vector<int32_t>& observers);
  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           const IndexedDBKeyRange& key_range,
           bool key_only,
           std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              const IndexedDBKeyRange& key_range,
              int64_t max_count,
              bool key_only,
              std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void Put(int64_t transaction_id,
           int64_t object_store_id,
           indexed_db::mojom::ValuePtr value,
           const IndexedDBKey& key,
           blink::WebIDBPutMode mode,
           std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
           const std::vector<content::IndexedDBIndexKeys>& index_keys);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    const IndexedDBKey& primary_key,
                    const std::vector<content::IndexedDBIndexKeys>& index_keys);
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const std::vector<int64_t>& index_ids);
  void OpenCursor(int64_t transaction_id,
                  int64_t object_store_id,
                  int64_t index_id,
                  const IndexedDBKeyRange& key_range,
                  blink::WebIDBCursorDirection direction,
                  bool key_only,
                  blink::WebIDBTaskType task_type,
                  std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             const IndexedDBKeyRange& key_range,
             std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   const IndexedDBKeyRange& key_range,
                   std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void Clear(int64_t transaction_id,
             int64_t object_store_id,
             std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  void CreateIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& name,
                   const IndexedDBKeyPath& key_path,
                   bool unique,
                   bool multi_entry);
  void DeleteIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id);
  void RenameIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& new_name);
  void Abort(int64_t transaction_id);
  void Commit(int64_t transaction_id);
  void AckReceivedBlobs(const std::vector<std::string>& uuids);

 private:
  CallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);

  indexed_db::mojom::DatabaseAssociatedPtr database_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

WebIDBDatabaseImpl::WebIDBDatabaseImpl(
    DatabaseAssociatedPtrInfo database_info,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    scoped_refptr<ThreadSafeSender> thread_safe_sender)
    : helper_(new IOThreadHelper()),
      io_runner_(std::move(io_runner)),
      thread_safe_sender_(std::move(thread_safe_sender)) {
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Bind, base::Unretained(helper_),
                            base::Passed(&database_info)));
}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() {
  io_runner_->DeleteSoon(FROM_HERE, helper_);
}

void WebIDBDatabaseImpl::createObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const WebString& name,
                                           const WebIDBKeyPath& key_path,
                                           bool auto_increment) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::CreateObjectStore, base::Unretained(helper_),
                 transaction_id, object_store_id, base::string16(name),
                 IndexedDBKeyPathBuilder::Build(key_path), auto_increment));
}

void WebIDBDatabaseImpl::deleteObjectStore(long long transaction_id,
                                           long long object_store_id) {
  io_runner_->PostTask(FROM_HERE, base::Bind(&IOThreadHelper::DeleteObjectStore,
                                             base::Unretained(helper_),
                                             transaction_id, object_store_id));
}

void WebIDBDatabaseImpl::renameObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const blink::WebString& new_name) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::RenameObjectStore, base::Unretained(helper_),
                 transaction_id, object_store_id, base::string16(new_name)));
}

void WebIDBDatabaseImpl::createTransaction(
    long long transaction_id,
    const WebVector<long long>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::CreateTransaction, base::Unretained(helper_),
                 transaction_id, std::vector<int64_t>(object_store_ids.begin(),
                                                      object_store_ids.end()),
                 mode));
}

void WebIDBDatabaseImpl::close() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  std::vector<int32_t> remove_observer_ids(observer_ids_.begin(),
                                           observer_ids_.end());
  dispatcher->RemoveObservers(remove_observer_ids);
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Close, base::Unretained(helper_)));
}

void WebIDBDatabaseImpl::versionChangeIgnored() {
  io_runner_->PostTask(FROM_HERE,
                       base::Bind(&IOThreadHelper::VersionChangeIgnored,
                                  base::Unretained(helper_)));
}

int32_t WebIDBDatabaseImpl::addObserver(
    std::unique_ptr<WebIDBObserver> observer,
    long long transaction_id) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  WebIDBObserver* observer_ptr = observer.get();
  int32_t observer_id = dispatcher->RegisterObserver(std::move(observer));
  observer_ids_.insert(observer_id);
  static_assert(blink::WebIDBOperationTypeCount < sizeof(uint16_t) * CHAR_BIT,
                "WebIDBOperationType Count exceeds size of uint16_t");
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::AddObserver, base::Unretained(helper_),
                 transaction_id, observer_id, observer_ptr->transaction(),
                 observer_ptr->noRecords(), observer_ptr->values(),
                 observer_ptr->operationTypes().to_ulong()));
  return observer_id;
}

void WebIDBDatabaseImpl::removeObservers(
    const WebVector<int32_t>& observer_ids_to_remove) {
  std::vector<int32_t> remove_observer_ids(
      observer_ids_to_remove.data(),
      observer_ids_to_remove.data() + observer_ids_to_remove.size());
  for (int32_t id : observer_ids_to_remove)
    observer_ids_.erase(id);

  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RemoveObservers(remove_observer_ids);
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::RemoveObservers,
                            base::Unretained(helper_), remove_observer_ids));
}

void WebIDBDatabaseImpl::get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const WebIDBKeyRange& key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Get, base::Unretained(helper_),
                            transaction_id, object_store_id, index_id,
                            IndexedDBKeyRangeBuilder::Build(key_range),
                            key_only, base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::getAll(long long transaction_id,
                                long long object_store_id,
                                long long index_id,
                                const WebIDBKeyRange& key_range,
                                long long max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::GetAll, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id,
                 IndexedDBKeyRangeBuilder::Build(key_range), max_count,
                 key_only, base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::put(long long transaction_id,
                             long long object_store_id,
                             const blink::WebData& value,
                             const blink::WebVector<WebBlobInfo>& web_blob_info,
                             const WebIDBKey& web_key,
                             blink::WebIDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const WebVector<long long>& index_ids,
                             const WebVector<WebIndexKeys>& index_keys) {
  IndexedDBKey key = IndexedDBKeyBuilder::Build(web_key);

  if (value.size() + key.size_estimate() > max_put_value_size_) {
    callbacks->onError(blink::WebIDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        WebString::fromUTF8(base::StringPrintf(
            "The serialized value is too large"
            " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
            value.size(), max_put_value_size_))));
    return;
  }

  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto mojo_value = indexed_db::mojom::Value::New();
  mojo_value->bits.assign(value.data(), value.data() + value.size());
  mojo_value->blob_or_file_info.reserve(web_blob_info.size());
  for (const WebBlobInfo& info : web_blob_info) {
    auto blob_info = indexed_db::mojom::BlobInfo::New();
    if (info.isFile()) {
      blob_info->file = indexed_db::mojom::FileInfo::New();
      blob_info->file->path =
          base::FilePath::FromUTF8Unsafe(info.filePath().utf8());
      blob_info->file->name = info.fileName();
      blob_info->file->last_modified =
          base::Time::FromDoubleT(info.lastModified());
    }
    blob_info->size = info.size();
    blob_info->uuid = info.uuid().latin1();
    DCHECK(blob_info->uuid.size());
    blob_info->mime_type = info.type();
    mojo_value->blob_or_file_info.push_back(std::move(blob_info));
  }

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::Put, base::Unretained(helper_),
                 transaction_id, object_store_id, base::Passed(&mojo_value),
                 key, put_mode, base::Passed(&callbacks_impl),
                 ConvertWebIndexKeys(index_ids, index_keys)));
}

void WebIDBDatabaseImpl::setIndexKeys(
    long long transaction_id,
    long long object_store_id,
    const WebIDBKey& primary_key,
    const WebVector<long long>& index_ids,
    const WebVector<WebIndexKeys>& index_keys) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::SetIndexKeys, base::Unretained(helper_),
                 transaction_id, object_store_id,
                 IndexedDBKeyBuilder::Build(primary_key),
                 ConvertWebIndexKeys(index_ids, index_keys)));
}

void WebIDBDatabaseImpl::setIndexesReady(
    long long transaction_id,
    long long object_store_id,
    const WebVector<long long>& web_index_ids) {
  std::vector<int64_t> index_ids(web_index_ids.data(),
                                 web_index_ids.data() + web_index_ids.size());
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::SetIndexesReady, base::Unretained(helper_),
                 transaction_id, object_store_id, base::Passed(&index_ids)));
}

void WebIDBDatabaseImpl::openCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const WebIDBKeyRange& key_range,
                                    blink::WebIDBCursorDirection direction,
                                    bool key_only,
                                    blink::WebIDBTaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::OpenCursor, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id,
                 IndexedDBKeyRangeBuilder::Build(key_range), direction,
                 key_only, task_type, base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::count(long long transaction_id,
                               long long object_store_id,
                               long long index_id,
                               const WebIDBKeyRange& key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Count, base::Unretained(helper_),
                            transaction_id, object_store_id, index_id,
                            IndexedDBKeyRangeBuilder::Build(key_range),
                            base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::deleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const WebIDBKeyRange& key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::DeleteRange, base::Unretained(helper_),
                 transaction_id, object_store_id,
                 IndexedDBKeyRangeBuilder::Build(key_range),
                 base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::clear(long long transaction_id,
                               long long object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->ResetCursorPrefetchCaches(transaction_id,
                                        IndexedDBDispatcher::kAllCursors);

  auto callbacks_impl = base::MakeUnique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, io_runner_,
      thread_safe_sender_);
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Clear, base::Unretained(helper_),
                            transaction_id, object_store_id,
                            base::Passed(&callbacks_impl)));
}

void WebIDBDatabaseImpl::createIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& name,
                                     const WebIDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::CreateIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id,
                 base::string16(name), IndexedDBKeyPathBuilder::Build(key_path),
                 unique, multi_entry));
}

void WebIDBDatabaseImpl::deleteIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::DeleteIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id));
}

void WebIDBDatabaseImpl::renameIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& new_name) {
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IOThreadHelper::RenameIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id, new_name));
}

void WebIDBDatabaseImpl::abort(long long transaction_id) {
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Abort, base::Unretained(helper_),
                            transaction_id));
}

void WebIDBDatabaseImpl::commit(long long transaction_id) {
  io_runner_->PostTask(
      FROM_HERE, base::Bind(&IOThreadHelper::Commit, base::Unretained(helper_),
                            transaction_id));
}

void WebIDBDatabaseImpl::ackReceivedBlobs(const WebVector<WebString>& uuids) {
  DCHECK(uuids.size());
  std::vector<std::string> param(uuids.size());
  for (size_t i = 0; i < uuids.size(); ++i)
    param[i] = uuids[i].latin1().data();
  io_runner_->PostTask(FROM_HERE,
                       base::Bind(&IOThreadHelper::AckReceivedBlobs,
                                  base::Unretained(helper_), std::move(param)));
}

WebIDBDatabaseImpl::IOThreadHelper::IOThreadHelper() {}

WebIDBDatabaseImpl::IOThreadHelper::~IOThreadHelper() {}

void WebIDBDatabaseImpl::IOThreadHelper::Bind(
    DatabaseAssociatedPtrInfo database_info) {
  database_.Bind(std::move(database_info));
}

void WebIDBDatabaseImpl::IOThreadHelper::CreateObjectStore(
    int64_t transaction_id,
    int64_t object_store_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool auto_increment) {
  database_->CreateObjectStore(transaction_id, object_store_id, name, key_path,
                               auto_increment);
}

void WebIDBDatabaseImpl::IOThreadHelper::DeleteObjectStore(
    int64_t transaction_id,
    int64_t object_store_id) {
  database_->DeleteObjectStore(transaction_id, object_store_id);
}

void WebIDBDatabaseImpl::IOThreadHelper::RenameObjectStore(
    int64_t transaction_id,
    int64_t object_store_id,
    const base::string16& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id, new_name);
}

void WebIDBDatabaseImpl::IOThreadHelper::CreateTransaction(
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  database_->CreateTransaction(transaction_id, object_store_ids, mode);
}

void WebIDBDatabaseImpl::IOThreadHelper::Close() {
  database_->Close();
}

void WebIDBDatabaseImpl::IOThreadHelper::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabaseImpl::IOThreadHelper::AddObserver(int64_t transaction_id,
                                                     int32_t observer_id,
                                                     bool include_transaction,
                                                     bool no_records,
                                                     bool values,
                                                     uint16_t operation_types) {
  database_->AddObserver(transaction_id, observer_id, include_transaction,
                         no_records, values, operation_types);
}

void WebIDBDatabaseImpl::IOThreadHelper::RemoveObservers(
    const std::vector<int32_t>& observers) {
  database_->RemoveObservers(observers);
}

void WebIDBDatabaseImpl::IOThreadHelper::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->Get(transaction_id, object_store_id, index_id, key_range, key_only,
                 GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::GetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    int64_t max_count,
    bool key_only,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->GetAll(transaction_id, object_store_id, index_id, key_range,
                    key_only, max_count,
                    GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::Put(
    int64_t transaction_id,
    int64_t object_store_id,
    indexed_db::mojom::ValuePtr value,
    const IndexedDBKey& key,
    blink::WebIDBPutMode mode,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks,
    const std::vector<content::IndexedDBIndexKeys>& index_keys) {
  database_->Put(transaction_id, object_store_id, std::move(value), key, mode,
                 index_keys, GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<content::IndexedDBIndexKeys>& index_keys) {
  database_->SetIndexKeys(transaction_id, object_store_id, primary_key,
                          index_keys);
}

void WebIDBDatabaseImpl::IOThreadHelper::SetIndexesReady(
    int64_t transaction_id,
    int64_t object_store_id,
    const std::vector<int64_t>& index_ids) {
  database_->SetIndexesReady(transaction_id, object_store_id, index_ids);
}

void WebIDBDatabaseImpl::IOThreadHelper::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection direction,
    bool key_only,
    blink::WebIDBTaskType task_type,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->OpenCursor(transaction_id, object_store_id, index_id, key_range,
                        direction, key_only, task_type,
                        GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->Count(transaction_id, object_store_id, index_id, key_range,
                   GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->DeleteRange(transaction_id, object_store_id, key_range,
                         GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  database_->Clear(transaction_id, object_store_id,
                   GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBDatabaseImpl::IOThreadHelper::CreateIndex(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool unique,
    bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id, name,
                         key_path, unique, multi_entry);
}

void WebIDBDatabaseImpl::IOThreadHelper::DeleteIndex(int64_t transaction_id,
                                                     int64_t object_store_id,
                                                     int64_t index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabaseImpl::IOThreadHelper::RenameIndex(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const base::string16& new_name) {
  database_->RenameIndex(transaction_id, object_store_id, index_id, new_name);
}

void WebIDBDatabaseImpl::IOThreadHelper::Abort(int64_t transaction_id) {
  database_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::IOThreadHelper::Commit(int64_t transaction_id) {
  database_->Commit(transaction_id);
}

void WebIDBDatabaseImpl::IOThreadHelper::AckReceivedBlobs(
    const std::vector<std::string>& uuids) {
  database_->AckReceivedBlobs(uuids);
}

CallbacksAssociatedPtrInfo
WebIDBDatabaseImpl::IOThreadHelper::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  CallbacksAssociatedPtrInfo ptr_info;
  indexed_db::mojom::CallbacksAssociatedRequest request;
  database_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace content
