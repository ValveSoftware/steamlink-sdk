// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include "base/logging.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace net {

int NetworkDelegate::NotifyBeforeURLRequest(
    URLRequest* request, const CompletionCallback& callback,
    GURL* new_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  DCHECK(!callback.is_null());
  return OnBeforeURLRequest(request, callback, new_url);
}

int NetworkDelegate::NotifyBeforeSendHeaders(
    URLRequest* request, const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
  DCHECK(CalledOnValidThread());
  DCHECK(headers);
  DCHECK(!callback.is_null());
  return OnBeforeSendHeaders(request, callback, headers);
}

void NetworkDelegate::NotifySendHeaders(URLRequest* request,
                                        const HttpRequestHeaders& headers) {
  DCHECK(CalledOnValidThread());
  OnSendHeaders(request, headers);
}

int NetworkDelegate::NotifyHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(original_response_headers);
  DCHECK(!callback.is_null());
  return OnHeadersReceived(request,
                           callback,
                           original_response_headers,
                           override_response_headers,
                           allowed_unsafe_redirect_url);
}

void NetworkDelegate::NotifyResponseStarted(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnResponseStarted(request);
}

void NetworkDelegate::NotifyRawBytesRead(const URLRequest& request,
                                         int bytes_read) {
  DCHECK(CalledOnValidThread());
  OnRawBytesRead(request, bytes_read);
}

void NetworkDelegate::NotifyBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnBeforeRedirect(request, new_location);
}

void NetworkDelegate::NotifyCompleted(URLRequest* request, bool started) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnCompleted(request, started);
}

void NetworkDelegate::NotifyURLRequestDestroyed(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnURLRequestDestroyed(request);
}

void NetworkDelegate::NotifyPACScriptError(int line_number,
                                           const base::string16& error) {
  DCHECK(CalledOnValidThread());
  OnPACScriptError(line_number, error);
}

NetworkDelegate::AuthRequiredResponse NetworkDelegate::NotifyAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  DCHECK(CalledOnValidThread());
  return OnAuthRequired(request, auth_info, callback, credentials);
}

int NetworkDelegate::NotifyBeforeSocketStreamConnect(
    SocketStream* socket,
    const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(!callback.is_null());
  return OnBeforeSocketStreamConnect(socket, callback);
}

bool NetworkDelegate::CanGetCookies(const URLRequest& request,
                                    const CookieList& cookie_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(!(request.load_flags() & net::LOAD_DO_NOT_SEND_COOKIES));
  return OnCanGetCookies(request, cookie_list);
}

bool NetworkDelegate::CanSetCookie(const URLRequest& request,
                                   const std::string& cookie_line,
                                   CookieOptions* options) {
  DCHECK(CalledOnValidThread());
  DCHECK(!(request.load_flags() & net::LOAD_DO_NOT_SAVE_COOKIES));
  return OnCanSetCookie(request, cookie_line, options);
}

bool NetworkDelegate::CanAccessFile(const URLRequest& request,
                                    const base::FilePath& path) const {
  DCHECK(CalledOnValidThread());
  return OnCanAccessFile(request, path);
}

bool NetworkDelegate::CanThrottleRequest(const URLRequest& request) const {
  DCHECK(CalledOnValidThread());
  return OnCanThrottleRequest(request);
}

bool NetworkDelegate::CanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  DCHECK(CalledOnValidThread());
  return OnCanEnablePrivacyMode(url, first_party_for_cookies);
}

int NetworkDelegate::OnBeforeURLRequest(URLRequest* request,
                                        const CompletionCallback& callback,
                                        GURL* new_url) {
  return OK;
}

int NetworkDelegate::OnBeforeSendHeaders(URLRequest* request,
                                         const CompletionCallback& callback,
                                         HttpRequestHeaders* headers) {
  return OK;
}

void NetworkDelegate::OnSendHeaders(URLRequest* request,
                                    const HttpRequestHeaders& headers) {
}

int NetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return OK;
}

void NetworkDelegate::OnBeforeRedirect(URLRequest* request,
                                       const GURL& new_location) {
}

void NetworkDelegate::OnResponseStarted(URLRequest* request) {
}

void NetworkDelegate::OnRawBytesRead(const URLRequest& request,
                                     int bytes_read) {
}

void NetworkDelegate::OnCompleted(URLRequest* request, bool started) {
}

void NetworkDelegate::OnURLRequestDestroyed(URLRequest* request) {
}

void NetworkDelegate::OnPACScriptError(int line_number,
                                       const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse NetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegate::OnCanGetCookies(const URLRequest& request,
                                      const CookieList& cookie_list)  {
  return true;
}

bool NetworkDelegate::OnCanSetCookie(const URLRequest& request,
                                     const std::string& cookie_line,
                                     CookieOptions* options) {
  return true;
}

bool NetworkDelegate::OnCanAccessFile(const URLRequest& request,
                                      const base::FilePath& path) const  {
  return false;
}

bool NetworkDelegate::OnCanThrottleRequest(const URLRequest& request) const {
  return false;
}

bool NetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  return false;
}

int NetworkDelegate::OnBeforeSocketStreamConnect(
    SocketStream* socket,
    const CompletionCallback& callback) {
  return OK;
}

}  // namespace net
