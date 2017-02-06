// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "ui/gfx/text_elider.h"
#endif  // ENABLE_PRINT_PREVIEW

namespace cloud_print {

namespace {

const char kUrlPlaceHolder[] = "http://host/";
const char kPrivetRegisterActionArgName[] = "action";
const char kPrivetRegisterUserArgName[] = "user";

const int kPrivetCancelationTimeoutSeconds = 3;

#if defined(ENABLE_PRINT_PREVIEW)
const char kPrivetURLKeyUserName[] = "user_name";
const char kPrivetURLKeyClientName[] = "client_name";
const char kPrivetURLKeyJobname[] = "job_name";
const char kPrivetURLKeyOffline[] = "offline";
const char kPrivetURLValueOffline[] = "1";
const char kPrivetURLValueClientName[] = "Chrome";

const char kPrivetContentTypePDF[] = "application/pdf";
const char kPrivetContentTypePWGRaster[] = "image/pwg-raster";
const char kPrivetContentTypeAny[] = "*/*";

const char kPrivetKeyJobID[] = "job_id";

const int kPrivetLocalPrintMaxRetries = 2;
const int kPrivetLocalPrintDefaultTimeout = 5;

const size_t kPrivetLocalPrintMaxJobNameLength = 64;
#endif  // ENABLE_PRINT_PREVIEW

GURL CreatePrivetURL(const std::string& path) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL CreatePrivetRegisterURL(const std::string& action,
                             const std::string& user) {
  GURL url = CreatePrivetURL(kPrivetRegisterPath);
  url = net::AppendQueryParameter(url, kPrivetRegisterActionArgName, action);
  return net::AppendQueryParameter(url, kPrivetRegisterUserArgName, user);
}

GURL CreatePrivetParamURL(const std::string& path,
                          const std::string& query_params) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  if (!query_params.empty()) {
    replacements.SetQueryStr(query_params);
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace

PrivetInfoOperationImpl::PrivetInfoOperationImpl(
    PrivetHTTPClient* privet_client,
    const PrivetJSONOperation::ResultCallback& callback)
    : privet_client_(privet_client), callback_(callback) {
}

PrivetInfoOperationImpl::~PrivetInfoOperationImpl() {
}

void PrivetInfoOperationImpl::Start() {
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetInfoPath), net::URLFetcher::GET, this);

  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->SendEmptyPrivetToken();

  url_fetcher_->Start();
}

PrivetHTTPClient* PrivetInfoOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetInfoOperationImpl::OnError(PrivetURLFetcher* fetcher,
                                      PrivetURLFetcher::ErrorType error) {
  callback_.Run(NULL);
}

void PrivetInfoOperationImpl::OnParsedJson(PrivetURLFetcher* fetcher,
                                           const base::DictionaryValue& value,
                                           bool has_error) {
  callback_.Run(&value);
}

PrivetRegisterOperationImpl::PrivetRegisterOperationImpl(
    PrivetHTTPClient* privet_client,
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate)
    : user_(user),
      delegate_(delegate),
      privet_client_(privet_client),
      ongoing_(false) {
}

PrivetRegisterOperationImpl::~PrivetRegisterOperationImpl() {
}

void PrivetRegisterOperationImpl::Start() {
  ongoing_ = true;
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::StartResponse,
                 base::Unretained(this));
  SendRequest(kPrivetActionStart);
}

void PrivetRegisterOperationImpl::Cancel() {
  url_fetcher_.reset();

  if (ongoing_) {
    // Owned by the message loop.
    Cancelation* cancelation = new Cancelation(privet_client_, user_);

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrivetRegisterOperationImpl::Cancelation::Cleanup,
                   base::Owned(cancelation)),
        base::TimeDelta::FromSeconds(kPrivetCancelationTimeoutSeconds));

    ongoing_ = false;
  }
}

void PrivetRegisterOperationImpl::CompleteRegistration() {
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::CompleteResponse,
                 base::Unretained(this));
  SendRequest(kPrivetActionComplete);
}

PrivetHTTPClient* PrivetRegisterOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetRegisterOperationImpl::OnError(PrivetURLFetcher* fetcher,
                                          PrivetURLFetcher::ErrorType error) {
  ongoing_ = false;
  int visible_http_code = -1;
  FailureReason reason = FAILURE_NETWORK;

  if (error == PrivetURLFetcher::RESPONSE_CODE_ERROR) {
    visible_http_code = fetcher->response_code();
    reason = FAILURE_HTTP_ERROR;
  } else if (error == PrivetURLFetcher::JSON_PARSE_ERROR) {
    reason = FAILURE_MALFORMED_RESPONSE;
  } else if (error == PrivetURLFetcher::TOKEN_ERROR) {
    reason = FAILURE_TOKEN;
  } else if (error == PrivetURLFetcher::UNKNOWN_ERROR) {
    reason = FAILURE_UNKNOWN;
  }

  delegate_->OnPrivetRegisterError(this,
                                   current_action_,
                                   reason,
                                   visible_http_code,
                                   NULL);
}

void PrivetRegisterOperationImpl::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
  if (has_error) {
    std::string error;
    value.GetString(kPrivetKeyError, &error);

    ongoing_ = false;
    delegate_->OnPrivetRegisterError(this,
                                     current_action_,
                                     FAILURE_JSON_ERROR,
                                     fetcher->response_code(),
                                     &value);
    return;
  }

  // TODO(noamsml): Match the user&action with the user&action in the object,
  // and fail if different.

  next_response_handler_.Run(value);
}

void PrivetRegisterOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

void PrivetRegisterOperationImpl::SendRequest(const std::string& action) {
  current_action_ = action;
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetRegisterURL(action, user_), net::URLFetcher::POST, this);
  url_fetcher_->Start();
}

void PrivetRegisterOperationImpl::StartResponse(
    const base::DictionaryValue& value) {
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::GetClaimTokenResponse,
                 base::Unretained(this));

  SendRequest(kPrivetActionGetClaimToken);
}

void PrivetRegisterOperationImpl::GetClaimTokenResponse(
    const base::DictionaryValue& value) {
  std::string claimUrl;
  std::string claimToken;
  bool got_url = value.GetString(kPrivetKeyClaimURL, &claimUrl);
  bool got_token = value.GetString(kPrivetKeyClaimToken, &claimToken);
  if (got_url || got_token) {
    delegate_->OnPrivetRegisterClaimToken(this, claimToken, GURL(claimUrl));
  } else {
    delegate_->OnPrivetRegisterError(this,
                                     current_action_,
                                     FAILURE_MALFORMED_RESPONSE,
                                     -1,
                                     NULL);
  }
}

void PrivetRegisterOperationImpl::CompleteResponse(
    const base::DictionaryValue& value) {
  std::string id;
  value.GetString(kPrivetKeyDeviceID, &id);
  ongoing_ = false;
  expected_id_ = id;
  StartInfoOperation();
}

void PrivetRegisterOperationImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  // TODO(noamsml): Simplify error case and depracate HTTP error value in
  // OnPrivetRegisterError.
  if (!value) {
    delegate_->OnPrivetRegisterError(this,
                                     kPrivetActionNameInfo,
                                     FAILURE_NETWORK,
                                     -1,
                                     NULL);
    return;
  }

  if (!value->HasKey(kPrivetInfoKeyID)) {
    if (value->HasKey(kPrivetKeyError)) {
      delegate_->OnPrivetRegisterError(this,
                                       kPrivetActionNameInfo,
                                        FAILURE_JSON_ERROR,
                                       -1,
                                       value);
    } else {
      delegate_->OnPrivetRegisterError(this,
                                       kPrivetActionNameInfo,
                                       FAILURE_MALFORMED_RESPONSE,
                                       -1,
                                       NULL);
    }
    return;
  }

  std::string id;

  if (!value->GetString(kPrivetInfoKeyID, &id) ||
      id != expected_id_) {
    delegate_->OnPrivetRegisterError(this,
                                     kPrivetActionNameInfo,
                                     FAILURE_MALFORMED_RESPONSE,
                                     -1,
                                     NULL);
  } else {
    delegate_->OnPrivetRegisterDone(this, id);
  }
}

void PrivetRegisterOperationImpl::StartInfoOperation() {
  info_operation_ = privet_client_->CreateInfoOperation(
      base::Bind(&PrivetRegisterOperationImpl::OnPrivetInfoDone,
                 base::Unretained(this)));
  info_operation_->Start();
}

PrivetRegisterOperationImpl::Cancelation::Cancelation(
    PrivetHTTPClient* privet_client,
    const std::string& user) {
  url_fetcher_ =
      privet_client->CreateURLFetcher(
          CreatePrivetRegisterURL(kPrivetActionCancel, user),
          net::URLFetcher::POST, this);
  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->Start();
}

PrivetRegisterOperationImpl::Cancelation::~Cancelation() {
}

void PrivetRegisterOperationImpl::Cancelation::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
}

void PrivetRegisterOperationImpl::Cancelation::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
}

void PrivetRegisterOperationImpl::Cancelation::Cleanup() {
  // Nothing needs to be done, as base::Owned will delete this object,
  // this callback is just here to pass ownership of the Cancelation to
  // the message loop.
}

PrivetJSONOperationImpl::PrivetJSONOperationImpl(
    PrivetHTTPClient* privet_client,
    const std::string& path,
    const std::string& query_params,
    const PrivetJSONOperation::ResultCallback& callback)
    : privet_client_(privet_client),
      path_(path),
      query_params_(query_params),
      callback_(callback) {
}

PrivetJSONOperationImpl::~PrivetJSONOperationImpl() {
}

void PrivetJSONOperationImpl::Start() {
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetParamURL(path_, query_params_), net::URLFetcher::GET, this);
  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->Start();
}

PrivetHTTPClient* PrivetJSONOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetJSONOperationImpl::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  callback_.Run(NULL);
}

void PrivetJSONOperationImpl::OnParsedJson(PrivetURLFetcher* fetcher,
                                           const base::DictionaryValue& value,
                                           bool has_error) {
  callback_.Run(&value);
}

void PrivetJSONOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

#if defined(ENABLE_PRINT_PREVIEW)
PrivetLocalPrintOperationImpl::PrivetLocalPrintOperationImpl(
    PrivetHTTPClient* privet_client,
    PrivetLocalPrintOperation::Delegate* delegate)
    : privet_client_(privet_client),
      delegate_(delegate),
      use_pdf_(false),
      has_extended_workflow_(false),
      started_(false),
      offline_(false),
      invalid_job_retries_(0),
      weak_factory_(this) {
}

PrivetLocalPrintOperationImpl::~PrivetLocalPrintOperationImpl() {
}

void PrivetLocalPrintOperationImpl::Start() {
  DCHECK(!started_);

  // We need to get the /info response so we can know which APIs are available.
  // TODO(noamsml): Use cached info when available.
  info_operation_ = privet_client_->CreateInfoOperation(
      base::Bind(&PrivetLocalPrintOperationImpl::OnPrivetInfoDone,
                 base::Unretained(this)));
  info_operation_->Start();

  started_ = true;
}

void PrivetLocalPrintOperationImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  if (value && !value->HasKey(kPrivetKeyError)) {
    has_extended_workflow_ = false;
    bool has_printing = false;

    const base::ListValue* api_list;
    if (value->GetList(kPrivetInfoKeyAPIList, &api_list)) {
      for (size_t i = 0; i < api_list->GetSize(); i++) {
        std::string api;
        api_list->GetString(i, &api);
        if (api == kPrivetSubmitdocPath) {
          has_printing = true;
        } else if (api == kPrivetCreatejobPath) {
          has_extended_workflow_ = true;
        }
      }
    }

    if (!has_printing) {
      delegate_->OnPrivetPrintingError(this, -1);
      return;
    }

    StartInitialRequest();
  } else {
    delegate_->OnPrivetPrintingError(this, -1);
  }
}

void PrivetLocalPrintOperationImpl::StartInitialRequest() {
  use_pdf_ = false;
  cloud_devices::printer::ContentTypesCapability content_types;
  if (content_types.LoadFrom(capabilities_)) {
    use_pdf_ = content_types.Contains(kPrivetContentTypePDF) ||
               content_types.Contains(kPrivetContentTypeAny);
  }

  if (use_pdf_) {
    StartPrinting();
  } else {
    StartConvertToPWG();
  }
}

void PrivetLocalPrintOperationImpl::DoCreatejob() {
  current_response_ = base::Bind(
      &PrivetLocalPrintOperationImpl::OnCreatejobResponse,
      base::Unretained(this));

  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetCreatejobPath), net::URLFetcher::POST, this);
  url_fetcher_->SetUploadData(cloud_print::kContentTypeJSON,
                              ticket_.ToString());

  url_fetcher_->Start();
}

void PrivetLocalPrintOperationImpl::DoSubmitdoc() {
  current_response_ = base::Bind(
      &PrivetLocalPrintOperationImpl::OnSubmitdocResponse,
      base::Unretained(this));

  GURL url = CreatePrivetURL(kPrivetSubmitdocPath);

  url = net::AppendQueryParameter(url,
                                  kPrivetURLKeyClientName,
                                  kPrivetURLValueClientName);

  if (!user_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyUserName,
                                    user_);
  }

  base::string16 shortened_jobname;

  gfx::ElideString(base::UTF8ToUTF16(jobname_),
                   kPrivetLocalPrintMaxJobNameLength,
                   &shortened_jobname);

  if (!jobname_.empty()) {
    url = net::AppendQueryParameter(
        url, kPrivetURLKeyJobname, base::UTF16ToUTF8(shortened_jobname));
  }

  if (!jobid_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetKeyJobID,
                                    jobid_);
  }

  if (offline_) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyOffline,
                                    kPrivetURLValueOffline);
  }

  url_fetcher_ =
      privet_client_->CreateURLFetcher(url, net::URLFetcher::POST, this);

  if (!use_pdf_) {
    url_fetcher_->SetUploadFilePath(kPrivetContentTypePWGRaster,
                                    pwg_file_path_);
  } else {
    // TODO(noamsml): Move to file-based upload data?
    std::string data_str(reinterpret_cast<const char*>(data_->front()),
                         data_->size());
    url_fetcher_->SetUploadData(kPrivetContentTypePDF, data_str);
  }

  url_fetcher_->Start();
}

void PrivetLocalPrintOperationImpl::StartPrinting() {
  if (has_extended_workflow_ && jobid_.empty()) {
    DoCreatejob();
  } else {
    DoSubmitdoc();
  }
}

void PrivetLocalPrintOperationImpl::StartConvertToPWG() {
  using printing::PWGRasterConverter;
  if (!pwg_raster_converter_)
    pwg_raster_converter_ = PWGRasterConverter::CreateDefault();

  pwg_raster_converter_->Start(
      data_.get(),
      PWGRasterConverter::GetConversionSettings(capabilities_, page_size_),
      PWGRasterConverter::GetBitmapSettings(capabilities_, ticket_),
      base::Bind(&PrivetLocalPrintOperationImpl::OnPWGRasterConverted,
                 base::Unretained(this)));
}

void PrivetLocalPrintOperationImpl::OnSubmitdocResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  std::string error;
  // This error is only relevant in the case of extended workflow:
  // If the print job ID is invalid, retry createjob and submitdoc,
  // rather than simply retrying the current request.
  if (has_error && value->GetString(kPrivetKeyError, &error)) {
    if (has_extended_workflow_ &&
        error == kPrivetErrorInvalidPrintJob &&
        invalid_job_retries_ < kPrivetLocalPrintMaxRetries) {
      invalid_job_retries_++;

      int timeout = kPrivetLocalPrintDefaultTimeout;
      value->GetInteger(kPrivetKeyTimeout, &timeout);

      double random_scaling_factor =
          1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

      timeout = static_cast<int>(timeout * random_scaling_factor);

      timeout = std::max(timeout, kPrivetMinimumTimeout);

      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::Bind(&PrivetLocalPrintOperationImpl::DoCreatejob,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(timeout));
    } else if (use_pdf_ && error == kPrivetErrorInvalidDocumentType) {
      use_pdf_ = false;
      StartConvertToPWG();
    } else {
      delegate_->OnPrivetPrintingError(this, 200);
    }

    return;
  }

  // If we've gotten this far, there are no errors, so we've effectively
  // succeeded.
  delegate_->OnPrivetPrintingDone(this);
}

void PrivetLocalPrintOperationImpl::OnCreatejobResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  if (has_error) {
    delegate_->OnPrivetPrintingError(this, 200);
    return;
  }

  // Try to get job ID from value. If not, jobid_ will be empty and we will use
  // simple printing.
  value->GetString(kPrivetKeyJobID, &jobid_);

  DoSubmitdoc();
}

void PrivetLocalPrintOperationImpl::OnPWGRasterConverted(
    bool success,
    const base::FilePath& pwg_file_path) {
  if (!success) {
    delegate_->OnPrivetPrintingError(this, -1);
    return;
  }

  DCHECK(!pwg_file_path.empty());

  pwg_file_path_ = pwg_file_path;
  StartPrinting();
}

PrivetHTTPClient* PrivetLocalPrintOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetLocalPrintOperationImpl::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  delegate_->OnPrivetPrintingError(this, -1);
}

void PrivetLocalPrintOperationImpl::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
  DCHECK(!current_response_.is_null());
  current_response_.Run(has_error, &value);
}

void PrivetLocalPrintOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

void PrivetLocalPrintOperationImpl::SetData(
    const scoped_refptr<base::RefCountedBytes>& data) {
  DCHECK(!started_);
  data_ = data;
}

void PrivetLocalPrintOperationImpl::SetTicket(const std::string& ticket) {
  DCHECK(!started_);
  ticket_.InitFromString(ticket);
}

void PrivetLocalPrintOperationImpl::SetCapabilities(
    const std::string& capabilities) {
  DCHECK(!started_);
  capabilities_.InitFromString(capabilities);
}

void PrivetLocalPrintOperationImpl::SetUsername(const std::string& user) {
  DCHECK(!started_);
  user_ = user;
}

void PrivetLocalPrintOperationImpl::SetJobname(const std::string& jobname) {
  DCHECK(!started_);
  jobname_ = jobname;
}

void PrivetLocalPrintOperationImpl::SetOffline(bool offline) {
  DCHECK(!started_);
  offline_ = offline;
}

void PrivetLocalPrintOperationImpl::SetPageSize(const gfx::Size& page_size) {
  DCHECK(!started_);
  page_size_ = page_size;
}

void PrivetLocalPrintOperationImpl::SetPWGRasterConverterForTesting(
    std::unique_ptr<printing::PWGRasterConverter> pwg_raster_converter) {
  pwg_raster_converter_ = std::move(pwg_raster_converter);
}
#endif  // ENABLE_PRINT_PREVIEW

PrivetHTTPClientImpl::PrivetHTTPClientImpl(
    const std::string& name,
    const net::HostPortPair& host_port,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter)
    : name_(name), context_getter_(context_getter), host_port_(host_port) {}

PrivetHTTPClientImpl::~PrivetHTTPClientImpl() {
}

const std::string& PrivetHTTPClientImpl::GetName() {
  return name_;
}

std::unique_ptr<PrivetJSONOperation> PrivetHTTPClientImpl::CreateInfoOperation(
    const PrivetJSONOperation::ResultCallback& callback) {
  return std::unique_ptr<PrivetJSONOperation>(
      new PrivetInfoOperationImpl(this, callback));
}

std::unique_ptr<PrivetURLFetcher> PrivetHTTPClientImpl::CreateURLFetcher(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    PrivetURLFetcher::Delegate* delegate) {
  GURL::Replacements replacements;
  std::string host = host_port_.HostForURL();
  replacements.SetHostStr(host);
  std::string port = base::UintToString(host_port_.port());
  replacements.SetPortStr(port);
  return std::unique_ptr<PrivetURLFetcher>(
      new PrivetURLFetcher(url.ReplaceComponents(replacements), request_type,
                           context_getter_, delegate));
}

void PrivetHTTPClientImpl::RefreshPrivetToken(
    const PrivetURLFetcher::TokenCallback& callback) {
  token_callbacks_.push_back(callback);

  if (!info_operation_) {
    info_operation_ = CreateInfoOperation(
        base::Bind(&PrivetHTTPClientImpl::OnPrivetInfoDone,
                   base::Unretained(this)));
    info_operation_->Start();
  }
}

void PrivetHTTPClientImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  info_operation_.reset();
  std::string token;

  // If this does not succeed, token will be empty, and an empty string
  // is our sentinel value, since empty X-Privet-Tokens are not allowed.
  if (value) {
    value->GetString(kPrivetInfoKeyToken, &token);
  }

  TokenCallbackVector token_callbacks;
  token_callbacks_.swap(token_callbacks);

  for (TokenCallbackVector::iterator i = token_callbacks.begin();
       i != token_callbacks.end(); i++) {
    i->Run(token);
  }
}

PrivetV1HTTPClientImpl::PrivetV1HTTPClientImpl(
    std::unique_ptr<PrivetHTTPClient> info_client)
    : info_client_(std::move(info_client)) {}

PrivetV1HTTPClientImpl::~PrivetV1HTTPClientImpl() {
}

const std::string& PrivetV1HTTPClientImpl::GetName() {
  return info_client()->GetName();
}

std::unique_ptr<PrivetJSONOperation>
PrivetV1HTTPClientImpl::CreateInfoOperation(
    const PrivetJSONOperation::ResultCallback& callback) {
  return info_client()->CreateInfoOperation(callback);
}

std::unique_ptr<PrivetRegisterOperation>
PrivetV1HTTPClientImpl::CreateRegisterOperation(
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate) {
  return std::unique_ptr<PrivetRegisterOperation>(
      new PrivetRegisterOperationImpl(info_client(), user, delegate));
}

std::unique_ptr<PrivetJSONOperation>
PrivetV1HTTPClientImpl::CreateCapabilitiesOperation(
    const PrivetJSONOperation::ResultCallback& callback) {
  return std::unique_ptr<PrivetJSONOperation>(new PrivetJSONOperationImpl(
      info_client(), kPrivetCapabilitiesPath, "", callback));
}

std::unique_ptr<PrivetLocalPrintOperation>
PrivetV1HTTPClientImpl::CreateLocalPrintOperation(
    PrivetLocalPrintOperation::Delegate* delegate) {
#if defined(ENABLE_PRINT_PREVIEW)
  return std::unique_ptr<PrivetLocalPrintOperation>(
      new PrivetLocalPrintOperationImpl(info_client(), delegate));
#else
  return std::unique_ptr<PrivetLocalPrintOperation>();
#endif  // ENABLE_PRINT_PREVIEW
}

}  // namespace cloud_print
