// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace autofill {

namespace {

const size_t kMaxFormCacheSize = 16;
const size_t kMaxFieldsPerQueryRequest = 100;

const net::BackoffEntry::Policy kAutofillBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    1000,  // 1 second.

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.33,  // 33%.

    // Maximum amount of time we are willing to delay our request in ms.
    30 * 1000,  // 30 seconds.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

#if defined(GOOGLE_CHROME_BUILD)
const char kClientName[] = "Google Chrome";
#else
const char kClientName[] = "Chromium";
#endif  // defined(GOOGLE_CHROME_BUILD)

size_t CountActiveFieldsInForms(const std::vector<FormStructure*>& forms) {
  size_t active_field_count = 0;
  for (const auto* form : forms)
    active_field_count += form->active_field_count();
  return active_field_count;
}

std::string RequestTypeToString(AutofillDownloadManager::RequestType type) {
  switch (type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      return "query";
    case AutofillDownloadManager::REQUEST_UPLOAD:
      return "upload";
  }
  NOTREACHED();
  return std::string();
}

GURL GetRequestUrl(AutofillDownloadManager::RequestType request_type) {
  return GURL("https://clients1.google.com/tbproxy/af/" +
              RequestTypeToString(request_type) + "?client=" + kClientName);
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryContents& query) {
  out << "client_version: " << query.client_version();
  for (const auto& form : query.form()) {
    out << "\nForm\n signature: " << form.signature();
    for (const auto& field : form.field()) {
      out << "\n Field\n  signature: " << field.signature();
      if (!field.name().empty())
        out << "\n  name: " << field.name();
      if (!field.type().empty())
        out << "\n  type: " << field.type();
      if (!field.label().empty())
        out << "\n  label: " << field.label();
    }
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillUploadContents& upload) {
  out << "client_version: " << upload.client_version() << "\n";
  out << "form_signature: " << upload.form_signature() << "\n";
  out << "data_present: " << upload.data_present() << "\n";
  out << "submission: " << upload.submission() << "\n";
  if (!upload.action_signature())
    out << "action_signature: " << upload.action_signature() << "\n";
  if (!upload.login_form_signature())
    out << "login_form_signature: " << upload.login_form_signature() << "\n";
  if (!upload.form_name().empty())
    out << "form_name: " << upload.form_name() << "\n";

  for (const auto& field : upload.field()) {
    out << "\n Field"
      << "\n signature: " << field.signature()
      << "\n autofill_type: " << field.autofill_type();
    if (!field.name().empty())
      out << "\n name: " << field.name();
    if (!field.autocomplete().empty())
      out << "\n autocomplete: " << field.autocomplete();
    if (!field.type().empty())
      out << "\n type: " << field.type();
    if (!field.label().empty())
      out << "\n label: " << field.label();
    if (field.generation_type())
      out << "\n generation_type: " << field.generation_type();
  }
  return out;
}

}  // namespace

struct AutofillDownloadManager::FormRequestData {
  std::vector<std::string> form_signatures;
  RequestType request_type;
  std::string payload;
};

AutofillDownloadManager::AutofillDownloadManager(AutofillDriver* driver,
                                                 Observer* observer)
    : driver_(driver),
      observer_(observer),
      max_form_cache_size_(kMaxFormCacheSize),
      fetcher_backoff_(&kAutofillBackoffPolicy),
      fetcher_id_for_unittest_(0),
      weak_factory_(this) {
  DCHECK(observer_);
}

AutofillDownloadManager::~AutofillDownloadManager() {
  STLDeleteContainerPairFirstPointers(url_fetchers_.begin(),
                                      url_fetchers_.end());
}

bool AutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms) {
  // Do not send the request if it contains more fields than the server can
  // accept.
  if (CountActiveFieldsInForms(forms) > kMaxFieldsPerQueryRequest)
    return false;

  AutofillQueryContents query;
  FormRequestData request_data;
  if (!FormStructure::EncodeQueryRequest(forms, &request_data.form_signatures,
                                         &query)) {
    return false;
  }

  std::string payload;
  if (!query.SerializeToString(&payload))
    return false;

  request_data.request_type = AutofillDownloadManager::REQUEST_QUERY;
  request_data.payload = payload;
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    VLOG(1) << "AutofillDownloadManager: query request has been retrieved "
             << "from the cache, form signatures: "
             << GetCombinedSignature(request_data.form_signatures);
    observer_->OnLoadedServerPredictions(std::move(query_data),
                                         request_data.form_signatures);
    return true;
  }

  VLOG(1) << "Sending Autofill Query Request:\n" << query;

  return StartRequest(request_data);
}

bool AutofillDownloadManager::StartUploadRequest(
    const FormStructure& form,
    bool form_was_autofilled,
    const ServerFieldTypeSet& available_field_types,
    const std::string& login_form_signature,
    bool observed_submission) {
  AutofillUploadContents upload;
  if (!form.EncodeUploadRequest(available_field_types, form_was_autofilled,
                                login_form_signature, observed_submission,
                                &upload))
    return false;

  std::string payload;
  if (!upload.SerializeToString(&payload))
    return false;

  if (form.upload_required() == UPLOAD_NOT_REQUIRED) {
    VLOG(1) << "AutofillDownloadManager: Upload request is ignored.";
    // If we ever need notification that upload was skipped, add it here.
    return false;
  }

  FormRequestData request_data;
  request_data.form_signatures.push_back(form.FormSignature());
  request_data.request_type = AutofillDownloadManager::REQUEST_UPLOAD;
  request_data.payload = payload;

  VLOG(1) << "Sending Autofill Upload Request:\n" << upload;

  return StartRequest(request_data);
}

bool AutofillDownloadManager::StartRequest(
    const FormRequestData& request_data) {
  net::URLRequestContextGetter* request_context =
      driver_->GetURLRequestContext();
  DCHECK(request_context);
  GURL request_url = GetRequestUrl(request_data.request_type);

  // Id is ignored for regular chrome, in unit test id's for fake fetcher
  // factory will be 0, 1, 2, ...
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(fetcher_id_for_unittest_++, request_url,
                              net::URLFetcher::POST, this).release();
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::AUTOFILL);
  url_fetchers_[fetcher] = request_data;
  fetcher->SetAutomaticallyRetryOn5xx(false);
  fetcher->SetRequestContext(request_context);
  fetcher->SetUploadData("text/proto", request_data.payload);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(
      fetcher->GetOriginalURL(), driver_->IsOffTheRecord(), false, &headers);
  fetcher->SetExtraRequestHeaders(headers.ToString());
  fetcher->Start();

  return true;
}

void AutofillDownloadManager::CacheQueryRequest(
    const std::vector<std::string>& forms_in_query,
    const std::string& query_data) {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (auto it = cached_forms_.begin(); it != cached_forms_.end(); ++it) {
    if (it->first == signature) {
      // We hit the cache, move to the first position and return.
      std::pair<std::string, std::string> data = *it;
      cached_forms_.erase(it);
      cached_forms_.push_front(data);
      return;
    }
  }
  std::pair<std::string, std::string> data;
  data.first = signature;
  data.second = query_data;
  cached_forms_.push_front(data);
  while (cached_forms_.size() > max_form_cache_size_)
    cached_forms_.pop_back();
}

bool AutofillDownloadManager::CheckCacheForQueryRequest(
    const std::vector<std::string>& forms_in_query,
    std::string* query_data) const {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (const auto& it : cached_forms_) {
    if (it.first == signature) {
      // We hit the cache, fill the data and return.
      *query_data = it.second;
      return true;
    }
  }
  return false;
}

std::string AutofillDownloadManager::GetCombinedSignature(
    const std::vector<std::string>& forms_in_query) const {
  size_t total_size = forms_in_query.size();
  for (size_t i = 0; i < forms_in_query.size(); ++i)
    total_size += forms_in_query[i].length();
  std::string signature;

  signature.reserve(total_size);

  for (size_t i = 0; i < forms_in_query.size(); ++i) {
    if (i)
      signature.append(",");
    signature.append(forms_in_query[i]);
  }
  return signature;
}

void AutofillDownloadManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::map<net::URLFetcher *, FormRequestData>::iterator it =
      url_fetchers_.find(const_cast<net::URLFetcher*>(source));
  if (it == url_fetchers_.end()) {
    // Looks like crash on Mac is possibly caused with callback entering here
    // with unknown fetcher when network is refreshed.
    return;
  }
  std::string request_type(RequestTypeToString(it->second.request_type));

  CHECK(it->second.form_signatures.size());
  bool success = source->GetResponseCode() == net::HTTP_OK;
  fetcher_backoff_.InformOfRequest(success);

  if (!success) {
    // Reschedule with the appropriate delay, ignoring return value because
    // payload is already well formed.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&AutofillDownloadManager::StartRequest),
                   weak_factory_.GetWeakPtr(), it->second),
        fetcher_backoff_.GetTimeUntilRelease());

    VLOG(1) << "AutofillDownloadManager: " << request_type
             << " request has failed with response "
             << source->GetResponseCode();
    observer_->OnServerRequestError(it->second.form_signatures[0],
                                    it->second.request_type,
                                    source->GetResponseCode());
  } else {
    std::string response_body;
    source->GetResponseAsString(&response_body);
    if (it->second.request_type == AutofillDownloadManager::REQUEST_QUERY) {
      CacheQueryRequest(it->second.form_signatures, response_body);
      observer_->OnLoadedServerPredictions(std::move(response_body),
                                           it->second.form_signatures);
    } else {
      VLOG(1) << "AutofillDownloadManager: upload request has succeeded.";
      observer_->OnUploadedPossibleFieldTypes();
    }
  }
  delete it->first;
  url_fetchers_.erase(it);
}

}  // namespace autofill
