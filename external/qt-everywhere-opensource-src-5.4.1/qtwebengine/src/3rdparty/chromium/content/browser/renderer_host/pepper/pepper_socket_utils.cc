// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "net/cert/x509_certificate.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"

namespace content {
namespace pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr) {
  std::string host =
      ppapi::NetAddressPrivateImpl::DescribeNetAddress(net_addr, false);
  int port = 0;
  std::vector<unsigned char> address;
  ppapi::NetAddressPrivateImpl::NetAddressToIPEndPoint(
      net_addr, &address, &port);
  return SocketPermissionRequest(type, host, port);
}

bool CanUseSocketAPIs(bool external_plugin,
                      bool private_api,
                      const SocketPermissionRequest* params,
                      int render_process_id,
                      int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!external_plugin) {
    // Always allow socket APIs for out-process plugins (other than external
    // plugins instantiated by the embeeder through
    // BrowserPpapiHost::CreateExternalPluginProcess).
    return true;
  }

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return false;
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance)
    return false;
  if (!GetContentClient()->browser()->AllowPepperSocketAPI(
          site_instance->GetBrowserContext(),
          site_instance->GetSiteURL(),
          private_api,
          params)) {
    LOG(ERROR) << "Host " << site_instance->GetSiteURL().host()
               << " cannot use socket API or destination is not allowed";
    return false;
  }

  return true;
}

bool GetCertificateFields(const net::X509Certificate& cert,
                          ppapi::PPB_X509Certificate_Fields* fields) {
  const net::CertPrincipal& issuer = cert.issuer();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME,
                   new base::StringValue(issuer.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_LOCALITY_NAME,
                   new base::StringValue(issuer.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_STATE_OR_PROVINCE_NAME,
                   new base::StringValue(issuer.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COUNTRY_NAME,
                   new base::StringValue(issuer.country_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_NAME,
      new base::StringValue(JoinString(issuer.organization_names, '\n')));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(issuer.organization_unit_names, '\n')));

  const net::CertPrincipal& subject = cert.subject();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME,
                   new base::StringValue(subject.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_LOCALITY_NAME,
                   new base::StringValue(subject.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_STATE_OR_PROVINCE_NAME,
                   new base::StringValue(subject.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COUNTRY_NAME,
                   new base::StringValue(subject.country_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_NAME,
      new base::StringValue(JoinString(subject.organization_names, '\n')));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(subject.organization_unit_names, '\n')));

  const std::string& serial_number = cert.serial_number();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SERIAL_NUMBER,
                   base::BinaryValue::CreateWithCopiedBuffer(
                       serial_number.data(), serial_number.length()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_BEFORE,
                   new base::FundamentalValue(cert.valid_start().ToDoubleT()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_AFTER,
                   new base::FundamentalValue(cert.valid_expiry().ToDoubleT()));
  std::string der;
  net::X509Certificate::GetDEREncoded(cert.os_cert_handle(), &der);
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_RAW,
      base::BinaryValue::CreateWithCopiedBuffer(der.data(), der.length()));
  return true;
}

bool GetCertificateFields(const char* der,
                          uint32_t length,
                          ppapi::PPB_X509Certificate_Fields* fields) {
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(der, length);
  if (!cert.get())
    return false;
  return GetCertificateFields(*cert.get(), fields);
}

}  // namespace pepper_socket_utils
}  // namespace content
