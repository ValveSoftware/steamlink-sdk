// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_TEST_URL_LOADER_CLIENT_H_
#define CONTENT_BROWSER_LOADER_TEST_URL_LOADER_CLIENT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/resource_request_completion_status.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_response.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/url_request/redirect_info.h"

namespace content {

// A TestURLLoaderClient records URLLoaderClient function calls. It also calls
// the closure set via set_quit_closure if set, in order to make it possible to
// create a base::RunLoop, set its quit closure to this client and then run the
// RunLoop.
class TestURLLoaderClient final : public mojom::URLLoaderClient {
 public:
  TestURLLoaderClient();
  ~TestURLLoaderClient() override;

  void OnReceiveResponse(const ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

  bool has_received_response() const { return has_received_response_; }
  bool has_received_redirect() const { return has_received_redirect_; }
  bool has_data_downloaded() const { return has_data_downloaded_; }
  bool has_received_completion() const { return has_received_completion_; }
  const ResourceResponseHead& response_head() const { return response_head_; }
  const net::RedirectInfo& redirect_info() const { return redirect_info_; }
  mojo::DataPipeConsumerHandle response_body() { return response_body_.get(); }
  const ResourceRequestCompletionStatus& completion_status() const {
    return completion_status_;
  }
  int64_t download_data_length() const { return download_data_length_; }
  int64_t encoded_download_data_length() const {
    return encoded_download_data_length_;
  }

  void ClearHasReceivedRedirect();
  // Creates an AssociatedPtrInfo, binds it to |*this| and returns it. The
  // returned PtrInfo is marked as remote, i.e., expected to be passed to the
  // remote endpoint.
  mojom::URLLoaderClientAssociatedPtrInfo CreateRemoteAssociatedPtrInfo(
      mojo::AssociatedGroup* associated_group);

  void Unbind();

  void RunUntilResponseReceived();
  void RunUntilRedirectReceived();
  void RunUntilDataDownloaded();
  void RunUntilResponseBodyArrived();
  void RunUntilComplete();

 private:
  mojo::AssociatedBinding<mojom::URLLoaderClient> binding_;
  ResourceResponseHead response_head_;
  net::RedirectInfo redirect_info_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  ResourceRequestCompletionStatus completion_status_;
  bool has_received_response_ = false;
  bool has_received_redirect_ = false;
  bool has_data_downloaded_ = false;
  bool has_received_completion_ = false;
  base::Closure quit_closure_for_on_receive_response_;
  base::Closure quit_closure_for_on_receive_redirect_;
  base::Closure quit_closure_for_on_data_downloaded_;
  base::Closure quit_closure_for_on_start_loading_response_body_;
  base::Closure quit_closure_for_on_complete_;
  mojom::URLLoaderFactoryPtr url_loader_factory_;
  int64_t download_data_length_ = 0;
  int64_t encoded_download_data_length_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_TEST_URL_LOADER_CLIENT_H_
