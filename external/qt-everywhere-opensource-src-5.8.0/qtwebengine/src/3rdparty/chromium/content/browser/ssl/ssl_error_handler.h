// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace net {
class SSLInfo;
class URLRequest;
}  // namespace net

namespace content {

class ResourceDispatcherHostImpl;
class SSLCertErrorHandler;
class SSLManager;
class WebContents;

// An SSLErrorHandler carries information from the IO thread to the UI thread
// and is dispatched to the appropriate SSLManager when it arrives on the
// UI thread.  Subclasses should override the OnDispatched/OnDispatchFailed
// methods to implement the actions that should be taken on the UI thread.
// These methods can call the different convenience methods ContinueRequest/
// CancelRequest to perform any required action on the net::URLRequest the
// ErrorHandler was created with.
//
// IMPORTANT NOTE:
//
//   If you are not doing anything in OnDispatched/OnDispatchFailed, make sure
//   you call TakeNoAction().  This is necessary for ensuring the instance is
//   not leaked.
//
class SSLErrorHandler : public base::RefCountedThreadSafe<SSLErrorHandler> {
 public:
  // Delegate functions must be called from IO thread. Finally,
  // CancelSSLRequest() or ContinueSSLRequest() will be called after
  // SSLErrorHandler makes a decision on the SSL error.
  class CONTENT_EXPORT Delegate {
   public:
    // Called when SSLErrorHandler decides to cancel the request because of
    // the SSL error.
    virtual void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) = 0;

    // Called when SSLErrorHandler decides to continue the request despite the
    // SSL error.
    virtual void ContinueSSLRequest() = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual SSLCertErrorHandler* AsSSLCertErrorHandler();

  // Find the appropriate SSLManager for the net::URLRequest and begin handling
  // this error.
  //
  // Call on UI thread.
  void Dispatch(const base::Callback<WebContents*(void)>& web_contents_getter);

  // Available on either thread.
  const GURL& request_url() const { return request_url_; }

  // Available on either thread.
  ResourceType resource_type() const { return resource_type_; }

  // Cancels the associated net::URLRequest.
  // This method can be called from OnDispatchFailed and OnDispatched.
  CONTENT_EXPORT void CancelRequest();

  // Continue the net::URLRequest ignoring any previous errors.  Note that some
  // errors cannot be ignored, in which case this will result in the request
  // being canceled.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void ContinueRequest();

  // Cancels the associated net::URLRequest and mark it as denied.  The renderer
  // processes such request in a special manner, optionally replacing them
  // with alternate content (typically frames content is replaced with a
  // warning message).
  // This method can be called from OnDispatchFailed and OnDispatched.
  void DenyRequest();

  // Does nothing on the net::URLRequest but ensures the current instance ref
  // count is decremented appropriately.  Subclasses that do not want to
  // take any specific actions in their OnDispatched/OnDispatchFailed should
  // call this.
  void TakeNoAction();

  // Returns the manager associated with this SSLErrorHandler.
  // Should only be accessed on the UI thread.
  SSLManager* GetManager() const;

 protected:
  friend class base::RefCountedThreadSafe<SSLErrorHandler>;

  // Construct on the IO thread.
  SSLErrorHandler(const base::WeakPtr<Delegate>& delegate,
                  ResourceType resource_type,
                  const GURL& url);

  virtual ~SSLErrorHandler();

  // The following 2 methods are the methods subclasses should implement.
  virtual void OnDispatchFailed();

  // Can use the manager_ member.
  virtual void OnDispatched();

  // Should only be accessed on the UI thread.
  SSLManager* manager_;  // Our manager.

  // The delegate we are associated with.
  base::WeakPtr<Delegate> delegate_;

 private:
  // Completes the CancelRequest operation on the IO thread.
  // Call on the IO thread.
  void CompleteCancelRequest(int error);

  // Completes the ContinueRequest operation on the IO thread.
  //
  // Call on the IO thread.
  void CompleteContinueRequest();

  // Derefs this instance.
  // Call on the IO thread.
  void CompleteTakeNoAction();

  // The URL that we requested.
  // This read-only member can be accessed on any thread.
  const GURL request_url_;

  // What kind of resource is associated with the requested that generated
  // that error.
  // This read-only member can be accessed on any thread.
  const ResourceType resource_type_;

  // A flag to make sure we notify the net::URLRequest exactly once.
  // Should only be accessed on the IO thread
  bool request_has_been_notified_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_ERROR_HANDLER_H_
