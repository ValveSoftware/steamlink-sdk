// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START CacheStorageMsgStart

// TODO(jsbell): This depends on traits for content::ServiceWorkerResponse
// which are defined in service_worker_messages.h - correct this implicit
// cross-dependency.

IPC_STRUCT_TRAITS_BEGIN(content::CacheStorageCacheQueryParams)
  IPC_STRUCT_TRAITS_MEMBER(ignore_search)
  IPC_STRUCT_TRAITS_MEMBER(ignore_method)
  IPC_STRUCT_TRAITS_MEMBER(ignore_vary)
  IPC_STRUCT_TRAITS_MEMBER(cache_name)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::CacheStorageCacheOperationType,
                          content::CACHE_STORAGE_CACHE_OPERATION_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::CacheStorageBatchOperation)
  IPC_STRUCT_TRAITS_MEMBER(operation_type)
  IPC_STRUCT_TRAITS_MEMBER(request)
  IPC_STRUCT_TRAITS_MEMBER(response)
  IPC_STRUCT_TRAITS_MEMBER(match_params)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerCacheError,
                          blink::WebServiceWorkerCacheErrorLast)

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

// CacheStorage operations in the browser.
IPC_MESSAGE_CONTROL4(CacheStorageHostMsg_CacheStorageHas,
                     int /* thread_id */,
                     int /* request_id */,
                     url::Origin /* origin */,
                     base::string16 /* fetch_store_name */)

IPC_MESSAGE_CONTROL4(CacheStorageHostMsg_CacheStorageOpen,
                     int /* thread_id */,
                     int /* request_id */,
                     url::Origin /* origin */,
                     base::string16 /* fetch_store_name */)

IPC_MESSAGE_CONTROL4(CacheStorageHostMsg_CacheStorageDelete,
                     int /* thread_id */,
                     int /* request_id */,
                     url::Origin /* origin */,
                     base::string16 /* fetch_store_name */)

IPC_MESSAGE_CONTROL3(CacheStorageHostMsg_CacheStorageKeys,
                     int /* thread_id */,
                     int /* request_id */,
                     url::Origin /* origin */)

IPC_MESSAGE_CONTROL5(CacheStorageHostMsg_CacheStorageMatch,
                     int /* thread_id */,
                     int /* request_id */,
                     url::Origin /* origin */,
                     content::ServiceWorkerFetchRequest,
                     content::CacheStorageCacheQueryParams)

// Cache operations in the browser.
IPC_MESSAGE_CONTROL5(CacheStorageHostMsg_CacheMatch,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* cache_id */,
                     content::ServiceWorkerFetchRequest,
                     content::CacheStorageCacheQueryParams)

IPC_MESSAGE_CONTROL5(CacheStorageHostMsg_CacheMatchAll,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* cache_id */,
                     content::ServiceWorkerFetchRequest,
                     content::CacheStorageCacheQueryParams)

IPC_MESSAGE_CONTROL5(CacheStorageHostMsg_CacheKeys,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* cache_id */,
                     content::ServiceWorkerFetchRequest,
                     content::CacheStorageCacheQueryParams)

IPC_MESSAGE_CONTROL4(CacheStorageHostMsg_CacheBatch,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* cache_id */,
                     std::vector<content::CacheStorageBatchOperation>)

IPC_MESSAGE_CONTROL1(CacheStorageHostMsg_CacheClosed,
                     int /* cache_id */)

IPC_MESSAGE_CONTROL1(CacheStorageHostMsg_BlobDataHandled,
                     std::string /* uuid */)

//---------------------------------------------------------------------------
// Messages sent from the browser to the child process.
//
// All such messages must includes thread_id as the first int; it is read off
// by CacheStorageMessageFilter::GetWorkerThreadIdForMessage to route delivery
// to the appropriate thread.

// Sent at successful completion of CacheStorage operations.
IPC_MESSAGE_CONTROL2(CacheStorageMsg_CacheStorageHasSuccess,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageOpenSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* fetch_store_id */)
IPC_MESSAGE_CONTROL2(CacheStorageMsg_CacheStorageDeleteSuccess,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageKeysSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<base::string16> /* keys */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageMatchSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerResponse)

// Sent at erroneous completion of CacheStorage operations.
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageHasError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageOpenError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageDeleteError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageKeysError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheStorageMatchError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)

// Sent at successful completion of Cache operations.
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheMatchSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerResponse)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheMatchAllSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerResponse>)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheKeysSuccess,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerFetchRequest>)
IPC_MESSAGE_CONTROL2(CacheStorageMsg_CacheBatchSuccess,
                     int /* thread_id */,
                     int /* request_id */)

// Sent at erroneous completion of CacheStorage operations.
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheMatchError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheMatchAllError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheKeysError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL3(CacheStorageMsg_CacheBatchError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
