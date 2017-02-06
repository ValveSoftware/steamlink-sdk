// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/domain_reliability/util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

namespace {

const char kJsonMimeType[] = "application/json; charset=utf-8";

class UploadUserData : public base::SupportsUserData::Data {
 public:
  static net::URLFetcher::CreateDataCallback CreateCreateDataCallback(
      int depth) {
    return base::Bind(&UploadUserData::CreateUploadUserData, depth);
  }

  static const void* const kUserDataKey;

  int depth() const { return depth_; }

 private:
  UploadUserData(int depth) : depth_(depth) {}

  static base::SupportsUserData::Data* CreateUploadUserData(int depth) {
    return new UploadUserData(depth);
  }

  int depth_;
};

const void* const UploadUserData::kUserDataKey =
    &UploadUserData::kUserDataKey;

class DomainReliabilityUploaderImpl
    : public DomainReliabilityUploader, net::URLFetcherDelegate {
 public:
  DomainReliabilityUploaderImpl(
      MockableTime* time,
      const scoped_refptr<
          net::URLRequestContextGetter>& url_request_context_getter)
      : time_(time),
        url_request_context_getter_(url_request_context_getter),
        discard_uploads_(true) {}

  ~DomainReliabilityUploaderImpl() override {
    // Delete any in-flight URLFetchers.
    STLDeleteContainerPairFirstPointers(
        upload_callbacks_.begin(), upload_callbacks_.end());
  }

  // DomainReliabilityUploader implementation:
  void UploadReport(
      const std::string& report_json,
      int max_upload_depth,
      const GURL& upload_url,
      const DomainReliabilityUploader::UploadCallback& callback) override {
    VLOG(1) << "Uploading report to " << upload_url;
    VLOG(2) << "Report JSON: " << report_json;

    if (discard_uploads_) {
      VLOG(1) << "Discarding report instead of uploading.";
      UploadResult result;
      result.status = UploadResult::SUCCESS;
      callback.Run(result);
      return;
    }

    net::URLFetcher* fetcher =
        net::URLFetcher::Create(0, upload_url, net::URLFetcher::POST, this)
            .release();
    data_use_measurement::DataUseUserData::AttachToFetcher(
        fetcher, data_use_measurement::DataUseUserData::DOMAIN_RELIABILITY);
    fetcher->SetRequestContext(url_request_context_getter_.get());
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->SetUploadData(kJsonMimeType, report_json);
    fetcher->SetAutomaticallyRetryOn5xx(false);
    fetcher->SetURLRequestUserData(
        UploadUserData::kUserDataKey,
        UploadUserData::CreateCreateDataCallback(max_upload_depth + 1));
    fetcher->Start();

    upload_callbacks_[fetcher] = callback;
  }

  void set_discard_uploads(bool discard_uploads) override {
    discard_uploads_ = discard_uploads;
    VLOG(1) << "Setting discard_uploads to " << discard_uploads;
  }

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* fetcher) override {
    DCHECK(fetcher);

    UploadCallbackMap::iterator callback_it = upload_callbacks_.find(fetcher);
    DCHECK(callback_it != upload_callbacks_.end());

    int net_error = GetNetErrorFromURLRequestStatus(fetcher->GetStatus());
    int http_response_code = fetcher->GetResponseCode();
    base::TimeDelta retry_after;
    {
      std::string retry_after_string;
      if (fetcher->GetResponseHeaders() &&
          fetcher->GetResponseHeaders()->EnumerateHeader(nullptr,
                                                         "Retry-After",
                                                         &retry_after_string)) {
        net::HttpUtil::ParseRetryAfterHeader(retry_after_string,
                                             time_->Now(),
                                             &retry_after);
      }
    }

    VLOG(1) << "Upload finished with net error " << net_error
            << ", response code " << http_response_code
            << ", retry after " << retry_after;

    UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.UploadResponseCode",
                                http_response_code);
    UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.UploadNetError",
                                -net_error);

    UploadResult result;
    GetUploadResultFromResponseDetails(net_error,
                                       http_response_code,
                                       retry_after,
                                       &result);
    callback_it->second.Run(result);

    delete callback_it->first;
    upload_callbacks_.erase(callback_it);
  }

 private:
  using DomainReliabilityUploader::UploadCallback;
  typedef std::map<const net::URLFetcher*, UploadCallback> UploadCallbackMap;

  MockableTime* time_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  UploadCallbackMap upload_callbacks_;
  bool discard_uploads_;
};

}  // namespace

DomainReliabilityUploader::DomainReliabilityUploader() {}
DomainReliabilityUploader::~DomainReliabilityUploader() {}

// static
std::unique_ptr<DomainReliabilityUploader> DomainReliabilityUploader::Create(
    MockableTime* time,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  return std::unique_ptr<DomainReliabilityUploader>(
      new DomainReliabilityUploaderImpl(time, url_request_context_getter));
}

// static
int DomainReliabilityUploader::GetURLRequestUploadDepth(
    const net::URLRequest& request) {
  UploadUserData* data = static_cast<UploadUserData*>(
      request.GetUserData(UploadUserData::kUserDataKey));
  if (!data)
    return 0;
  return data->depth();
}

}  // namespace domain_reliability
