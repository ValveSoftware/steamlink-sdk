// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_network_delegate.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "net/url_request/url_request.h"

namespace extensions {

namespace {
bool g_accept_all_cookies = true;
}

ShellNetworkDelegate::ShellNetworkDelegate(
      void* browser_context, InfoMap* extension_info_map) {
  browser_context_ = browser_context;
  extension_info_map_ = extension_info_map;
}

ShellNetworkDelegate::~ShellNetworkDelegate() {}

void ShellNetworkDelegate::SetAcceptAllCookies(bool accept) {
  g_accept_all_cookies = accept;
}

int ShellNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      browser_context_, extension_info_map_.get(), request, callback, new_url);
}

int ShellNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      browser_context_, extension_info_map_.get(), request, callback, headers);
}

void ShellNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      browser_context_, extension_info_map_.get(), request, headers);
}

int ShellNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      browser_context_,
      extension_info_map_.get(),
      request,
      callback,
      original_response_headers,
      override_response_headers,
      allowed_unsafe_redirect_url);
}

void ShellNetworkDelegate::OnBeforeRedirect(
    net::URLRequest* request,
    const GURL& new_location) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      browser_context_, extension_info_map_.get(), request, new_location);
}


void ShellNetworkDelegate::OnResponseStarted(
    net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      browser_context_, extension_info_map_.get(), request);
}

void ShellNetworkDelegate::OnCompleted(
    net::URLRequest* request,
    bool started) {
  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    bool is_redirect = request->response_headers() &&
        net::HttpResponseHeaders::IsRedirectResponseCode(
            request->response_headers()->response_code());
    if (!is_redirect) {
      ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
          browser_context_, extension_info_map_.get(), request);
    }
    return;
  }

  if (request->status().status() == net::URLRequestStatus::FAILED ||
      request->status().status() == net::URLRequestStatus::CANCELED) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        browser_context_, extension_info_map_.get(), request, started);
    return;
  }

  NOTREACHED();
}

void ShellNetworkDelegate::OnURLRequestDestroyed(
    net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      browser_context_, request);
}

void ShellNetworkDelegate::OnPACScriptError(
    int line_number,
    const base::string16& error) {
}

net::NetworkDelegate::AuthRequiredResponse
ShellNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      browser_context_, extension_info_map_.get(), request, auth_info, callback,
      credentials);
}

}  // namespace extensions
