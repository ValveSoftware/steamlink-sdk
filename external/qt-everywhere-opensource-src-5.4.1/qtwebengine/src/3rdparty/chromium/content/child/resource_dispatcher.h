// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_CHILD_RESOURCE_DISPATCHER_H_
#define CONTENT_CHILD_RESOURCE_DISPATCHER_H_

#include <deque>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "net/base/request_priority.h"
#include "webkit/common/resource_type.h"

struct ResourceMsg_RequestCompleteData;

namespace blink {
class WebThreadedDataReceiver;
}

namespace webkit_glue {
class ResourceLoaderBridge;
}

namespace content {
class RequestPeer;
class ResourceDispatcherDelegate;
class ThreadedDataProvider;
struct ResourceResponseInfo;
struct RequestInfo;
struct ResourceResponseHead;
struct SiteIsolationResponseMetaData;

// This class serves as a communication interface between the
// ResourceDispatcherHost in the browser process and the ResourceLoaderBridge in
// the child process.  It can be used from any child process.
class CONTENT_EXPORT ResourceDispatcher : public IPC::Listener {
 public:
  explicit ResourceDispatcher(IPC::Sender* sender);
  virtual ~ResourceDispatcher();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Creates a ResourceLoaderBridge for this type of dispatcher, this is so
  // this can be tested regardless of the ResourceLoaderBridge::Create
  // implementation.
  webkit_glue::ResourceLoaderBridge* CreateBridge(
      const RequestInfo& request_info);

  // Adds a request from the |pending_requests_| list, returning the new
  // requests' ID.
  int AddPendingRequest(RequestPeer* callback,
                        ResourceType::Type resource_type,
                        int origin_pid,
                        const GURL& frame_origin,
                        const GURL& request_url,
                        bool download_to_file);

  // Removes a request from the |pending_requests_| list, returning true if the
  // request was found and removed.
  bool RemovePendingRequest(int request_id);

  // Cancels a request in the |pending_requests_| list.  The request will be
  // removed from the dispatcher as well.
  void CancelPendingRequest(int request_id);

  // Toggles the is_deferred attribute for the specified request.
  void SetDefersLoading(int request_id, bool value);

  // Indicates the priority of the specified request changed.
  void DidChangePriority(int request_id,
                         net::RequestPriority new_priority,
                         int intra_priority_value);

  // The provided data receiver will receive incoming resource data rather
  // than the resource bridge.
  bool AttachThreadedDataReceiver(
      int request_id, blink::WebThreadedDataReceiver* threaded_data_receiver);

  IPC::Sender* message_sender() const { return message_sender_; }

  // This does not take ownership of the delegate. It is expected that the
  // delegate have a longer lifetime than the ResourceDispatcher.
  void set_delegate(ResourceDispatcherDelegate* delegate) {
    delegate_ = delegate;
  }

  // Remembers IO thread timestamp for next resource message.
  void set_io_timestamp(base::TimeTicks io_timestamp) {
    io_timestamp_ = io_timestamp;
  }

 private:
  friend class ResourceDispatcherTest;

  typedef std::deque<IPC::Message*> MessageQueue;
  struct PendingRequestInfo {
    PendingRequestInfo();

    PendingRequestInfo(RequestPeer* peer,
                       ResourceType::Type resource_type,
                       int origin_pid,
                       const GURL& frame_origin,
                       const GURL& request_url,
                       bool download_to_file);

    ~PendingRequestInfo();

    RequestPeer* peer;
    ThreadedDataProvider* threaded_data_provider;
    ResourceType::Type resource_type;
    // The PID of the original process which issued this request. This gets
    // non-zero only for a request proxied by another renderer, particularly
    // requests from plugins.
    int origin_pid;
    MessageQueue deferred_message_queue;
    bool is_deferred;
    // Original requested url.
    GURL url;
    // The security origin of the frame that initiates this request.
    GURL frame_origin;
    // The url of the latest response even in case of redirection.
    GURL response_url;
    bool download_to_file;
    linked_ptr<IPC::Message> pending_redirect_message;
    base::TimeTicks request_start;
    base::TimeTicks response_start;
    base::TimeTicks completion_time;
    linked_ptr<base::SharedMemory> buffer;
    linked_ptr<SiteIsolationResponseMetaData> site_isolation_metadata;
    bool blocked_response;
    int buffer_size;
  };
  typedef base::hash_map<int, PendingRequestInfo> PendingRequestList;

  // Helper to lookup the info based on the request_id.
  // May return NULL if the request as been canceled from the client side.
  PendingRequestInfo* GetPendingRequestInfo(int request_id);

  // Follows redirect, if any, for the given request.
  void FollowPendingRedirect(int request_id, PendingRequestInfo& request_info);

  // Message response handlers, called by the message handler for this process.
  void OnUploadProgress(int request_id, int64 position, int64 size);
  void OnReceivedResponse(int request_id, const ResourceResponseHead&);
  void OnReceivedCachedMetadata(int request_id, const std::vector<char>& data);
  void OnReceivedRedirect(int request_id,
                          const GURL& new_url,
                          const GURL& new_first_party_for_cookies,
                          const ResourceResponseHead& response_head);
  void OnSetDataBuffer(int request_id,
                       base::SharedMemoryHandle shm_handle,
                       int shm_size,
                       base::ProcessId renderer_pid);
  void OnReceivedData(int request_id,
                      int data_offset,
                      int data_length,
                      int encoded_data_length);
  void OnDownloadedData(int request_id, int data_len, int encoded_data_length);
  void OnRequestComplete(
      int request_id,
      const ResourceMsg_RequestCompleteData& request_complete_data);

  // Dispatch the message to one of the message response handlers.
  void DispatchMessage(const IPC::Message& message);

  // Dispatch any deferred messages for the given request, provided it is not
  // again in the deferred state.
  void FlushDeferredMessages(int request_id);

  void ToResourceResponseInfo(const PendingRequestInfo& request_info,
                              const ResourceResponseHead& browser_info,
                              ResourceResponseInfo* renderer_info) const;

  base::TimeTicks ToRendererCompletionTime(
      const PendingRequestInfo& request_info,
      const base::TimeTicks& browser_completion_time) const;

  // Returns timestamp provided by IO thread. If no timestamp is supplied,
  // then current time is returned. Saved timestamp is reset, so following
  // invocations will return current time until set_io_timestamp is called.
  base::TimeTicks ConsumeIOTimestamp();

  // Returns true if the message passed in is a resource related message.
  static bool IsResourceDispatcherMessage(const IPC::Message& message);

  // ViewHostMsg_Resource_DataReceived is not POD, it has a shared memory
  // handle in it that we should cleanup it up nicely. This method accepts any
  // message and determine whether the message is
  // ViewHostMsg_Resource_DataReceived and clean up the shared memory handle.
  static void ReleaseResourcesInDataMessage(const IPC::Message& message);

  // Iterate through a message queue and clean up the messages by calling
  // ReleaseResourcesInDataMessage and removing them from the queue. Intended
  // for use on deferred message queues that are no longer needed.
  static void ReleaseResourcesInMessageQueue(MessageQueue* queue);

  IPC::Sender* message_sender_;

  // All pending requests issued to the host
  PendingRequestList pending_requests_;

  base::WeakPtrFactory<ResourceDispatcher> weak_factory_;

  ResourceDispatcherDelegate* delegate_;

  // IO thread timestamp for ongoing IPC message.
  base::TimeTicks io_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_RESOURCE_DISPATCHER_H_
