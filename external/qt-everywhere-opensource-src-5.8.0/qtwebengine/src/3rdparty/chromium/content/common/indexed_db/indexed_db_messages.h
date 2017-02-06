// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/common/indexed_db/indexed_db_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"
#include "url/origin.h"

// Singly-included section for typedefs in multiply-included file.
#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_

// An index id, and corresponding set of keys to insert.
typedef std::pair<int64_t, std::vector<content::IndexedDBKey>> IndexKeys;

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START IndexedDBMsgStart

// Argument structures used in messages

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBCursorDirection,
                          blink::WebIDBCursorDirectionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBPutMode, blink::WebIDBPutModeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBTaskType, blink::WebIDBTaskTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBTransactionMode,
                          blink::WebIDBTransactionModeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBDataLoss, blink::WebIDBDataLossTotal)

// Used to enumerate indexed databases.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryGetDatabaseNames_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(url::Origin, origin)
IPC_STRUCT_END()

// Used to open an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryOpen_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  // Identifier of the request
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // Identifier for database callbacks
  IPC_STRUCT_MEMBER(int32_t, ipc_database_callbacks_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(url::Origin, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(base::string16, name)
  // The transaction id used if a database upgrade is needed.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The requested version of the database.
  IPC_STRUCT_MEMBER(int64_t, version)
IPC_STRUCT_END()

// Used to delete an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryDeleteDatabase_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(url::Origin, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(base::string16, name)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCreateTransaction_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction id as minted by the frontend.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // To get to WebIDBDatabaseCallbacks.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_callbacks_id)
  // The scope of the transaction.
  IPC_STRUCT_MEMBER(std::vector<int64_t>, object_store_ids)
  // The transaction mode.
  IPC_STRUCT_MEMBER(blink::WebIDBTransactionMode, mode)
IPC_STRUCT_END()

// Used to create an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCreateObjectStore_Params)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction its associated with.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The storage id of the object store.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The name of the object store.
  IPC_STRUCT_MEMBER(base::string16, name)
  // The keyPath of the object store.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the object store created should have a key generator.
  IPC_STRUCT_MEMBER(bool, auto_increment)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseGet_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction its associated with.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The index's id.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // If this is just retrieving the key
  IPC_STRUCT_MEMBER(bool, key_only)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseGetAll_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction its associated with.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The index id.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // If this is just retrieving the key
  IPC_STRUCT_MEMBER(bool, key_only)
  // The max number of values to retrieve.
  IPC_STRUCT_MEMBER(int64_t, max_count)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_BlobOrFileInfo)
IPC_STRUCT_MEMBER(bool, is_file)
IPC_STRUCT_MEMBER(std::string, uuid)
IPC_STRUCT_MEMBER(base::string16, mime_type)
IPC_STRUCT_MEMBER(uint64_t, size)
IPC_STRUCT_MEMBER(base::string16, file_path)
IPC_STRUCT_MEMBER(base::string16, file_name)
IPC_STRUCT_MEMBER(double, last_modified)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_Value)
  // The serialized value being transferred.
  IPC_STRUCT_MEMBER(std::string, bits)
  // Sideband data for any blob or file encoded in value.
  IPC_STRUCT_MEMBER(std::vector<IndexedDBMsg_BlobOrFileInfo>, blob_or_file_info)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN_WITH_PARENT(IndexedDBMsg_ReturnValue, IndexedDBMsg_Value)
  IPC_STRUCT_TRAITS_PARENT(IndexedDBMsg_Value)
  // Optional primary key & path used only when key generator specified.
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
IPC_STRUCT_END()

// Used to set a value in an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabasePut_Params)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction it's associated with.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The index's id.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The value to set.
  IPC_STRUCT_MEMBER(IndexedDBMsg_Value, value)
  // The key to set it on (may not be "valid"/set in some cases).
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  // Whether this is an add or a put.
  IPC_STRUCT_MEMBER(blink::WebIDBPutMode, put_mode)
  // The index ids and the list of keys for each index.
  IPC_STRUCT_MEMBER(std::vector<IndexKeys>, index_keys)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The index if any.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(blink::WebIDBCursorDirection, direction)
  // If this is just retrieving the key
  IPC_STRUCT_MEMBER(bool, key_only)
  // The priority of this cursor.
  IPC_STRUCT_MEMBER(blink::WebIDBTaskType, task_type)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The index if any.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseDeleteRange_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseSetIndexKeys_Params)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The object store key that we're setting index keys for.
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  // The index ids and the list of keys for each index.
  IPC_STRUCT_MEMBER(std::vector<IndexKeys>, index_keys)
IPC_STRUCT_END()

// Used to create an index.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCreateIndex_Params)
  // The transaction this is associated with.
  IPC_STRUCT_MEMBER(int64_t, transaction_id)
  // The database being used.
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  // The object store the index belongs to.
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  // The storage id of the index.
  IPC_STRUCT_MEMBER(int64_t, index_id)
  // The name of the index.
  IPC_STRUCT_MEMBER(base::string16, name)
  // The keyPath of the index.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the index created has unique keys.
  IPC_STRUCT_MEMBER(bool, unique)
  // Whether the index created produces keys for each array entry.
  IPC_STRUCT_MEMBER(bool, multi_entry)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessIDBCursor_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(IndexedDBMsg_Value, value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorContinue_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(IndexedDBMsg_Value, value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_cursor_id)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, keys)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, primary_keys)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBMsg_Value>, values)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessArray_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBMsg_ReturnValue>, values)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessValue_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(IndexedDBMsg_ReturnValue, value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBIndexMetadata)
  IPC_STRUCT_MEMBER(int64_t, id)
  IPC_STRUCT_MEMBER(base::string16, name)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  IPC_STRUCT_MEMBER(bool, unique)
  IPC_STRUCT_MEMBER(bool, multi_entry)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBObjectStoreMetadata)
  IPC_STRUCT_MEMBER(int64_t, id)
  IPC_STRUCT_MEMBER(base::string16, name)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  IPC_STRUCT_MEMBER(bool, auto_increment)
  IPC_STRUCT_MEMBER(int64_t, max_index_id)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBIndexMetadata>, indexes)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBDatabaseMetadata)
  IPC_STRUCT_MEMBER(int64_t, id)
  IPC_STRUCT_MEMBER(base::string16, name)
  IPC_STRUCT_MEMBER(int64_t, version)
  IPC_STRUCT_MEMBER(int64_t, max_object_store_id)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBObjectStoreMetadata>, object_stores)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksUpgradeNeeded_Params)
  IPC_STRUCT_MEMBER(int32_t, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_callbacks_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_database_callbacks_id)
  IPC_STRUCT_MEMBER(int32_t, ipc_database_id)
  IPC_STRUCT_MEMBER(int64_t, old_version)
  IPC_STRUCT_MEMBER(blink::WebIDBDataLoss, data_loss)
  IPC_STRUCT_MEMBER(std::string, data_loss_message)
  IPC_STRUCT_MEMBER(IndexedDBDatabaseMetadata, idb_metadata)
IPC_STRUCT_END()

// Indexed DB messages sent from the browser to the renderer.

// The thread_id needs to be the first parameter in these messages.  In the IO
// thread on the renderer/client process, an IDB message filter assumes the
// thread_id is the first int.

// IDBCallback message handlers.
IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessIDBCursor,
                     IndexedDBMsg_CallbacksSuccessIDBCursor_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorContinue,
                     IndexedDBMsg_CallbacksSuccessCursorContinue_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorAdvance,
                     IndexedDBMsg_CallbacksSuccessCursorContinue_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorPrefetch,
                     IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessArray,
                     IndexedDBMsg_CallbacksSuccessArray_Params)

IPC_MESSAGE_CONTROL5(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     int32_t /* ipc_database_callbacks_id */,
                     int32_t /* ipc_database_id */,
                     IndexedDBDatabaseMetadata)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     content::IndexedDBKey /* indexed_db_key */)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessValue,
                     IndexedDBMsg_CallbacksSuccessValue_Params)

IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessInteger,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     int64_t /* value */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessUndefined,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessStringList,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     std::vector<base::string16> /* dom_string_list */)
IPC_MESSAGE_CONTROL4(IndexedDBMsg_CallbacksError,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     int /* code */,
                     base::string16 /* message */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksBlocked,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksIntBlocked,
                     int32_t /* ipc_thread_id */,
                     int32_t /* ipc_callbacks_id */,
                     int64_t /* existing_version */)
IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksUpgradeNeeded,
                     IndexedDBMsg_CallbacksUpgradeNeeded_Params)

// IDBDatabaseCallback message handlers
IPC_MESSAGE_CONTROL2(IndexedDBMsg_DatabaseCallbacksForcedClose,
                     int32_t, /* ipc_thread_id */
                     int32_t) /* ipc_database_callbacks_id */
IPC_MESSAGE_CONTROL4(IndexedDBMsg_DatabaseCallbacksVersionChange,
                     int32_t, /* ipc_thread_id */
                     int32_t, /* ipc_database_callbacks_id */
                     int64_t, /* old_version */
                     int64_t) /* new_version */
IPC_MESSAGE_CONTROL5(IndexedDBMsg_DatabaseCallbacksAbort,
                     int32_t,        /* ipc_thread_id */
                     int32_t,        /* ipc_database_callbacks_id */
                     int64_t,        /* transaction_id */
                     int,            /* code */
                     base::string16) /* message */
IPC_MESSAGE_CONTROL3(IndexedDBMsg_DatabaseCallbacksComplete,
                     int32_t, /* ipc_thread_id */
                     int32_t, /* ipc_database_callbacks_id */
                     int64_t) /* transaction_id */

// Indexed DB messages sent from the renderer to the browser.

// WebIDBCursor::advance() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_CursorAdvance,
                     int32_t,  /* ipc_cursor_id */
                     int32_t,  /* ipc_thread_id */
                     int32_t,  /* ipc_callbacks_id */
                     uint32_t) /* count */

// WebIDBCursor::continue() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_CursorContinue,
                     int32_t,               /* ipc_cursor_id */
                     int32_t,               /* ipc_thread_id */
                     int32_t,               /* ipc_callbacks_id */
                     content::IndexedDBKey, /* key */
                     content::IndexedDBKey) /* primary_key */

// WebIDBCursor::prefetchContinue() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_CursorPrefetch,
                     int32_t, /* ipc_cursor_id */
                     int32_t, /* ipc_thread_id */
                     int32_t, /* ipc_callbacks_id */
                     int32_t) /* n */

// WebIDBCursor::prefetchReset() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_CursorPrefetchReset,
                     int32_t, /* ipc_cursor_id */
                     int32_t, /* used_prefetches */
                     int32_t) /* used_prefetches */

// WebIDBFactory::getDatabaseNames() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryGetDatabaseNames,
                     IndexedDBHostMsg_FactoryGetDatabaseNames_Params)

// WebIDBFactory::open() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryOpen,
                     IndexedDBHostMsg_FactoryOpen_Params)

// WebIDBFactory::deleteDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryDeleteDatabase,
                     IndexedDBHostMsg_FactoryDeleteDatabase_Params)

IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_AckReceivedBlobs,
                     std::vector<std::string>) /* uuids */

// WebIDBDatabase::createObjectStore() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseCreateObjectStore,
                     IndexedDBHostMsg_DatabaseCreateObjectStore_Params)

// WebIDBDatabase::deleteObjectStore() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                     int32_t, /* ipc_database_id */
                     int64_t, /* transaction_id */
                     int64_t) /* object_store_id */

// WebIDBDatabase::createTransaction() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseCreateTransaction,
                     IndexedDBHostMsg_DatabaseCreateTransaction_Params)

// WebIDBDatabase::close() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseClose,
                     int32_t /* ipc_database_id */)

// WebIDBDatabase::versionChangeIgnored() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseVersionChangeIgnored,
                     int32_t /* ipc_database_id */)

// WebIDBDatabase::~WebIDBDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseDestroyed,
                     int32_t /* ipc_database_id */)

// WebIDBDatabase::get() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseGet,
                     IndexedDBHostMsg_DatabaseGet_Params)

// WebIDBDatabase::getAll() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseGetAll,
                     IndexedDBHostMsg_DatabaseGetAll_Params)

// WebIDBDatabase::put() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabasePut,
                     IndexedDBHostMsg_DatabasePut_Params)

// WebIDBDatabase::setIndexKeys() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseSetIndexKeys,
                     IndexedDBHostMsg_DatabaseSetIndexKeys_Params)

// WebIDBDatabase::setIndexesReady() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_DatabaseSetIndexesReady,
                     int32_t,              /* ipc_database_id */
                     int64_t,              /* transaction_id */
                     int64_t,              /* object_store_id */
                     std::vector<int64_t>) /* index_ids */

// WebIDBDatabase::openCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseOpenCursor,
                     IndexedDBHostMsg_DatabaseOpenCursor_Params)

// WebIDBDatabase::count() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseCount,
                     IndexedDBHostMsg_DatabaseCount_Params)

// WebIDBDatabase::deleteRange() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseDeleteRange,
                     IndexedDBHostMsg_DatabaseDeleteRange_Params)

// WebIDBDatabase::clear() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_DatabaseClear,
                     int32_t, /* ipc_thread_id */
                     int32_t, /* ipc_callbacks_id */
                     int32_t, /* ipc_database_id */
                     int64_t, /* transaction_id */
                     int64_t) /* object_store_id */

// WebIDBDatabase::createIndex() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseCreateIndex,
                     IndexedDBHostMsg_DatabaseCreateIndex_Params)

// WebIDBDatabase::deleteIndex() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_DatabaseDeleteIndex,
                     int32_t, /* ipc_database_id */
                     int64_t, /* transaction_id */
                     int64_t, /* object_store_id */
                     int64_t) /* index_id */

// WebIDBDatabase::abort() message.
IPC_MESSAGE_CONTROL2(IndexedDBHostMsg_DatabaseAbort,
                     int32_t, /* ipc_database_id */
                     int64_t) /* transaction_id */

// WebIDBDatabase::commit() message.
IPC_MESSAGE_CONTROL2(IndexedDBHostMsg_DatabaseCommit,
                     int32_t, /* ipc_database_id */
                     int64_t) /* transaction_id */

// WebIDBDatabase::~WebIDBCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_CursorDestroyed,
                     int32_t /* ipc_cursor_id */)
