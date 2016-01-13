// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The intent of this file is to provide a type-neutral abstraction between
// Chrome and WebKit for resource loading. This pure-virtual interface is
// implemented by the embedder.
//
// One of these objects will be created by WebKit for each request. WebKit
// will own the pointer to the bridge, and will delete it when the request is
// no longer needed.
//
// In turn, the bridge's owner on the WebKit end will implement the
// RequestPeer interface, which we will use to communicate notifications
// back.

#ifndef WEBKIT_CHILD_RESOURCE_LOADER_BRIDGE_H_
#define WEBKIT_CHILD_RESOURCE_LOADER_BRIDGE_H_

#include "base/macros.h"
#include "net/base/request_priority.h"
#include "webkit/child/webkit_child_export.h"

namespace blink {
class WebThreadedDataReceiver;
}

// TODO(pilgrim) remove this once resource loader is moved to content
// http://crbug.com/338338
namespace content {
class RequestPeer;
class ResourceRequestBody;
struct SyncLoadResponse;
}

namespace webkit_glue {

class ResourceLoaderBridge {
 public:
  // use WebKitPlatformSupportImpl::CreateResourceLoader() for construction, but
  // anybody can delete at any time, INCLUDING during processing of callbacks.
  WEBKIT_CHILD_EXPORT virtual ~ResourceLoaderBridge();

  // Call this method before calling Start() to set the request body.
  // May only be used with HTTP(S) POST requests.
  virtual void SetRequestBody(content::ResourceRequestBody* request_body) = 0;

  // Call this method to initiate the request.  If this method succeeds, then
  // the peer's methods will be called asynchronously to report various events.
  virtual bool Start(content::RequestPeer* peer) = 0;

  // Call this method to cancel a request that is in progress.  This method
  // causes the request to immediately transition into the 'done' state. The
  // OnCompletedRequest method will be called asynchronously; this assumes
  // the peer is still valid.
  virtual void Cancel() = 0;

  // Call this method to suspend or resume a load that is in progress.  This
  // method may only be called after a successful call to the Start method.
  virtual void SetDefersLoading(bool value) = 0;

  // Call this method when the priority of the requested resource changes after
  // Start() has been called.  This method may only be called after a successful
  // call to the Start method.
  virtual void DidChangePriority(net::RequestPriority new_priority,
                                 int intra_priority_value) = 0;

  // Call this method to attach a data receiver which will receive resource data
  // on its own thread.
  virtual bool AttachThreadedDataReceiver(
      blink::WebThreadedDataReceiver* threaded_data_receiver) = 0;

  // Call this method to load the resource synchronously (i.e., in one shot).
  // This is an alternative to the Start method.  Be warned that this method
  // will block the calling thread until the resource is fully downloaded or an
  // error occurs.  It could block the calling thread for a long time, so only
  // use this if you really need it!  There is also no way for the caller to
  // interrupt this method.  Errors are reported via the status field of the
  // response parameter.
  virtual void SyncLoad(content::SyncLoadResponse* response) = 0;

 protected:
  // Construction must go through
  // WebKitPlatformSupportImpl::CreateResourceLoader()
  // For HTTP(S) POST requests, the AppendDataToUpload and AppendFileToUpload
  // methods may be called to construct the body of the request.
  WEBKIT_CHILD_EXPORT ResourceLoaderBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_RESOURCE_LOADER_BRIDGE_H_
