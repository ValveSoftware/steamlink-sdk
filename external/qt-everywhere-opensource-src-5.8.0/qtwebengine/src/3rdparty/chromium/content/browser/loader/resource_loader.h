// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"
#include "net/url_request/url_request.h"

namespace net {
class X509Certificate;
}

namespace content {
class CertStore;
class ResourceDispatcherHostLoginDelegate;
class ResourceLoaderDelegate;
class ResourceRequestInfoImpl;

// This class is responsible for driving the URLRequest (i.e., calling Start,
// Read, and servicing events).  It has a ResourceHandler, which is typically a
// chain of ResourceHandlers, and is the ResourceController for its handler.
class CONTENT_EXPORT ResourceLoader : public net::URLRequest::Delegate,
                                      public SSLErrorHandler::Delegate,
                                      public SSLClientAuthHandler::Delegate,
                                      public ResourceController {
 public:
  ResourceLoader(std::unique_ptr<net::URLRequest> request,
                 std::unique_ptr<ResourceHandler> handler,
                 CertStore* cert_store,
                 ResourceLoaderDelegate* delegate);
  ~ResourceLoader() override;

  void StartRequest();
  void CancelRequest(bool from_renderer);

  bool is_transferring() const { return is_transferring_; }
  void MarkAsTransferring(const scoped_refptr<ResourceResponse>& response);
  void CompleteTransfer();

  net::URLRequest* request() { return request_.get(); }
  ResourceRequestInfoImpl* GetRequestInfo();

  void ClearLoginDelegate();

  // Returns a pointer to the ResourceResponse for a request that is
  // being transferred to a new consumer. The response is valid between
  // the time that the request is marked as transferring via
  // MarkAsTransferring() and the time that the transfer is completed
  // via CompleteTransfer().
  ResourceResponse* transferring_response() {
    return transferring_response_.get();
  }

 private:
  // net::URLRequest::Delegate implementation:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* info) override;
  void OnCertificateRequested(net::URLRequest* request,
                              net::SSLCertRequestInfo* info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& info,
                             bool fatal) override;
  void OnBeforeNetworkStart(net::URLRequest* request, bool* defer) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // SSLErrorHandler::Delegate implementation:
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override;
  void ContinueSSLRequest() override;

  // SSLClientAuthHandler::Delegate implementation.
  void ContinueWithCertificate(net::X509Certificate* cert) override;
  void CancelCertificateSelection() override;

  // ResourceController implementation:
  void Resume() override;
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;

  void StartRequestInternal();
  void CancelRequestInternal(int error, bool from_renderer);
  void CompleteResponseStarted();
  void StartReading(bool is_continuation);
  void ResumeReading();
  void ReadMore(int* bytes_read);
  // Passes a read result to the handler.
  void CompleteRead(int bytes_read);
  void ResponseCompleted();
  void CallDidFinishLoading();
  void RecordHistograms();

  bool is_deferred() const { return deferred_stage_ != DEFERRED_NONE; }

  // Used for categorizing loading of prefetches for reporting in histograms.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum PrefetchStatus {
    STATUS_UNDEFINED,
    STATUS_SUCCESS_FROM_CACHE,
    STATUS_SUCCESS_FROM_NETWORK,
    STATUS_CANCELED,
    STATUS_MAX,
  };

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_NETWORK_START,
    DEFERRED_REDIRECT,
    DEFERRED_READ,
    DEFERRED_RESPONSE_COMPLETE,
    DEFERRED_FINISH
  };
  DeferredStage deferred_stage_;

  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<ResourceHandler> handler_;
  ResourceLoaderDelegate* delegate_;

  scoped_refptr<ResourceDispatcherHostLoginDelegate> login_delegate_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;

  base::TimeTicks read_deferral_start_time_;

  // Indicates that we are in a state of being transferred to a new downstream
  // consumer.  We are waiting for a notification to complete the transfer, at
  // which point we'll receive a new ResourceHandler.
  bool is_transferring_;

  // Holds the ResourceResponse for a request that is being transferred
  // to a new consumer. This member is populated when the request is
  // marked as transferring via MarkAsTransferring(), and it is cleared
  // when the transfer is completed via CompleteTransfer().
  scoped_refptr<ResourceResponse> transferring_response_;

  // Instrumentation add to investigate http://crbug.com/503306.
  // TODO(mmenke): Remove once bug is fixed.
  int times_cancelled_before_request_start_;
  bool started_request_;
  int times_cancelled_after_request_start_;

  // Allows tests to use a mock CertStore. The CertStore must outlive
  // the ResourceLoader.
  CertStore* cert_store_;

  base::WeakPtrFactory<ResourceLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
