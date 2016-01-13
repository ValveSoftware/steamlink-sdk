// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/network_delegate_error_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestNetworkDelegate : public net::NetworkDelegate {
 public:
  TestNetworkDelegate() : got_pac_error_(false) {}
  virtual ~TestNetworkDelegate() {}

  bool got_pac_error() const { return got_pac_error_; }

 private:
  // net::NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    return OK;
  }
  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) OVERRIDE {
    return OK;
  }
  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) OVERRIDE {}
  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE {
    return net::OK;
  }
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) OVERRIDE {}
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE {}
  virtual void OnRawBytesRead(const URLRequest& request,
                              int bytes_read) OVERRIDE {}
  virtual void OnCompleted(URLRequest* request, bool started) OVERRIDE {}
  virtual void OnURLRequestDestroyed(URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE {
    got_pac_error_ = true;
  }
  virtual AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) OVERRIDE {
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }
  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list) OVERRIDE {
    return true;
  }
  virtual bool OnCanSetCookie(const URLRequest& request,
                              const std::string& cookie_line,
                              CookieOptions* options) OVERRIDE {
    return true;
  }
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE {
    return true;
  }
  virtual bool OnCanThrottleRequest(const URLRequest& request) const OVERRIDE {
    return false;
  }
  virtual int OnBeforeSocketStreamConnect(
      SocketStream* stream,
      const CompletionCallback& callback) OVERRIDE {
    return OK;
  }

  bool got_pac_error_;
};

}  // namespace

// Check that the OnPACScriptError method can be called from an arbitrary
// thread.
TEST(NetworkDelegateErrorObserverTest, CallOnThread) {
  base::Thread thread("test_thread");
  thread.Start();
  TestNetworkDelegate network_delegate;
  NetworkDelegateErrorObserver observer(
      &network_delegate, base::MessageLoopProxy::current().get());
  thread.message_loop()
      ->PostTask(FROM_HERE,
                 base::Bind(&NetworkDelegateErrorObserver::OnPACScriptError,
                            base::Unretained(&observer),
                            42,
                            base::string16()));
  thread.Stop();
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_TRUE(network_delegate.got_pac_error());
}

// Check that passing a NULL network delegate works.
TEST(NetworkDelegateErrorObserverTest, NoDelegate) {
  base::Thread thread("test_thread");
  thread.Start();
  NetworkDelegateErrorObserver observer(
      NULL, base::MessageLoopProxy::current().get());
  thread.message_loop()
      ->PostTask(FROM_HERE,
                 base::Bind(&NetworkDelegateErrorObserver::OnPACScriptError,
                            base::Unretained(&observer),
                            42,
                            base::string16()));
  thread.Stop();
  base::MessageLoop::current()->RunUntilIdle();
  // Shouldn't have crashed until here...
}

}  // namespace net
