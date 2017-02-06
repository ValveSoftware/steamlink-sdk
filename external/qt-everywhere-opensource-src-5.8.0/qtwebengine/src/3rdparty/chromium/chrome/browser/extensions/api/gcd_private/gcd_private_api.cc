// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"
#include "chrome/browser/extensions/api/gcd_private/privet_v3_session.h"
#include "chrome/browser/local_discovery/endpoint_resolver.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"

namespace extensions {

namespace gcd_private = api::gcd_private;

namespace {

const char kPrivatAPISetup[] = "/privet/v3/setup/start";
const char kPrivetKeyWifi[] = "wifi";
const char kPrivetKeyPassphrase[] = "passphrase";
const char kPrivetKeySSID[] = "ssid";

base::LazyInstance<BrowserContextKeyedAPIFactory<GcdPrivateAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

class GcdPrivateSessionHolder;

class GcdPrivateAPIImpl {
 public:
  typedef base::Callback<void(bool success)> SuccessCallback;

  typedef base::Callback<void(int session_id,
                              api::gcd_private::Status status,
                              const base::DictionaryValue& info)>
      CreateSessionCallback;

  typedef base::Callback<void(api::gcd_private::Status status)> SessionCallback;

  typedef base::Callback<void(api::gcd_private::Status status,
                              const base::DictionaryValue& response)>
      MessageResponseCallback;

  explicit GcdPrivateAPIImpl(content::BrowserContext* context);
  virtual ~GcdPrivateAPIImpl();

  static GcdPrivateAPIImpl* Get(content::BrowserContext* context);

  void CreateSession(const std::string& service_name,
                     const CreateSessionCallback& callback);

  void StartPairing(int session_id,
                    api::gcd_private::PairingType pairing_type,
                    const SessionCallback& callback);

  void ConfirmCode(int session_id,
                   const std::string& code,
                   const SessionCallback& callback);

  void SendMessage(int session_id,
                   const std::string& api,
                   const base::DictionaryValue& input,
                   const MessageResponseCallback& callback);

  void RemoveSession(int session_id);
  void RemoveSessionDelayed(int session_id);

  std::unique_ptr<base::ListValue> GetPrefetchedSSIDList();

 private:
  typedef std::map<std::string /* ssid */, std::string /* password */>
      PasswordMap;

  void SendMessageInternal(int session_id,
                           const std::string& api,
                           const base::DictionaryValue& input,
                           const MessageResponseCallback& callback);

  void OnServiceResolved(int session_id,
                         const CreateSessionCallback& callback,
                         const net::IPEndPoint& endpoint);

  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;

  struct SessionInfo {
    linked_ptr<PrivetV3Session> session;
    linked_ptr<local_discovery::EndpointResolver> resolver;
  };

  std::map<int, SessionInfo> sessions_;
  int last_session_id_ = 0;

  content::BrowserContext* const browser_context_;

  scoped_refptr<PrivetV3ContextGetter> context_getter_;

  base::WeakPtrFactory<GcdPrivateAPIImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GcdPrivateAPIImpl);
};

GcdPrivateAPIImpl::GcdPrivateAPIImpl(content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK(browser_context_);
}

GcdPrivateAPIImpl::~GcdPrivateAPIImpl() {
}

// static
GcdPrivateAPIImpl* GcdPrivateAPIImpl::Get(content::BrowserContext* context) {
  GcdPrivateAPI* gcd_api =
      BrowserContextKeyedAPIFactory<GcdPrivateAPI>::Get(context);
  return gcd_api ? gcd_api->impl_.get() : NULL;
}

void GcdPrivateAPIImpl::CreateSession(const std::string& service_name,
                                      const CreateSessionCallback& callback) {
  int session_id = last_session_id_++;
  auto& session_data = sessions_[session_id];
  session_data.resolver.reset(new local_discovery::EndpointResolver());
  session_data.resolver->Start(
      service_name, base::Bind(&GcdPrivateAPIImpl::OnServiceResolved,
                               base::Unretained(this), session_id, callback));
}

void GcdPrivateAPIImpl::OnServiceResolved(int session_id,
                                          const CreateSessionCallback& callback,
                                          const net::IPEndPoint& endpoint) {
  if (endpoint.address().empty()) {
    return callback.Run(session_id, gcd_private::STATUS_SERVICERESOLUTIONERROR,
                        base::DictionaryValue());
  }
  auto& session_data = sessions_[session_id];

  if (!context_getter_) {
    context_getter_ = new PrivetV3ContextGetter(
        content::BrowserContext::GetDefaultStoragePartition(browser_context_)->
            GetURLRequestContext()->GetNetworkTaskRunner());
  }

  session_data.session.reset(new PrivetV3Session(
      context_getter_, net::HostPortPair::FromIPEndPoint(endpoint)));
  session_data.session->Init(base::Bind(callback, session_id));
}

void GcdPrivateAPIImpl::StartPairing(int session_id,
                                     api::gcd_private::PairingType pairing_type,
                                     const SessionCallback& callback) {
  auto found = sessions_.find(session_id);

  if (found == sessions_.end())
    return callback.Run(gcd_private::STATUS_UNKNOWNSESSIONERROR);

  found->second.session->StartPairing(pairing_type, callback);
}

void GcdPrivateAPIImpl::ConfirmCode(int session_id,
                                    const std::string& code,
                                    const SessionCallback& callback) {
  auto found = sessions_.find(session_id);

  if (found == sessions_.end())
    return callback.Run(gcd_private::STATUS_UNKNOWNSESSIONERROR);

  found->second.session->ConfirmCode(code, callback);
}

void GcdPrivateAPIImpl::SendMessage(int session_id,
                                    const std::string& api,
                                    const base::DictionaryValue& input,
                                    const MessageResponseCallback& callback) {
  const base::DictionaryValue* input_actual = &input;
  std::unique_ptr<base::DictionaryValue> input_cloned;

  if (api == kPrivatAPISetup) {
    const base::DictionaryValue* wifi = NULL;

    if (input.GetDictionary(kPrivetKeyWifi, &wifi)) {
      std::string ssid;

      if (!wifi->GetString(kPrivetKeySSID, &ssid)) {
        LOG(ERROR) << "Missing " << kPrivetKeySSID;
        return callback.Run(gcd_private::STATUS_SETUPPARSEERROR,
                            base::DictionaryValue());
      }

      if (!wifi->HasKey(kPrivetKeyPassphrase)) {
        LOG(ERROR) << "Password is unknown";
        return callback.Run(gcd_private::STATUS_WIFIPASSWORDERROR,
                            base::DictionaryValue());
      }
    }
  }

  auto found = sessions_.find(session_id);

  if (found == sessions_.end()) {
    return callback.Run(gcd_private::STATUS_UNKNOWNSESSIONERROR,
                        base::DictionaryValue());
  }

  found->second.session->SendMessage(api, *input_actual, callback);
}

void GcdPrivateAPIImpl::RemoveSession(int session_id) {
  sessions_.erase(session_id);
}

void GcdPrivateAPIImpl::RemoveSessionDelayed(int session_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&GcdPrivateAPIImpl::RemoveSession,
                            weak_ptr_factory_.GetWeakPtr(), session_id));
}

std::unique_ptr<base::ListValue> GcdPrivateAPIImpl::GetPrefetchedSSIDList() {
  std::unique_ptr<base::ListValue> retval(new base::ListValue);

  return retval;
}



GcdPrivateAPI::GcdPrivateAPI(content::BrowserContext* context)
    : impl_(new GcdPrivateAPIImpl(context)) {
}

GcdPrivateAPI::~GcdPrivateAPI() {
}

// static
BrowserContextKeyedAPIFactory<GcdPrivateAPI>*
GcdPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

GcdPrivateGetDeviceInfoFunction::GcdPrivateGetDeviceInfoFunction() {
}

GcdPrivateGetDeviceInfoFunction::~GcdPrivateGetDeviceInfoFunction() {
}

bool GcdPrivateGetDeviceInfoFunction::RunAsync() {
  std::unique_ptr<gcd_private::CreateSession::Params> params =
      gcd_private::CreateSession::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  GcdPrivateAPIImpl::CreateSessionCallback callback =
      base::Bind(&GcdPrivateGetDeviceInfoFunction::OnSessionInitialized, this);
  gcd_api->CreateSession(params->service_name, callback);

  return true;
}

void GcdPrivateGetDeviceInfoFunction::OnSessionInitialized(
    int session_id,
    api::gcd_private::Status status,
    const base::DictionaryValue& info) {
  gcd_private::GetDeviceInfo::Results::DeviceInfo device_info;
  device_info.additional_properties.MergeDictionary(&info);

  results_ = gcd_private::GetDeviceInfo::Results::Create(status, device_info);
  SendResponse(true);

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());
  gcd_api->RemoveSessionDelayed(session_id);
}

GcdPrivateCreateSessionFunction::GcdPrivateCreateSessionFunction() {
}

GcdPrivateCreateSessionFunction::~GcdPrivateCreateSessionFunction() {
}

bool GcdPrivateCreateSessionFunction::RunAsync() {
  std::unique_ptr<gcd_private::CreateSession::Params> params =
      gcd_private::CreateSession::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  GcdPrivateAPIImpl::CreateSessionCallback callback =
      base::Bind(&GcdPrivateCreateSessionFunction::OnSessionInitialized, this);
  gcd_api->CreateSession(params->service_name, callback);

  return true;
}

void GcdPrivateCreateSessionFunction::OnSessionInitialized(
    int session_id,
    api::gcd_private::Status status,
    const base::DictionaryValue& info) {
  std::vector<api::gcd_private::PairingType> pairing_types;

  // TODO(vitalybuka): Remove this parsing and |pairing_types| from callback.
  if (status == gcd_private::STATUS_SUCCESS) {
    const base::ListValue* pairing = nullptr;
    if (info.GetList("authentication.pairing", &pairing)) {
      for (const auto& value : *pairing) {
        std::string pairing_string;
        if (value->GetAsString(&pairing_string)) {
          api::gcd_private::PairingType pairing_type =
              api::gcd_private::ParsePairingType(pairing_string);
          if (pairing_type != api::gcd_private::PAIRING_TYPE_NONE)
            pairing_types.push_back(pairing_type);
        }
      }
    } else {
      status = gcd_private::STATUS_SESSIONERROR;
    }
  }

  results_ = gcd_private::CreateSession::Results::Create(session_id, status,
                                                         pairing_types);
  SendResponse(true);
}

GcdPrivateStartPairingFunction::GcdPrivateStartPairingFunction() {
}

GcdPrivateStartPairingFunction::~GcdPrivateStartPairingFunction() {
}

bool GcdPrivateStartPairingFunction::RunAsync() {
  std::unique_ptr<gcd_private::StartPairing::Params> params =
      gcd_private::StartPairing::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->StartPairing(
      params->session_id, params->pairing_type,
      base::Bind(&GcdPrivateStartPairingFunction::OnPairingStarted, this));

  return true;
}

void GcdPrivateStartPairingFunction::OnPairingStarted(
    api::gcd_private::Status status) {
  results_ = gcd_private::StartPairing::Results::Create(status);
  SendResponse(true);
}

GcdPrivateConfirmCodeFunction::GcdPrivateConfirmCodeFunction() {
}

GcdPrivateConfirmCodeFunction::~GcdPrivateConfirmCodeFunction() {
}

bool GcdPrivateConfirmCodeFunction::RunAsync() {
  std::unique_ptr<gcd_private::ConfirmCode::Params> params =
      gcd_private::ConfirmCode::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->ConfirmCode(
      params->session_id, params->code,
      base::Bind(&GcdPrivateConfirmCodeFunction::OnCodeConfirmed, this));

  return true;
}

void GcdPrivateConfirmCodeFunction::OnCodeConfirmed(
    api::gcd_private::Status status) {
  results_ = gcd_private::ConfirmCode::Results::Create(status);
  SendResponse(true);
}

GcdPrivateSendMessageFunction::GcdPrivateSendMessageFunction() {
}

GcdPrivateSendMessageFunction::~GcdPrivateSendMessageFunction() {
}

bool GcdPrivateSendMessageFunction::RunAsync() {
  std::unique_ptr<gcd_private::PassMessage::Params> params =
      gcd_private::PassMessage::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());


  gcd_api->SendMessage(
      params->session_id,
      params->api,
      params->input.additional_properties,
      base::Bind(&GcdPrivateSendMessageFunction::OnMessageSentCallback, this));

  return true;
}

void GcdPrivateSendMessageFunction::OnMessageSentCallback(
    api::gcd_private::Status status,
    const base::DictionaryValue& value) {
  gcd_private::PassMessage::Results::Response response;
  response.additional_properties.MergeDictionary(&value);

  results_ = gcd_private::PassMessage::Results::Create(status, response);
  SendResponse(true);
}

GcdPrivateTerminateSessionFunction::GcdPrivateTerminateSessionFunction() {
}

GcdPrivateTerminateSessionFunction::~GcdPrivateTerminateSessionFunction() {
}

bool GcdPrivateTerminateSessionFunction::RunAsync() {
  std::unique_ptr<gcd_private::TerminateSession::Params> params =
      gcd_private::TerminateSession::Params::Create(*args_);

  if (!params)
    return false;

  GcdPrivateAPIImpl* gcd_api = GcdPrivateAPIImpl::Get(GetProfile());

  gcd_api->RemoveSession(params->session_id);

  SendResponse(true);
  return true;
}

}  // namespace extensions
