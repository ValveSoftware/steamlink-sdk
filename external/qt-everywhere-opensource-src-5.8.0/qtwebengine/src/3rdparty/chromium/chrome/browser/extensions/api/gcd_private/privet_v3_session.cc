// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/privet_v3_session.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"
#include "crypto/hmac.h"
#include "crypto/p224_spake.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kPrivetV3AuthTokenHeaderPrefix[] = "Authorization: ";

const char kPrivetV3AuthAnonymous[] = "Privet anonymous";
const char kPrivetV3CryptoP224Spake2[] = "p224_spake2";
const char kPrivetV3Auto[] = "auto";

const char kPrivetV3InfoKeyAuth[] = "authentication";
const char kPrivetV3InfoKeyVersion[] = "version";
const char kPrivetV3InfoVersion[] = "3.0";

const char kPrivetV3KeyAccessToken[] = "accessToken";
const char kPrivetV3KeyAuthCode[] = "authCode";
const char kPrivetV3KeyCertFingerprint[] = "certFingerprint";
const char kPrivetV3KeyCertSignature[] = "certSignature";
const char kPrivetV3KeyClientCommitment[] = "clientCommitment";
const char kPrivetV3KeyCrypto[] = "crypto";
const char kPrivetV3KeyDeviceCommitment[] = "deviceCommitment";
const char kPrivetV3KeyMode[] = "mode";
const char kPrivetV3KeyPairing[] = "pairing";
const char kPrivetV3KeyRequestedScope[] = "requestedScope";
const char kPrivetV3KeyScope[] = "scope";
const char kPrivetV3KeySessionId[] = "sessionId";
const char kPrivetV3KeyTokenType[] = "tokenType";
const char kPrivetV3KeyError[] = "error";

const char kPrivetV3InfoHttpsPort[] = "endpoints.httpsPort";

const char kPrivetInfoPath[] = "/privet/info";
const char kPrivetV3PairingStartPath[] = "/privet/v3/pairing/start";
const char kPrivetV3PairingConfirmPath[] = "/privet/v3/pairing/confirm";
const char kPrivetV3PairingCancelPath[] = "/privet/v3/pairing/cancel";
const char kPrivetV3AuthPath[] = "/privet/v3/auth";

const char kContentTypeJSON[] = "application/json";

const int kUrlFetcherTimeoutSec = 60;

using PairingType = PrivetV3Session::PairingType;

bool GetDecodedString(const base::DictionaryValue& response,
                      const std::string& key,
                      std::string* value) {
  std::string base64;
  return response.GetString(key, &base64) && base::Base64Decode(base64, value);
}

bool ContainsString(const base::DictionaryValue& dictionary,
                    const std::string& key,
                    const std::string& expected_value) {
  const base::ListValue* list_of_string = nullptr;
  if (!dictionary.GetList(key, &list_of_string))
    return false;

  for (const auto& value : *list_of_string) {
    std::string string_value;
    if (value->GetAsString(&string_value) && string_value == expected_value)
      return true;
  }
  return false;
}

std::unique_ptr<base::DictionaryValue> GetJson(const net::URLFetcher* source) {
  if (!source->GetStatus().is_success())
    return nullptr;

  const net::HttpResponseHeaders* headers = source->GetResponseHeaders();
  if (!headers)
    return nullptr;

  std::string content_type;
  if (!headers->GetMimeType(&content_type))
    return nullptr;

  if (content_type != kContentTypeJSON)
    return nullptr;

  std::string json;
  if (!source->GetResponseAsString(&json))
    return nullptr;

  base::JSONReader json_reader(base::JSON_ALLOW_TRAILING_COMMAS);
  return base::DictionaryValue::From(json_reader.ReadToValue(json));
}

}  // namespace

class PrivetV3Session::FetcherDelegate : public net::URLFetcherDelegate {
 public:
  FetcherDelegate(const base::WeakPtr<PrivetV3Session>& session,
                  const PrivetV3Session::MessageCallback& callback);
  ~FetcherDelegate() override;

  // URLFetcherDelegate methods.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLFetcher* CreateURLFetcher(const GURL& url,
                                    net::URLFetcher::RequestType request_type,
                                    bool orphaned);

 private:
  void ReplyAndDestroyItself(Result result, const base::DictionaryValue& value);
  void OnTimeout();

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  base::WeakPtr<PrivetV3Session> session_;
  MessageCallback callback_;

  base::WeakPtrFactory<FetcherDelegate> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FetcherDelegate);
};

PrivetV3Session::FetcherDelegate::FetcherDelegate(
    const base::WeakPtr<PrivetV3Session>& session,
    const PrivetV3Session::MessageCallback& callback)
    : session_(session),
      callback_(callback),
      weak_ptr_factory_(this) {}

PrivetV3Session::FetcherDelegate::~FetcherDelegate() {}

void PrivetV3Session::FetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  VLOG(1) << "PrivetURLFetcher url: " << source->GetURL()
          << ", status: " << source->GetStatus().status()
          << ", error: " << source->GetStatus().error()
          << ", response code: " << source->GetResponseCode();

  std::unique_ptr<base::DictionaryValue> value = GetJson(source);
  if (!value) {
    return ReplyAndDestroyItself(Result::STATUS_CONNECTIONERROR,
                                 base::DictionaryValue());
  }

  bool has_error = value->HasKey(kPrivetV3KeyError);
  LOG_IF(ERROR, has_error) << "Response: " << value.get();
  ReplyAndDestroyItself(
      has_error ? Result::STATUS_DEVICEERROR : Result::STATUS_SUCCESS, *value);
}

net::URLFetcher* PrivetV3Session::FetcherDelegate::CreateURLFetcher(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    bool orphaned) {
  DCHECK(!url_fetcher_);
  DCHECK(session_);
  url_fetcher_ = net::URLFetcher::Create(url, request_type, this);
  auto timeout_task =
      orphaned ? base::Bind(&FetcherDelegate::OnTimeout, base::Owned(this))
               : base::Bind(&FetcherDelegate::OnTimeout,
                            weak_ptr_factory_.GetWeakPtr());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_task,
      base::TimeDelta::FromSeconds(kUrlFetcherTimeoutSec));
  return url_fetcher_.get();
}

void PrivetV3Session::FetcherDelegate::ReplyAndDestroyItself(
    Result result,
    const base::DictionaryValue& value) {
  if (session_) {
    if (!callback_.is_null()) {
      callback_.Run(result, value);
      callback_.Reset();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PrivetV3Session::DeleteFetcher, session_,
                              base::Unretained(this)));
    session_.reset();
  }
  url_fetcher_.reset();
}

void PrivetV3Session::FetcherDelegate::OnTimeout() {
  LOG(ERROR) << "PrivetURLFetcher timeout, url: " << url_fetcher_->GetURL();
  ReplyAndDestroyItself(Result::STATUS_CONNECTIONERROR,
                        base::DictionaryValue());
}

PrivetV3Session::PrivetV3Session(
    const scoped_refptr<PrivetV3ContextGetter>& context_getter,
    const net::HostPortPair& host_port)
    : host_port_(host_port),
      context_getter_(context_getter),
      weak_ptr_factory_(this) {
  CHECK(context_getter_);
}

PrivetV3Session::~PrivetV3Session() {
  Cancel();
}

void PrivetV3Session::Init(const InitCallback& callback) {
  DCHECK(fetchers_.empty());
  DCHECK(!use_https_);
  DCHECK(session_id_.empty());
  DCHECK(privet_auth_token_.empty());

  privet_auth_token_ = kPrivetV3AuthAnonymous;
  StartGetRequest(kPrivetInfoPath,
                  base::Bind(&PrivetV3Session::OnInfoDone,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnInfoDone(const InitCallback& callback,
                                 Result result,
                                 const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result, response);

  std::string version;
  if (!response.GetString(kPrivetV3InfoKeyVersion, &version) ||
      version != kPrivetV3InfoVersion) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, response);
  }

  const base::DictionaryValue* authentication = nullptr;
  const base::ListValue* pairing = nullptr;
  if (!response.GetDictionary(kPrivetV3InfoKeyAuth, &authentication) ||
      !authentication->GetList(kPrivetV3KeyPairing, &pairing)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, response);
  }

  // The only supported crypto.
  if (!ContainsString(*authentication, kPrivetV3KeyCrypto,
                      kPrivetV3CryptoP224Spake2) ||
      !ContainsString(*authentication, kPrivetV3KeyMode, kPrivetV3KeyPairing)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, response);
  }

  int port = 0;
  if (!response.GetInteger(kPrivetV3InfoHttpsPort, &port) || port <= 0 ||
      port > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, response);
  }
  https_port_ = port;

  callback.Run(Result::STATUS_SUCCESS, response);
}

void PrivetV3Session::StartPairing(PairingType pairing_type,
                                   const ResultCallback& callback) {
  base::DictionaryValue input;
  input.SetString(kPrivetV3KeyPairing,
                  extensions::api::gcd_private::ToString(pairing_type));
  input.SetString(kPrivetV3KeyCrypto, kPrivetV3CryptoP224Spake2);

  StartPostRequest(kPrivetV3PairingStartPath, input,
                   base::Bind(&PrivetV3Session::OnPairingStartDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnPairingStartDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  if (!response.GetString(kPrivetV3KeySessionId, &session_id_) ||
      !GetDecodedString(response, kPrivetV3KeyDeviceCommitment, &commitment_)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  return callback.Run(Result::STATUS_SUCCESS);
}

void PrivetV3Session::ConfirmCode(const std::string& code,
                                  const ResultCallback& callback) {
  if (session_id_.empty()) {
    LOG(ERROR) << "Pairing is not started";
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  spake_.reset(new crypto::P224EncryptedKeyExchange(
      crypto::P224EncryptedKeyExchange::kPeerTypeClient, code));

  base::DictionaryValue input;
  input.SetString(kPrivetV3KeySessionId, session_id_);

  std::string client_commitment;
  base::Base64Encode(spake_->GetNextMessage(), &client_commitment);
  input.SetString(kPrivetV3KeyClientCommitment, client_commitment);

  // Call ProcessMessage after GetNextMessage().
  crypto::P224EncryptedKeyExchange::Result result =
      spake_->ProcessMessage(commitment_);
  DCHECK_EQ(result, crypto::P224EncryptedKeyExchange::kResultPending);

  StartPostRequest(kPrivetV3PairingConfirmPath, input,
                   base::Bind(&PrivetV3Session::OnPairingConfirmDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnPairingConfirmDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  std::string fingerprint;
  std::string signature;
  if (!GetDecodedString(response, kPrivetV3KeyCertFingerprint, &fingerprint) ||
      !GetDecodedString(response, kPrivetV3KeyCertSignature, &signature)) {
    LOG(ERROR) << "Invalid response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  net::SHA256HashValue hash;
  if (fingerprint.size() != sizeof(hash.data)) {
    LOG(ERROR) << "Invalid fingerprint size: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }
  memcpy(hash.data, fingerprint.data(), sizeof(hash.data));

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  // Key will be verified below, using HMAC.
  const std::string& key = spake_->GetUnverifiedKey();
  if (!hmac.Init(reinterpret_cast<const unsigned char*>(key.c_str()),
                 key.size()) ||
      !hmac.Verify(fingerprint, signature)) {
    LOG(ERROR) << "Verification failed: " << response;
    return callback.Run(Result::STATUS_BADPAIRINGCODEERROR);
  }

  std::string auth_code(hmac.DigestLength(), ' ');
  if (!hmac.Sign(session_id_,
                 reinterpret_cast<unsigned char*>(string_as_array(&auth_code)),
                 auth_code.size())) {
    LOG(FATAL) << "Signing failed";
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  VLOG(1) << "Expected certificate: " << fingerprint;
  context_getter_->AddPairedHost(
      host_port_.host(), hash,
      base::Bind(&PrivetV3Session::OnPairedHostAddedToContext,
                 weak_ptr_factory_.GetWeakPtr(), auth_code, callback));
}

void PrivetV3Session::OnPairedHostAddedToContext(
    const std::string& auth_code,
    const ResultCallback& callback) {
  SwitchToHttps();

  std::string auth_code_base64;
  base::Base64Encode(auth_code, &auth_code_base64);

  base::DictionaryValue input;
  input.SetString(kPrivetV3KeyAuthCode, auth_code_base64);
  input.SetString(kPrivetV3KeyMode, kPrivetV3KeyPairing);
  input.SetString(kPrivetV3KeyRequestedScope, kPrivetV3Auto);

  // Now we can use SendMessage with certificate validation.
  SendMessage(kPrivetV3AuthPath, input,
              base::Bind(&PrivetV3Session::OnAuthenticateDone,
                         weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnAuthenticateDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  std::string access_token;
  std::string token_type;
  std::string scope;
  if (!response.GetString(kPrivetV3KeyAccessToken, &access_token) ||
      !response.GetString(kPrivetV3KeyTokenType, &token_type) ||
      !response.GetString(kPrivetV3KeyScope, &scope)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  privet_auth_token_ = token_type + " " + access_token;

  return callback.Run(Result::STATUS_SUCCESS);
}

void PrivetV3Session::SendMessage(const std::string& api,
                                  const base::DictionaryValue& input,
                                  const MessageCallback& callback) {
  if (!use_https_) {
    LOG(ERROR) << "Session is not paired";
    return callback.Run(Result::STATUS_SESSIONERROR, base::DictionaryValue());
  }

  StartPostRequest(api, input, callback);
}

void PrivetV3Session::RunCallback(const base::Closure& callback) {
  callback.Run();
}

void PrivetV3Session::StartGetRequest(const std::string& api,
                                      const MessageCallback& callback) {
  net::URLFetcher* fetcher =
      CreateFetcher(api, net::URLFetcher::RequestType::GET, callback);
  if (!fetcher)
    return;
  fetcher->Start();
}

void PrivetV3Session::StartPostRequest(const std::string& api,
                                       const base::DictionaryValue& input,
                                       const MessageCallback& callback) {
  if (!on_post_data_.is_null())
    on_post_data_.Run(input);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      input, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  net::URLFetcher* fetcher =
      CreateFetcher(api, net::URLFetcher::RequestType::POST, callback);
  if (!fetcher)
    return;
  fetcher->SetUploadData(kContentTypeJSON, json);
  fetcher->Start();
}

void PrivetV3Session::SwitchToHttps() {
  host_port_.set_port(https_port_);
  use_https_ = true;
}

GURL PrivetV3Session::CreatePrivetURL(const std::string& path) const {
  GURL url("http://host/");
  GURL::Replacements replacements;
  std::string schema = use_https_ ? "https" : "http";
  replacements.SetSchemeStr(schema);
  std::string host = host_port_.HostForURL();
  replacements.SetHostStr(host);
  std::string port = base::UintToString(host_port_.port());
  replacements.SetPortStr(port);
  replacements.SetPathStr(path);
  url = url.ReplaceComponents(replacements);
  DCHECK(url.is_valid()) << url;
  return url;
}

net::URLFetcher* PrivetV3Session::CreateFetcher(
    const std::string& api,
    net::URLFetcher::RequestType request_type,
    const MessageCallback& callback) {
  GURL url = CreatePrivetURL(api);
  if (!url.is_valid()) {
    callback.Run(Result::STATUS_SESSIONERROR, base::DictionaryValue());
    return nullptr;
  }

  // Don't abort cancel requests after session object is destroyed.
  const bool orphaned = (api == kPrivetV3PairingCancelPath);
  FetcherDelegate* fetcher =
      new FetcherDelegate(weak_ptr_factory_.GetWeakPtr(), callback);
  if (!orphaned)
    fetchers_.push_back(fetcher);
  net::URLFetcher* url_fetcher =
      fetcher->CreateURLFetcher(url, request_type, orphaned);
  url_fetcher->SetLoadFlags(url_fetcher->GetLoadFlags() |
                            net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE |
                            net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher->SetRequestContext(context_getter_.get());
  url_fetcher->AddExtraRequestHeader(
      std::string(kPrivetV3AuthTokenHeaderPrefix) + privet_auth_token_);

  return url_fetcher;
}

void PrivetV3Session::DeleteFetcher(const FetcherDelegate* fetcher) {
  fetchers_.erase(std::find(fetchers_.begin(), fetchers_.end(), fetcher));
}

void PrivetV3Session::Cancel() {
  // Only session with pairing in process needs to be canceled. Paired sessions
  // (in https mode) does not need to be canceled.
  if (session_id_.empty() || use_https_)
    return;
  base::DictionaryValue input;
  input.SetString(kPrivetV3KeySessionId, session_id_);
  StartPostRequest(kPrivetV3PairingCancelPath, input, MessageCallback());
}

}  // namespace extensions
