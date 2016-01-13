// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "net/url_request/url_request.h"

namespace content {
class ResourceDispatcherHostLoginDelegate;
class ResourceLoaderDelegate;
class ResourceRequestInfoImpl;
class SSLClientAuthHandler;

// This class is responsible for driving the URLRequest (i.e., calling Start,
// Read, and servicing events).  It has a ResourceHandler, which is typically a
// chain of ResourceHandlers, and is the ResourceController for its handler.
class CONTENT_EXPORT ResourceLoader : public net::URLRequest::Delegate,
                                      public SSLErrorHandler::Delegate,
                                      public ResourceController {
 public:
  ResourceLoader(scoped_ptr<net::URLRequest> request,
                 scoped_ptr<ResourceHandler> handler,
                 ResourceLoaderDelegate* delegate);
  virtual ~ResourceLoader();

  void StartRequest();
  void CancelRequest(bool from_renderer);

  void ReportUploadProgress();

  bool is_transferring() const { return is_transferring_; }
  void MarkAsTransferring();
  void CompleteTransfer();

  net::URLRequest* request() { return request_.get(); }
  ResourceRequestInfoImpl* GetRequestInfo();

  void ClearLoginDelegate();
  void ClearSSLClientAuthHandler();

  // IPC message handlers:
  void OnUploadProgressACK();

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceLoaderTest, ClientCertStoreLookup);
  FRIEND_TEST_ALL_PREFIXES(ResourceLoaderTest, ClientCertStoreNull);

  // net::URLRequest::Delegate implementation:
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer) OVERRIDE;
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* info) OVERRIDE;
  virtual void OnCertificateRequested(net::URLRequest* request,
                                      net::SSLCertRequestInfo* info) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& info,
                                     bool fatal) OVERRIDE;
  virtual void OnBeforeNetworkStart(net::URLRequest* request,
                                    bool* defer) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  // SSLErrorHandler::Delegate implementation:
  virtual void CancelSSLRequest(const GlobalRequestID& id,
                                int error,
                                const net::SSLInfo* ssl_info) OVERRIDE;
  virtual void ContinueSSLRequest(const GlobalRequestID& id) OVERRIDE;

  // ResourceController implementation:
  virtual void Resume() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void CancelAndIgnore() OVERRIDE;
  virtual void CancelWithError(int error_code) OVERRIDE;

  void StartRequestInternal();
  void CancelRequestInternal(int error, bool from_renderer);
  // Stores the SignedCertificateTimestamps held in |sct_list| in the
  // SignedCertificateTimestampStore singleton, associated with |process_id|.
  // On return, |sct_ids| contains the assigned ID and verification status of
  // each SignedCertificateTimestamp.
  void StoreSignedCertificateTimestamps(
      const net::SignedCertificateTimestampAndStatusList& sct_list,
      int process_id,
      SignedCertificateTimestampIDStatusList* sct_ids);
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

  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<ResourceHandler> handler_;
  ResourceLoaderDelegate* delegate_;

  scoped_refptr<ResourceDispatcherHostLoginDelegate> login_delegate_;
  scoped_refptr<SSLClientAuthHandler> ssl_client_auth_handler_;

  uint64 last_upload_position_;
  bool waiting_for_upload_progress_ack_;
  base::TimeTicks last_upload_ticks_;
  base::TimeTicks read_deferral_start_time_;

  // Indicates that we are in a state of being transferred to a new downstream
  // consumer.  We are waiting for a notification to complete the transfer, at
  // which point we'll receive a new ResourceHandler.
  bool is_transferring_;

  base::WeakPtrFactory<ResourceLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
