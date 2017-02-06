// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_handle.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {

DownloadRequestHandleInterface::~DownloadRequestHandleInterface() {}

DownloadRequestHandle::DownloadRequestHandle(
    const DownloadRequestHandle& other) = default;

DownloadRequestHandle::~DownloadRequestHandle() {}

DownloadRequestHandle::DownloadRequestHandle() {}

DownloadRequestHandle::DownloadRequestHandle(
    const base::WeakPtr<DownloadResourceHandler>& handler,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter)
    : handler_(handler), web_contents_getter_(web_contents_getter) {
  DCHECK(handler_.get());
}

WebContents* DownloadRequestHandle::GetWebContents() const {
  return web_contents_getter_.is_null() ? nullptr : web_contents_getter_.Run();
}

DownloadManager* DownloadRequestHandle::GetDownloadManager() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents == nullptr)
    return nullptr;
  BrowserContext* context = web_contents->GetBrowserContext();
  if (context == nullptr)
    return nullptr;
  return BrowserContext::GetDownloadManager(context);
}

void DownloadRequestHandle::PauseRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::PauseRequest, handler_));
}

void DownloadRequestHandle::ResumeRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::ResumeRequest, handler_));
}

void DownloadRequestHandle::CancelRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::CancelRequest, handler_));
}

}  // namespace content
