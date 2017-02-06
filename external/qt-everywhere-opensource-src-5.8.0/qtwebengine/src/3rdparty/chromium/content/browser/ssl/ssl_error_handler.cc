// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_error_handler.h"

#include "base/bind.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

using net::SSLInfo;

namespace content {

SSLErrorHandler::SSLErrorHandler(const base::WeakPtr<Delegate>& delegate,
                                 ResourceType resource_type,
                                 const GURL& url)
    : manager_(NULL),
      delegate_(delegate),
      request_url_(url),
      resource_type_(resource_type),
      request_has_been_notified_(false) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(delegate.get());

  // This makes sure we don't disappear on the IO thread until we've given an
  // answer to the net::URLRequest.
  //
  // Release in CompleteCancelRequest, CompleteContinueRequest, or
  // CompleteTakeNoAction.
  AddRef();
}

SSLErrorHandler::~SSLErrorHandler() {}

void SSLErrorHandler::OnDispatchFailed() {
  TakeNoAction();
}

void SSLErrorHandler::OnDispatched() {
  TakeNoAction();
}

SSLCertErrorHandler* SSLErrorHandler::AsSSLCertErrorHandler() {
  return NULL;
}

void SSLErrorHandler::Dispatch(
    const base::Callback<WebContents*(void)>& web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();

  if (!web_contents) {
    // We arrived on the UI thread, but the tab we're looking for is no longer
    // here.
    OnDispatchFailed();
    return;
  }

  // Hand ourselves off to the SSLManager.
  manager_ =
      static_cast<NavigationControllerImpl*>(&web_contents->GetController())->
          ssl_manager();
  OnDispatched();
}

void SSLErrorHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SSLErrorHandler::CompleteCancelRequest, this, net::ERR_ABORTED));
}

void SSLErrorHandler::DenyRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SSLErrorHandler::CompleteCancelRequest, this,
          net::ERR_INSECURE_RESPONSE));
}

void SSLErrorHandler::ContinueRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SSLErrorHandler::CompleteContinueRequest, this));
}

void SSLErrorHandler::TakeNoAction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SSLErrorHandler::CompleteTakeNoAction, this));
}

SSLManager* SSLErrorHandler::GetManager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return manager_;
}

void SSLErrorHandler::CompleteCancelRequest(int error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It is important that we notify the net::URLRequest only once.  If we try
  // to notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  SSLCertErrorHandler* cert_error = AsSSLCertErrorHandler();
  const SSLInfo* ssl_info = NULL;
  if (cert_error)
    ssl_info = &cert_error->ssl_info();
  if (delegate_.get())
    delegate_->CancelSSLRequest(error, ssl_info);
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteContinueRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It is important that we notify the net::URLRequest only once. If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  if (delegate_.get())
    delegate_->ContinueSSLRequest();
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteTakeNoAction() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It is important that we notify the net::URLRequest only once. If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

}  // namespace content
