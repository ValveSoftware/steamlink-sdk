// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/gcd_api_flow_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace cloud_print {

GCDApiFlowImpl::GCDApiFlowImpl(net::URLRequestContextGetter* request_context,
                               OAuth2TokenService* token_service,
                               const std::string& account_id)
    : OAuth2TokenService::Consumer("cloud_print"),
      request_context_(request_context),
      token_service_(token_service),
      account_id_(account_id) {
}

GCDApiFlowImpl::~GCDApiFlowImpl() {
}

void GCDApiFlowImpl::Start(std::unique_ptr<Request> request) {
  request_ = std::move(request);
  OAuth2TokenService::ScopeSet oauth_scopes;
  oauth_scopes.insert(request_->GetOAuthScope());
  oauth_request_ =
      token_service_->StartRequest(account_id_, oauth_scopes, this);
}

void GCDApiFlowImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  CreateRequest(request_->GetURL());

  std::string authorization_header =
      base::StringPrintf(kCloudPrintOAuthHeaderFormat, access_token.c_str());

  url_fetcher_->AddExtraRequestHeader(authorization_header);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->Start();
}

void GCDApiFlowImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  request_->OnGCDApiFlowError(ERROR_TOKEN);
}

void GCDApiFlowImpl::CreateRequest(const GURL& url) {
  net::URLFetcher::RequestType request_type = request_->GetRequestType();

  url_fetcher_ = net::URLFetcher::Create(url, request_type, this);

  if (request_type != net::URLFetcher::GET) {
    std::string upload_type;
    std::string upload_data;
    request_->GetUploadData(&upload_type, &upload_data);
    url_fetcher_->SetUploadData(upload_type, upload_data);
  }

  url_fetcher_->SetRequestContext(request_context_.get());

  std::vector<std::string> extra_headers = request_->GetExtraRequestHeaders();
  for (size_t i = 0; i < extra_headers.size(); ++i)
    url_fetcher_->AddExtraRequestHeader(extra_headers[i]);
}

void GCDApiFlowImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(noamsml): Error logging.

  // TODO(noamsml): Extract this and PrivetURLFetcher::OnURLFetchComplete into
  // one helper method.
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      !source->GetResponseAsString(&response_str)) {
    request_->OnGCDApiFlowError(ERROR_NETWORK);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    request_->OnGCDApiFlowError(ERROR_HTTP_CODE);
    return;
  }

  base::JSONReader reader;
  std::unique_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value)) {
    request_->OnGCDApiFlowError(ERROR_MALFORMED_RESPONSE);
    return;
  }

  request_->OnGCDApiFlowComplete(*dictionary_value);
}

}  // namespace cloud_print
