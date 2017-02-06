// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics_action.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"

using base::UserMetricsAction;

namespace extensions {

class WebViewGuest;
class WebViewPermissionHelperDelegate;

// WebViewPermissionHelper manages <webview> permission requests. This helper
// class is owned by WebViewGuest. Its purpose is to request permission for
// various operations from the <webview> embedder, and reply back via callbacks
// to the callers on a response from the embedder.
class WebViewPermissionHelper
      : public content::WebContentsObserver {
 public:
  explicit WebViewPermissionHelper(WebViewGuest* guest);
  ~WebViewPermissionHelper() override;
  typedef base::Callback<
      void(bool /* allow */, const std::string& /* user_input */)>
      PermissionResponseCallback;

  // A map to store the callback for a request keyed by the request's id.
  struct PermissionResponseInfo {
    PermissionResponseCallback callback;
    WebViewPermissionType permission_type;
    bool allowed_by_default;
    PermissionResponseInfo();
    PermissionResponseInfo(const PermissionResponseCallback& callback,
                           WebViewPermissionType permission_type,
                           bool allowed_by_default);
    PermissionResponseInfo(const PermissionResponseInfo& other);
    ~PermissionResponseInfo();
  };

  typedef std::map<int, PermissionResponseInfo> RequestMap;

  int RequestPermission(WebViewPermissionType permission_type,
                        const base::DictionaryValue& request_info,
                        const PermissionResponseCallback& callback,
                        bool allowed_by_default);

  static WebViewPermissionHelper* FromWebContents(
      content::WebContents* web_contents);
  static WebViewPermissionHelper* FromFrameID(int render_process_id,
                                              int render_frame_id);
  void RequestMediaAccessPermission(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback);
  bool CheckMediaAccessPermission(content::WebContents* source,
                                  const GURL& security_origin,
                                  content::MediaStreamType type);
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback);
  void RequestPointerLockPermission(bool user_gesture,
                                    bool last_unlocked_by_target,
                                    const base::Callback<void(bool)>& callback);

  // Requests Geolocation Permission from the embedder.
  void RequestGeolocationPermission(int bridge_id,
                                    const GURL& requesting_frame,
                                    const base::Callback<void(bool)>& callback);
  void CancelGeolocationPermissionRequest(int bridge_id);

  void RequestFileSystemPermission(const GURL& url,
                                   bool allowed_by_default,
                                   const base::Callback<void(bool)>& callback);

  // Called when file system access is requested by the guest content using the
  // asynchronous HTML5 file system API. The request is plumbed through the
  // <webview> permission request API. The request will be:
  // - Allowed if the embedder explicitly allowed it.
  // - Denied if the embedder explicitly denied.
  // - Determined by the guest's content settings if the embedder does not
  // perform an explicit action.
  // If access was blocked due to the page's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  void FileSystemAccessedAsync(int render_process_id,
                               int render_frame_id,
                               int request_id,
                               const GURL& url,
                               bool blocked_by_policy);

  // Called when file system access is requested by the guest content using the
  // synchronous HTML5 file system API in a worker thread or shared worker. The
  // request is plumbed through the <webview> permission request API. The
  // request will be:
  // - Allowed if the embedder explicitly allowed it.
  // - Denied if the embedder explicitly denied.
  // - Determined by the guest's content settings if the embedder does not
  // perform an explicit action.
  // If access was blocked due to the page's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  void FileSystemAccessedSync(int render_process_id,
                              int render_frame_id,
                              const GURL& url,
                              bool blocked_by_policy,
                              IPC::Message* reply_msg);

  enum PermissionResponseAction { DENY, ALLOW, DEFAULT };

  enum SetPermissionResult {
    SET_PERMISSION_INVALID,
    SET_PERMISSION_ALLOWED,
    SET_PERMISSION_DENIED
  };

  // Responds to the permission request |request_id| with |action| and
  // |user_input|. Returns whether there was a pending request for the provided
  // |request_id|.
  SetPermissionResult SetPermission(int request_id,
                                    PermissionResponseAction action,
                                    const std::string& user_input);

  void CancelPendingPermissionRequest(int request_id);

  WebViewGuest* web_view_guest() { return web_view_guest_; }

 private:
  void OnMediaPermissionResponse(const content::MediaStreamRequest& request,
                                 const content::MediaResponseCallback& callback,
                                 bool allow,
                                 const std::string& user_input);

#if defined(ENABLE_PLUGINS)
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message) override;
#endif  // defined(ENABLE_PLUGINS)

  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given WebViewGuest.
  int next_permission_request_id_;

  WebViewPermissionHelper::RequestMap pending_permission_requests_;

  std::unique_ptr<WebViewPermissionHelperDelegate>
      web_view_permission_helper_delegate_;

  WebViewGuest* const web_view_guest_;

  base::WeakPtrFactory<WebViewPermissionHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebViewPermissionHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
