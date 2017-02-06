// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/lazy_instance.h"
#include "chrome/browser/copresence/chrome_whispernet_client.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/audio_bus.h"

using audio_modem::WhispernetClient;
using content::BrowserThread;

namespace extensions {

namespace SendFound = api::copresence_private::SendFound;
namespace SendSamples = api::copresence_private::SendSamples;
namespace SendInitialized = api::copresence_private::SendInitialized;

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<CopresencePrivateService>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

void RunInitCallback(WhispernetClient* client, bool status) {
  DCHECK(client);
  audio_modem::SuccessCallback init_callback =
      client->GetInitializedCallback();
  if (!init_callback.is_null())
    init_callback.Run(status);
}

}  // namespace

CopresencePrivateService::CopresencePrivateService(
    content::BrowserContext* context)
    : initialized_(false) {}

CopresencePrivateService::~CopresencePrivateService() {}

const std::string CopresencePrivateService::RegisterWhispernetClient(
    WhispernetClient* client) {
  if (initialized_)
    RunInitCallback(client, true);

  std::string id = base::GenerateGUID();
  whispernet_clients_[id] = client;

  return id;
}

void CopresencePrivateService::OnWhispernetInitialized(bool success) {
  if (success)
    initialized_ = true;

  DVLOG(2) << "Notifying " << whispernet_clients_.size()
           << " clients that initialization is complete.";
  for (auto client_entry : whispernet_clients_)
    RunInitCallback(client_entry.second, success);
}

WhispernetClient* CopresencePrivateService::GetWhispernetClient(
    const std::string& id) {
  WhispernetClient* client = whispernet_clients_[id];
  DCHECK(client);
  return client;
}

// static
BrowserContextKeyedAPIFactory<CopresencePrivateService>*
CopresencePrivateService::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<CopresencePrivateService>
    ::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}


// Copresence Private functions.

// CopresenceSendFoundFunction implementation:
ExtensionFunction::ResponseAction CopresencePrivateSendFoundFunction::Run() {
  std::unique_ptr<SendFound::Params> params(SendFound::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WhispernetClient* whispernet_client =
      CopresencePrivateService::GetFactoryInstance()->Get(browser_context())
          ->GetWhispernetClient(params->client_id);
  if (whispernet_client->GetTokensCallback().is_null())
    return RespondNow(NoArguments());

  std::vector<audio_modem::AudioToken> tokens;
  for (size_t i = 0; i < params->tokens.size(); ++i) {
    tokens.push_back(audio_modem::AudioToken(params->tokens[i].token,
                                             params->tokens[i].audible));
  }
  whispernet_client->GetTokensCallback().Run(tokens);
  return RespondNow(NoArguments());
}

// CopresenceSendEncodedFunction implementation:
ExtensionFunction::ResponseAction CopresencePrivateSendSamplesFunction::Run() {
  std::unique_ptr<SendSamples::Params> params(
      SendSamples::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WhispernetClient* whispernet_client =
      CopresencePrivateService::GetFactoryInstance()->Get(browser_context())
          ->GetWhispernetClient(params->client_id);
  if (whispernet_client->GetSamplesCallback().is_null())
    return RespondNow(NoArguments());

  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1,  // Mono
                                        params->samples.size() / sizeof(float));

  memcpy(samples->channel(0), params->samples.data(), params->samples.size());

  whispernet_client->GetSamplesCallback().Run(
      params->token.audible ? audio_modem::AUDIBLE : audio_modem::INAUDIBLE,
      params->token.token, samples);
  return RespondNow(NoArguments());
}

// CopresenceSendInitializedFunction implementation:
ExtensionFunction::ResponseAction
CopresencePrivateSendInitializedFunction::Run() {
  std::unique_ptr<SendInitialized::Params> params(
      SendInitialized::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopresencePrivateService::GetFactoryInstance()->Get(browser_context())
      ->OnWhispernetInitialized(params->success);

  return RespondNow(NoArguments());
}

}  // namespace extensions
