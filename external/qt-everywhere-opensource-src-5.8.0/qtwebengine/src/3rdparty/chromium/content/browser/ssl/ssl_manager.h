// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#define CONTENT_BROWSER_SSL_SSL_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_policy_backend.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"

namespace net {
class SSLInfo;
}

namespace content {
class BrowserContext;
class NavigationEntryImpl;
class NavigationControllerImpl;
class SSLPolicy;
struct LoadCommittedDetails;
struct LoadFromMemoryCacheDetails;
struct ResourceRedirectDetails;
struct ResourceRequestDetails;
struct SSLStatus;

// The SSLManager SSLManager controls the SSL UI elements in a WebContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class CONTENT_EXPORT SSLManager {
 public:
  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |CancelSSLRequest| or
  // |ContinueSSLRequest| of |delegate|.
  //
  // Called on the IO thread.
  static void OnSSLCertificateError(
      const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
      ResourceType resource_type,
      const GURL& url,
      const base::Callback<WebContents*(void)>& web_contents_getter,
      const net::SSLInfo& ssl_info,
      bool fatal);

  // Same as the above, and only works for subresources. Prefer using
  // OnSSLCertificateError whenever possible (ie when you have access to the
  // WebContents).
  static void OnSSLCertificateSubresourceError(
      const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
      const GURL& url,
      int render_process_id,
      int render_frame_id,
      const net::SSLInfo& ssl_info,
      bool fatal);

  // Called when SSL state for a host or tab changes.
  static void NotifySSLInternalStateChanged(BrowserContext* context);

  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  explicit SSLManager(NavigationControllerImpl* controller);
  virtual ~SSLManager();

  SSLPolicy* policy() { return policy_.get(); }
  SSLPolicyBackend* backend() { return &backend_; }

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationControllerImpl* controller() { return controller_; }

  void DidCommitProvisionalLoad(const LoadCommittedDetails& details);
  void DidLoadFromMemoryCache(const LoadFromMemoryCacheDetails& details);
  void DidStartResourceResponse(const ResourceRequestDetails& details);
  void DidReceiveResourceRedirect(const ResourceRedirectDetails& details);

  // Insecure content entry point.
  void DidRunInsecureContent(const GURL& security_origin);

 private:
  // Updates the NavigationEntry with our current state. This will
  // notify the WebContents of an SSL state change if a change was
  // actually made.
  void UpdateEntry(NavigationEntryImpl* entry);

  // Notifies the WebContents that the SSL state changed.
  void NotifyDidChangeVisibleSSLState();

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The SSLPolicy instance for this manager.
  std::unique_ptr<SSLPolicy> policy_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_MANAGER_H_
