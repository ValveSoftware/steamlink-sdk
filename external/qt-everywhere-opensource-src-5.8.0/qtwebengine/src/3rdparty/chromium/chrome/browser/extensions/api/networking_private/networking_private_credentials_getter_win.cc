// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;
using extensions::NetworkingPrivateCredentialsGetter;

namespace {

class CredentialsGetterHostClient : public UtilityProcessHostClient {
 public:
  explicit CredentialsGetterHostClient(const std::string& public_key);

  // UtilityProcessHostClient
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed(int error_code) override;

  // IPC message handlers.
  void OnGotCredentials(const std::string& key_data, bool success);

  // Starts the utility process that gets wifi passphrase from system.
  void StartProcessOnIOThread(
      const std::string& network_guid,
      const NetworkingPrivateCredentialsGetter::CredentialsCallback& callback);

 private:
  ~CredentialsGetterHostClient() override;

  // Public key used to encrypt results
  std::vector<uint8_t> public_key_;

  // Callback for reporting the result.
  NetworkingPrivateCredentialsGetter::CredentialsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsGetterHostClient);
};

CredentialsGetterHostClient::CredentialsGetterHostClient(
    const std::string& public_key)
    : public_key_(public_key.begin(), public_key.end()) {
}

bool CredentialsGetterHostClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialsGetterHostClient, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotWiFiCredentials, OnGotCredentials)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialsGetterHostClient::OnProcessCrashed(int exit_code) {
  callback_.Run(
      "", base::StringPrintf("Process Crashed with code %08x.", exit_code));
}

void CredentialsGetterHostClient::OnProcessLaunchFailed(int error_code) {
  callback_.Run("", base::StringPrintf("Process Launch Failed with code %08x.",
                                       error_code));
}

void CredentialsGetterHostClient::OnGotCredentials(const std::string& key_data,
                                                   bool success) {
  if (success) {
    std::vector<uint8_t> ciphertext;
    if (!networking_private_crypto::EncryptByteString(
            public_key_, key_data, &ciphertext)) {
      callback_.Run("", "Encrypt Credentials Failed");
      return;
    }

    std::string base64_encoded_key_data;
    base::Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(ciphertext.data()),
                          ciphertext.size()),
        &base64_encoded_key_data);
    callback_.Run(base64_encoded_key_data, "");
  } else {
    callback_.Run("", "Get Credentials Failed");
  }
}

void CredentialsGetterHostClient::StartProcessOnIOThread(
    const std::string& network_guid,
    const NetworkingPrivateCredentialsGetter::CredentialsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback_ = callback;
  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::ThreadTaskRunnerHandle::Get());
  host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_WIFI_CREDENTIALS_GETTER_NAME));
  host->ElevatePrivileges();
  host->Send(new ChromeUtilityHostMsg_GetWiFiCredentials(network_guid));
}

CredentialsGetterHostClient::~CredentialsGetterHostClient() {
}

}  // namespace

namespace extensions {

class NetworkingPrivateCredentialsGetterWin
    : public NetworkingPrivateCredentialsGetter {
 public:
  NetworkingPrivateCredentialsGetterWin();

  void Start(const std::string& network_guid,
             const std::string& public_key,
             const CredentialsCallback& callback) override;

 private:
  ~NetworkingPrivateCredentialsGetterWin() override;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCredentialsGetterWin);
};

NetworkingPrivateCredentialsGetterWin::NetworkingPrivateCredentialsGetterWin() {
}

void NetworkingPrivateCredentialsGetterWin::Start(
    const std::string& network_guid,
    const std::string& public_key,
    const CredentialsCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CredentialsGetterHostClient::StartProcessOnIOThread,
                 new CredentialsGetterHostClient(public_key),
                 network_guid,
                 callback));
}

NetworkingPrivateCredentialsGetterWin::
    ~NetworkingPrivateCredentialsGetterWin() {}

NetworkingPrivateCredentialsGetter*
NetworkingPrivateCredentialsGetter::Create() {
  return new NetworkingPrivateCredentialsGetterWin();
}

}  // namespace extensions
