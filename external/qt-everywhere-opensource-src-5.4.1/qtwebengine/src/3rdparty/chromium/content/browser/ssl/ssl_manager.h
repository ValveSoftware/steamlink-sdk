// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#define CONTENT_BROWSER_SSL_SSL_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
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

// The SSLManager SSLManager controls the SSL UI elements in a WebContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class SSLManager {
 public:
  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |CancelSSLRequest| or
  // |ContinueSSLRequest| of |delegate| with |id| as the first argument.
  //
  // Called on the IO thread.
  static void OnSSLCertificateError(
      const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
      const GlobalRequestID& id,
      ResourceType::Type resource_type,
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
  void DidDisplayInsecureContent();
  void DidRunInsecureContent(const std::string& security_origin);

 private:
  // Update the NavigationEntry with our current state.
  void UpdateEntry(NavigationEntryImpl* entry);

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The SSLPolicy instance for this manager.
  scoped_ptr<SSLPolicy> policy_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_MANAGER_H_
