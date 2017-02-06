// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#include <stdint.h>

#include "content/common/appcache_interfaces.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START AppCacheMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheEventID,
                          content::APPCACHE_EVENT_ID_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheStatus,
                          content::APPCACHE_STATUS_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheErrorReason,
    content::APPCACHE_ERROR_REASON_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheInfo)
  IPC_STRUCT_TRAITS_MEMBER(manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(creation_time)
  IPC_STRUCT_TRAITS_MEMBER(last_update_time)
  IPC_STRUCT_TRAITS_MEMBER(last_access_time)
  IPC_STRUCT_TRAITS_MEMBER(cache_id)
  IPC_STRUCT_TRAITS_MEMBER(group_id)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_complete)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheResourceInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_master)
  IPC_STRUCT_TRAITS_MEMBER(is_manifest)
  IPC_STRUCT_TRAITS_MEMBER(is_fallback)
  IPC_STRUCT_TRAITS_MEMBER(is_foreign)
  IPC_STRUCT_TRAITS_MEMBER(is_explicit)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheErrorDetails)
IPC_STRUCT_TRAITS_MEMBER(message)
IPC_STRUCT_TRAITS_MEMBER(reason)
IPC_STRUCT_TRAITS_MEMBER(url)
IPC_STRUCT_TRAITS_MEMBER(status)
IPC_STRUCT_TRAITS_MEMBER(is_cross_origin)
IPC_STRUCT_TRAITS_END()

// AppCache messages sent from the child process to the browser.

// Informs the browser of a new appcache host.
IPC_MESSAGE_CONTROL1(AppCacheHostMsg_RegisterHost,
                     int /* host_id */)

// Informs the browser of an appcache host being destroyed.
IPC_MESSAGE_CONTROL1(AppCacheHostMsg_UnregisterHost,
                     int /* host_id */)

// Informs the browser of which host caused another to be created.
// This can influence which appcache should be utilized for the main
// resource load into the newly created host, so it should be sent
// prior to the main resource request being initiated.
IPC_MESSAGE_CONTROL2(AppCacheHostMsg_SetSpawningHostId,
                     int /* host_id */,
                     int /* spawning_host_id */)

// Initiates the cache selection algorithm for the given host.
// This is sent prior to any subresource loads. An AppCacheMsg_CacheSelected
// message will be sent in response.
// 'host_id' indentifies a specific document or worker
// 'document_url' the url of the main resource
// 'appcache_document_was_loaded_from' the id of the appcache the main
//     resource was loaded from or kAppCacheNoCacheId
// 'opt_manifest_url' the manifest url specified in the <html> tag if any
IPC_MESSAGE_CONTROL4(AppCacheHostMsg_SelectCache,
                     int /* host_id */,
                     GURL /* document_url */,
                     int64_t /* appcache_document_was_loaded_from */,
                     GURL /* opt_manifest_url */)

// Initiates worker specific cache selection algorithm for the given host.
IPC_MESSAGE_CONTROL3(AppCacheHostMsg_SelectCacheForWorker,
                     int /* host_id */,
                     int /* parent_process_id */,
                     int /* parent_host_id */)
IPC_MESSAGE_CONTROL2(AppCacheHostMsg_SelectCacheForSharedWorker,
                     int /* host_id */,
                     int64_t /* appcache_id */)

// Informs the browser of a 'foreign' entry in an appcache.
IPC_MESSAGE_CONTROL3(AppCacheHostMsg_MarkAsForeignEntry,
                     int /* host_id */,
                     GURL /* document_url */,
                     int64_t /* appcache_document_was_loaded_from */)

// Returns the status of the appcache associated with host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheHostMsg_GetStatus,
                            int /* host_id */,
                            content::AppCacheStatus)

// Initiates an update of the appcache associated with host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheHostMsg_StartUpdate,
                            int /* host_id */,
                            bool /* success */)

// Swaps a new pending appcache, if there is one, into use for host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheHostMsg_SwapCache,
                            int /* host_id */,
                            bool /* success */)

// Gets resource list from appcache synchronously.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheHostMsg_GetResourceList,
                            int /* host_id in*/,
                            std::vector<content::AppCacheResourceInfo>
                            /* resources out */)


// AppCache messages sent from the browser to the child process.

// Notifies the renderer of the appcache that has been selected for a
// a particular host. This is sent in reply to AppCacheHostMsg_SelectCache.
IPC_MESSAGE_CONTROL2(AppCacheMsg_CacheSelected,
                     int /* host_id */,
                     content::AppCacheInfo)

// Notifies the renderer of an AppCache status change.
IPC_MESSAGE_CONTROL2(AppCacheMsg_StatusChanged,
                     std::vector<int> /* host_ids */,
                     content::AppCacheStatus)

// Notifies the renderer of an AppCache event other than the
// progress event which has a seperate message.
IPC_MESSAGE_CONTROL2(AppCacheMsg_EventRaised,
                     std::vector<int> /* host_ids */,
                     content::AppCacheEventID)

// Notifies the renderer of an AppCache progress event.
IPC_MESSAGE_CONTROL4(AppCacheMsg_ProgressEventRaised,
                     std::vector<int> /* host_ids */,
                     GURL /* url being processed */,
                     int /* total */,
                     int /* complete */)

// Notifies the renderer of an AppCache error event.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ErrorEventRaised,
                     std::vector<int> /* host_ids */,
                     content::AppCacheErrorDetails)

// Notifies the renderer of an AppCache logging message.
IPC_MESSAGE_CONTROL3(AppCacheMsg_LogMessage,
                     int /* host_id */,
                     int /* log_level */,
                     std::string /* message */)

// Notifies the renderer of the fact that AppCache access was blocked.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ContentBlocked,
                     int /* host_id */,
                     GURL /* manifest_url */)
