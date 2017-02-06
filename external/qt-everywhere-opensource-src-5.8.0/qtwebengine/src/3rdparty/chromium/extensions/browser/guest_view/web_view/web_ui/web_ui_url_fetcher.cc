// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"

WebUIURLFetcher::WebUIURLFetcher(content::BrowserContext* context,
                                 int render_process_id,
                                 int render_view_id,
                                 const GURL& url,
                                 const WebUILoadFileCallback& callback)
    : context_(context),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      url_(url),
      callback_(callback) {
}

WebUIURLFetcher::~WebUIURLFetcher() {
}

void WebUIURLFetcher::Start() {
  fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::GET, this);
  fetcher_->SetRequestContext(
      content::BrowserContext::GetDefaultStoragePartition(context_)->
          GetURLRequestContext());
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);

  content::AssociateURLFetcherWithRenderFrame(
      fetcher_.get(), url_, render_process_id_, render_view_id_);
  fetcher_->Start();
}

void WebUIURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(fetcher_.get(), source);

  std::string data;
  bool result = false;
  if (fetcher_->GetStatus().status() == net::URLRequestStatus::SUCCESS) {
    result = fetcher_->GetResponseAsString(&data);
    DCHECK(result);
  }
  fetcher_.reset();
  // We cache the callback and reset it so that any references stored within it
  // are destroyed at the end of the method.
  auto callback_cache = callback_;
  callback_.Reset();
  callback_cache.Run(result, data);
}
