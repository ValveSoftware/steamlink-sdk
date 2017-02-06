// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi_sync/wifi_credential.h"

#include "base/i18n/streaming_utf8_validator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace wifi_sync {

WifiCredential::WifiCredential(const WifiCredential& other) = default;

WifiCredential::~WifiCredential() {
}

// static
std::unique_ptr<WifiCredential> WifiCredential::Create(
    const SsidBytes& ssid,
    WifiSecurityClass security_class,
    const std::string& passphrase) {
  if (security_class == SECURITY_CLASS_INVALID) {
    LOG(ERROR) << "SecurityClass is invalid.";
    return nullptr;
  }

  if (!base::StreamingUtf8Validator::Validate(passphrase)) {
    LOG(ERROR) << "Passphrase is not valid UTF-8";
    return nullptr;
  }

  return base::WrapUnique(new WifiCredential(ssid, security_class, passphrase));
}

std::unique_ptr<base::DictionaryValue> WifiCredential::ToOncProperties() const {
  const std::string ssid_utf8(ssid().begin(), ssid().end());
  // TODO(quiche): Remove this test, once ONC suports non-UTF-8 SSIDs.
  // crbug.com/432546.
  if (!base::StreamingUtf8Validator::Validate(ssid_utf8)) {
    LOG(ERROR) << "SSID is not valid UTF-8";
    return nullptr;
  }

  std::string onc_security;
  if (!WifiSecurityClassToOncSecurityString(security_class(), &onc_security)) {
    NOTREACHED() << "Failed to convert SecurityClass with value "
                 << security_class();
    return base::WrapUnique(new base::DictionaryValue());
  }

  std::unique_ptr<base::DictionaryValue> onc_properties(
      new base::DictionaryValue());
  onc_properties->Set(onc::toplevel_config::kType,
                      new base::StringValue(onc::network_type::kWiFi));
  // TODO(quiche): Switch to the HexSSID property, once ONC fully supports it.
  // crbug.com/432546.
  onc_properties->Set(onc::network_config::WifiProperty(onc::wifi::kSSID),
                      new base::StringValue(ssid_utf8));
  onc_properties->Set(onc::network_config::WifiProperty(onc::wifi::kSecurity),
                      new base::StringValue(onc_security));
  if (WifiSecurityClassSupportsPassphrases(security_class())) {
    onc_properties->Set(
        onc::network_config::WifiProperty(onc::wifi::kPassphrase),
        new base::StringValue(passphrase()));
  }
  return onc_properties;
}

std::string WifiCredential::ToString() const {
  return base::StringPrintf(
      "[SSID (hex): %s, SecurityClass: %d]",
      base::HexEncode(&ssid_.front(), ssid_.size()).c_str(),
      security_class_);  // Passphrase deliberately omitted.
}

// static
bool WifiCredential::IsLessThan(
    const WifiCredential& a, const WifiCredential& b) {
  return a.ssid_ < b.ssid_ ||
      a.security_class_< b.security_class_ ||
      a.passphrase_ < b.passphrase_;
}

// static
WifiCredential::CredentialSet WifiCredential::MakeSet() {
  return CredentialSet(WifiCredential::IsLessThan);
}

// static
WifiCredential::SsidBytes WifiCredential::MakeSsidBytesForTest(
    const std::string& ssid) {
  return SsidBytes(ssid.begin(), ssid.end());
}

// Private methods.

WifiCredential::WifiCredential(
    const std::vector<unsigned char>& ssid,
    WifiSecurityClass security_class,
    const std::string& passphrase)
    : ssid_(ssid),
      security_class_(security_class),
      passphrase_(passphrase) {
}

}  // namespace wifi_sync
