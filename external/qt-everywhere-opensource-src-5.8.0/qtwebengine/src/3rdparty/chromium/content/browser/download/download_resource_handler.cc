// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_response.h"

namespace content {

struct DownloadResourceHandler::DownloadTabInfo {
  GURL tab_url;
  GURL tab_referrer_url;
};

namespace {

// Static function in order to prevent any accidental accesses to
// DownloadResourceHandler members from the UI thread.
static void StartOnUIThread(
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info,
    std::unique_ptr<ByteStreamReader> stream,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    const DownloadUrlParameters::OnStartedCallback& started_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);

  // PlzNavigate: navigations don't have associated RenderFrameHosts. Get the
  // SiteInstance from the FrameTreeNode.
  if (!frame_host && IsBrowserSideNavigationEnabled()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    if (frame_tree_node)
      frame_host = frame_tree_node->current_frame_host();
  }

  DownloadManager* download_manager =
      info->request_handle->GetDownloadManager();
  if (!download_manager || !frame_host) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    if (!started_cb.is_null())
      started_cb.Run(nullptr, DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

    if (stream)
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                                stream.release());
    return;
  }

  info->tab_url = tab_info->tab_url;
  info->tab_referrer_url = tab_info->tab_referrer_url;
  info->site_url = frame_host->GetSiteInstance()->GetSiteURL();

  download_manager->StartDownload(std::move(info), std::move(stream),
                                  started_cb);
}

void InitializeDownloadTabInfoOnUIThread(
    const DownloadRequestHandle& request_handle,
    DownloadResourceHandler::DownloadTabInfo* tab_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = request_handle.GetWebContents();
  if (web_contents) {
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_info->tab_url = entry->GetURL();
      tab_info->tab_referrer_url = entry->GetReferrer().url;
    }
  }
}

void DeleteOnUIThread(
    std::unique_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info) {}

}  // namespace

DownloadResourceHandler::DownloadResourceHandler(net::URLRequest* request)
    : ResourceHandler(request),
      tab_info_(new DownloadTabInfo()),
      core_(request, this) {
  // Do UI thread initialization for tab_info_ asap after
  // DownloadResourceHandler creation since the tab could be navigated
  // before StartOnUIThread gets called.  This is safe because deletion
  // will occur via PostTask() as well, which will serialized behind this
  // PostTask()
  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &InitializeDownloadTabInfoOnUIThread,
          DownloadRequestHandle(AsWeakPtr(),
                                request_info->GetWebContentsGetterForRequest()),
          tab_info_.get()));
}

DownloadResourceHandler::~DownloadResourceHandler() {
  if (tab_info_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DeleteOnUIThread, base::Passed(&tab_info_)));
  }
}

bool DownloadResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  return core_.OnRequestRedirected();
}

// Send the download creation information to the download thread.
bool DownloadResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  // The MIME type in ResourceResponse is the product of
  // MimeTypeResourceHandler.
  return core_.OnResponseStarted(response->head.mime_type);
}

bool DownloadResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool DownloadResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                   bool* defer) {
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                         int* buf_size,
                                         int min_size) {
  return core_.OnWillRead(buf, buf_size, min_size);
}

// Pass the buffer to the download file writer.
bool DownloadResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  return core_.OnReadCompleted(bytes_read, defer);
}

void DownloadResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  core_.OnResponseCompleted(status);
}

void DownloadResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void DownloadResourceHandler::PauseRequest() {
  core_.PauseRequest();
}

void DownloadResourceHandler::ResumeRequest() {
  core_.ResumeRequest();
}

void DownloadResourceHandler::OnStart(
    std::unique_ptr<DownloadCreateInfo> create_info,
    std::unique_ptr<ByteStreamReader> stream_reader,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  // If the user cancels the download, then don't call start. Instead ignore the
  // download entirely.
  if (create_info->result == DOWNLOAD_INTERRUPT_REASON_USER_CANCELED &&
      create_info->download_id == DownloadItem::kInvalidId) {
    if (!callback.is_null())
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(callback, nullptr, create_info->result));
    return;
  }

  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  create_info->has_user_gesture = request_info->HasUserGesture();
  create_info->transition_type = request_info->GetPageTransition();

  create_info->request_handle.reset(new DownloadRequestHandle(
      AsWeakPtr(), request_info->GetWebContentsGetterForRequest()));

  int render_process_id = -1;
  int render_frame_id = -1;
  request_info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StartOnUIThread, base::Passed(&create_info),
                 base::Passed(&tab_info_), base::Passed(&stream_reader),
                 render_process_id, render_frame_id,
                 request_info->frame_tree_node_id(), callback));
}

void DownloadResourceHandler::OnReadyToRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller()->Resume();
}

void DownloadResourceHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const ResourceRequestInfoImpl* info = GetRequestInfo();
  ResourceDispatcherHostImpl::Get()->CancelRequest(
      info->GetChildID(),
      info->GetRequestID());
  // This object has been deleted.
}

std::string DownloadResourceHandler::DebugString() const {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  return base::StringPrintf("{"
                            " url_ = " "\"%s\""
                            " info = {"
                            " child_id = " "%d"
                            " request_id = " "%d"
                            " route_id = " "%d"
                            " }"
                            " }",
                            request() ?
                                request()->url().spec().c_str() :
                                "<NULL request>",
                            info->GetChildID(),
                            info->GetRequestID(),
                            info->GetRouteID());
}

}  // namespace content
