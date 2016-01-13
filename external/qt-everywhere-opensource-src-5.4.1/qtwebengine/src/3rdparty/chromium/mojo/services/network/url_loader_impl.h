// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_
#define MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace mojo {
class NetworkContext;

class URLLoaderImpl : public InterfaceImpl<URLLoader>,
                      public net::URLRequest::Delegate {
 public:
  explicit URLLoaderImpl(NetworkContext* context);
  virtual ~URLLoaderImpl();

 private:
  class PendingWriteToDataPipe;
  class DependentIOBuffer;

  // InterfaceImpl<> methods:
  virtual void OnConnectionError() OVERRIDE;

  // URLLoader methods:
  virtual void Start(
      URLRequestPtr request,
      ScopedDataPipeProducerHandle response_body_stream) OVERRIDE;
  virtual void FollowRedirect() OVERRIDE;

  // net::URLRequest::Delegate methods:
  virtual void OnReceivedRedirect(net::URLRequest* url_request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* url_request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* url_request, int bytes_read)
      OVERRIDE;

  void SendError(int error);
  void ReadMore();
  void DidRead(uint32_t num_bytes, bool completed_synchronously);

  NetworkContext* context_;
  scoped_ptr<net::URLRequest> url_request_;
  ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<PendingWriteToDataPipe> pending_write_;
  bool auto_follow_redirects_;

  base::WeakPtrFactory<URLLoaderImpl> weak_ptr_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_
