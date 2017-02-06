// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/url_provision_fetcher.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/android/provision_fetcher_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "net/url_request/url_fetcher.h"

using net::URLFetcher;

namespace content {

// Implementation of URLProvisionFetcher.

URLProvisionFetcher::URLProvisionFetcher(
    net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter) {}

URLProvisionFetcher::~URLProvisionFetcher() {}

void URLProvisionFetcher::Retrieve(
    const std::string& default_url,
    const std::string& request_data,
    const media::ProvisionFetcher::ResponseCB& response_cb) {
  response_cb_ = response_cb;

  const std::string request_string =
      default_url + "&signedRequest=" + request_data;
  DVLOG(1) << __FUNCTION__ << ": request:" << request_string;

  DCHECK(!request_);
  request_ = URLFetcher::Create(GURL(request_string), URLFetcher::POST, this);

  // SetUploadData is mandatory even if we are not uploading anything.
  request_->SetUploadData("", "");
  request_->AddExtraRequestHeader("User-Agent: Widevine CDM v1.0");
  request_->AddExtraRequestHeader("Content-Type: application/json");

  DCHECK(context_getter_);
  request_->SetRequestContext(context_getter_);

  request_->Start();
}

void URLProvisionFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);

  int response_code = source->GetResponseCode();
  DVLOG(1) << __FUNCTION__ << ": response code:" << source->GetResponseCode();

  std::string response;
  bool success = false;
  if (response_code == 200) {
    success = source->GetResponseAsString(&response);
    DVLOG_IF(1, !success) << __FUNCTION__ << ": GetResponseAsString() failed";
  } else {
    DVLOG(1) << "CDM provision: server returned error code " << response_code;
  }

  request_.reset();

  // URLFetcher implementation calls OnURLFetchComplete() on the same thread
  // that called Start() and it does this asynchronously.
  response_cb_.Run(success, response);
}

// Implementation of content public method CreateProvisionFetcher().

std::unique_ptr<media::ProvisionFetcher> CreateProvisionFetcher(
    net::URLRequestContextGetter* context_getter) {
  DCHECK(context_getter);
  return base::WrapUnique(new URLProvisionFetcher(context_getter));
}

}  // namespace content
