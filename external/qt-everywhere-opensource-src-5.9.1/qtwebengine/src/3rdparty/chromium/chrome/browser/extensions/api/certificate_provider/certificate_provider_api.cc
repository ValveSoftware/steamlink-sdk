// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "chrome/common/extensions/api/certificate_provider_internal.h"
#include "content/public/common/console_message_level.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace extensions {

namespace api_cp = api::certificate_provider;
namespace api_cpi = api::certificate_provider_internal;

namespace {

const char kErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kErrorECDSANotSupported[] = "Key type ECDSA not supported.";
const char kErrorUnknownKeyType[] = "Key type unknown.";
const char kErrorAborted[] = "Request was aborted.";
const char kErrorTimeout[] = "Request timed out, reply rejected.";

}  // namespace

CertificateProviderInternalReportCertificatesFunction::
    ~CertificateProviderInternalReportCertificatesFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportCertificatesFunction::Run() {
  std::unique_ptr<api_cpi::ReportCertificates::Params> params(
      api_cpi::ReportCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  if (!params->certificates) {
    // In the public API, the certificates parameter is mandatory. We only run
    // into this case, if the custom binding rejected the reply by the
    // extension.
    return RespondNow(Error(kErrorAborted));
  }

  chromeos::certificate_provider::CertificateInfoList cert_infos;
  std::vector<std::vector<char>> rejected_certificates;
  for (const api_cp::CertificateInfo& input_cert_info : *params->certificates) {
    chromeos::certificate_provider::CertificateInfo parsed_cert_info;

    if (ParseCertificateInfo(input_cert_info, &parsed_cert_info))
      cert_infos.push_back(parsed_cert_info);
    else
      rejected_certificates.push_back(input_cert_info.certificate);
  }

  if (service->SetCertificatesProvidedByExtension(
          extension_id(), params->request_id, cert_infos)) {
    return RespondNow(ArgumentList(
        api_cpi::ReportCertificates::Results::Create(rejected_certificates)));
  } else {
    // The custom binding already checks for multiple reports to the same
    // request. The only remaining case, why this reply can fail is that the
    // request timed out.
    return RespondNow(Error(kErrorTimeout));
  }
}

bool CertificateProviderInternalReportCertificatesFunction::
    ParseCertificateInfo(
        const api_cp::CertificateInfo& info,
        chromeos::certificate_provider::CertificateInfo* out_info) {
  const std::vector<char>& cert_der = info.certificate;
  if (cert_der.empty()) {
    WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR, kErrorInvalidX509Cert);
    return false;
  }

  out_info->certificate =
      net::X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());
  if (!out_info->certificate) {
    WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR, kErrorInvalidX509Cert);
    return false;
  }

  size_t public_key_length_in_bits = 0;
  net::X509Certificate::PublicKeyType type =
      net::X509Certificate::kPublicKeyTypeUnknown;
  net::X509Certificate::GetPublicKeyInfo(
      out_info->certificate->os_cert_handle(), &public_key_length_in_bits,
      &type);

  switch (type) {
    case net::X509Certificate::kPublicKeyTypeRSA:
      DCHECK(public_key_length_in_bits);

      // Convert bits to bytes.
      out_info->max_signature_length_in_bytes =
          (public_key_length_in_bits + 7) / 8;
      out_info->type = net::SSLPrivateKey::Type::RSA;
      break;
    case net::X509Certificate::kPublicKeyTypeECDSA:
      WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR,
                     kErrorECDSANotSupported);
      return false;
    default:
      WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR,
                     kErrorUnknownKeyType);
      return false;
  }

  for (const api_cp::Hash hash : info.supported_hashes) {
    switch (hash) {
      case api_cp::HASH_MD5_SHA1:
        out_info->supported_hashes.push_back(
            net::SSLPrivateKey::Hash::MD5_SHA1);
        break;
      case api_cp::HASH_SHA1:
        out_info->supported_hashes.push_back(net::SSLPrivateKey::Hash::SHA1);
        break;
      case api_cp::HASH_SHA256:
        out_info->supported_hashes.push_back(net::SSLPrivateKey::Hash::SHA256);
        break;
      case api_cp::HASH_SHA384:
        out_info->supported_hashes.push_back(net::SSLPrivateKey::Hash::SHA384);
        break;
      case api_cp::HASH_SHA512:
        out_info->supported_hashes.push_back(net::SSLPrivateKey::Hash::SHA512);
        break;
      case api_cp::HASH_NONE:
        NOTREACHED();
        return false;
    }
  }
  return true;
}

CertificateProviderInternalReportSignatureFunction::
    ~CertificateProviderInternalReportSignatureFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportSignatureFunction::Run() {
  std::unique_ptr<api_cpi::ReportSignature::Params> params(
      api_cpi::ReportSignature::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  std::vector<uint8_t> signature;
  // If an error occurred, |signature| will not be set.
  if (params->signature)
    signature.assign(params->signature->begin(), params->signature->end());

  service->ReplyToSignRequest(extension_id(), params->request_id, signature);
  return RespondNow(NoArguments());
}

}  // namespace extensions
