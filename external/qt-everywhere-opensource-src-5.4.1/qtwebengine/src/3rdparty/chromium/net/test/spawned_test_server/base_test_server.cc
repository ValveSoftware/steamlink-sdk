// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/base_test_server.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/host_resolver.h"
#include "url/gurl.h"

namespace net {

namespace {

std::string GetHostname(BaseTestServer::Type type,
                        const BaseTestServer::SSLOptions& options) {
  if (BaseTestServer::UsingSSL(type) &&
      options.server_certificate ==
          BaseTestServer::SSLOptions::CERT_MISMATCHED_NAME) {
    // Return a different hostname string that resolves to the same hostname.
    return "localhost";
  }

  // Use the 127.0.0.1 as default.
  return BaseTestServer::kLocalhost;
}

std::string GetClientCertType(SSLClientCertType type) {
  switch (type) {
    case CLIENT_CERT_RSA_SIGN:
      return "rsa_sign";
    case CLIENT_CERT_DSS_SIGN:
      return "dss_sign";
    case CLIENT_CERT_ECDSA_SIGN:
      return "ecdsa_sign";
    default:
      NOTREACHED();
      return "";
  }
}

void GetKeyExchangesList(int key_exchange, base::ListValue* values) {
  if (key_exchange & BaseTestServer::SSLOptions::KEY_EXCHANGE_RSA)
    values->Append(new base::StringValue("rsa"));
  if (key_exchange & BaseTestServer::SSLOptions::KEY_EXCHANGE_DHE_RSA)
    values->Append(new base::StringValue("dhe_rsa"));
}

void GetCiphersList(int cipher, base::ListValue* values) {
  if (cipher & BaseTestServer::SSLOptions::BULK_CIPHER_RC4)
    values->Append(new base::StringValue("rc4"));
  if (cipher & BaseTestServer::SSLOptions::BULK_CIPHER_AES128)
    values->Append(new base::StringValue("aes128"));
  if (cipher & BaseTestServer::SSLOptions::BULK_CIPHER_AES256)
    values->Append(new base::StringValue("aes256"));
  if (cipher & BaseTestServer::SSLOptions::BULK_CIPHER_3DES)
    values->Append(new base::StringValue("3des"));
}

}  // namespace

BaseTestServer::SSLOptions::SSLOptions()
    : server_certificate(CERT_OK),
      ocsp_status(OCSP_OK),
      cert_serial(0),
      request_client_certificate(false),
      key_exchanges(SSLOptions::KEY_EXCHANGE_ANY),
      bulk_ciphers(SSLOptions::BULK_CIPHER_ANY),
      record_resume(false),
      tls_intolerant(TLS_INTOLERANT_NONE),
      fallback_scsv_enabled(false),
      staple_ocsp_response(false),
      enable_npn(false) {}

BaseTestServer::SSLOptions::SSLOptions(
    BaseTestServer::SSLOptions::ServerCertificate cert)
    : server_certificate(cert),
      ocsp_status(OCSP_OK),
      cert_serial(0),
      request_client_certificate(false),
      key_exchanges(SSLOptions::KEY_EXCHANGE_ANY),
      bulk_ciphers(SSLOptions::BULK_CIPHER_ANY),
      record_resume(false),
      tls_intolerant(TLS_INTOLERANT_NONE),
      fallback_scsv_enabled(false),
      staple_ocsp_response(false),
      enable_npn(false) {}

BaseTestServer::SSLOptions::~SSLOptions() {}

base::FilePath BaseTestServer::SSLOptions::GetCertificateFile() const {
  switch (server_certificate) {
    case CERT_OK:
    case CERT_MISMATCHED_NAME:
      return base::FilePath(FILE_PATH_LITERAL("ok_cert.pem"));
    case CERT_EXPIRED:
      return base::FilePath(FILE_PATH_LITERAL("expired_cert.pem"));
    case CERT_CHAIN_WRONG_ROOT:
      // This chain uses its own dedicated test root certificate to avoid
      // side-effects that may affect testing.
      return base::FilePath(FILE_PATH_LITERAL("redundant-server-chain.pem"));
    case CERT_AUTO:
      return base::FilePath();
    default:
      NOTREACHED();
  }
  return base::FilePath();
}

std::string BaseTestServer::SSLOptions::GetOCSPArgument() const {
  if (server_certificate != CERT_AUTO)
    return std::string();

  switch (ocsp_status) {
    case OCSP_OK:
      return "ok";
    case OCSP_REVOKED:
      return "revoked";
    case OCSP_INVALID:
      return "invalid";
    case OCSP_UNAUTHORIZED:
      return "unauthorized";
    case OCSP_UNKNOWN:
      return "unknown";
    default:
      NOTREACHED();
      return std::string();
  }
}

const char BaseTestServer::kLocalhost[] = "127.0.0.1";

BaseTestServer::BaseTestServer(Type type, const std::string& host)
    : type_(type),
      started_(false),
      log_to_console_(false) {
  Init(host);
}

BaseTestServer::BaseTestServer(Type type, const SSLOptions& ssl_options)
    : ssl_options_(ssl_options),
      type_(type),
      started_(false),
      log_to_console_(false) {
  DCHECK(UsingSSL(type));
  Init(GetHostname(type, ssl_options));
}

BaseTestServer::~BaseTestServer() {}

const HostPortPair& BaseTestServer::host_port_pair() const {
  DCHECK(started_);
  return host_port_pair_;
}

const base::DictionaryValue& BaseTestServer::server_data() const {
  DCHECK(started_);
  DCHECK(server_data_.get());
  return *server_data_;
}

std::string BaseTestServer::GetScheme() const {
  switch (type_) {
    case TYPE_FTP:
      return "ftp";
    case TYPE_HTTP:
      return "http";
    case TYPE_HTTPS:
      return "https";
    case TYPE_WS:
      return "ws";
    case TYPE_WSS:
      return "wss";
    case TYPE_TCP_ECHO:
    case TYPE_UDP_ECHO:
    default:
      NOTREACHED();
  }
  return std::string();
}

bool BaseTestServer::GetAddressList(AddressList* address_list) const {
  DCHECK(address_list);

  scoped_ptr<HostResolver> resolver(HostResolver::CreateDefaultResolver(NULL));
  HostResolver::RequestInfo info(host_port_pair_);
  TestCompletionCallback callback;
  int rv = resolver->Resolve(info,
                             DEFAULT_PRIORITY,
                             address_list,
                             callback.callback(),
                             NULL,
                             BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to resolve hostname: " << host_port_pair_.host();
    return false;
  }
  return true;
}

uint16 BaseTestServer::GetPort() {
  return host_port_pair_.port();
}

void BaseTestServer::SetPort(uint16 port) {
  host_port_pair_.set_port(port);
}

GURL BaseTestServer::GetURL(const std::string& path) const {
  return GURL(GetScheme() + "://" + host_port_pair_.ToString() + "/" + path);
}

GURL BaseTestServer::GetURLWithUser(const std::string& path,
                                const std::string& user) const {
  return GURL(GetScheme() + "://" + user + "@" + host_port_pair_.ToString() +
              "/" + path);
}

GURL BaseTestServer::GetURLWithUserAndPassword(const std::string& path,
                                           const std::string& user,
                                           const std::string& password) const {
  return GURL(GetScheme() + "://" + user + ":" + password + "@" +
              host_port_pair_.ToString() + "/" + path);
}

// static
bool BaseTestServer::GetFilePathWithReplacements(
    const std::string& original_file_path,
    const std::vector<StringPair>& text_to_replace,
    std::string* replacement_path) {
  std::string new_file_path = original_file_path;
  bool first_query_parameter = true;
  const std::vector<StringPair>::const_iterator end = text_to_replace.end();
  for (std::vector<StringPair>::const_iterator it = text_to_replace.begin();
       it != end;
       ++it) {
    const std::string& old_text = it->first;
    const std::string& new_text = it->second;
    std::string base64_old;
    std::string base64_new;
    base::Base64Encode(old_text, &base64_old);
    base::Base64Encode(new_text, &base64_new);
    if (first_query_parameter) {
      new_file_path += "?";
      first_query_parameter = false;
    } else {
      new_file_path += "&";
    }
    new_file_path += "replace_text=";
    new_file_path += base64_old;
    new_file_path += ":";
    new_file_path += base64_new;
  }

  *replacement_path = new_file_path;
  return true;
}

void BaseTestServer::Init(const std::string& host) {
  host_port_pair_ = HostPortPair(host, 0);

  // TODO(battre) Remove this after figuring out why the TestServer is flaky.
  // http://crbug.com/96594
  log_to_console_ = true;
}

void BaseTestServer::SetResourcePath(const base::FilePath& document_root,
                                     const base::FilePath& certificates_dir) {
  // This method shouldn't get called twice.
  DCHECK(certificates_dir_.empty());
  document_root_ = document_root;
  certificates_dir_ = certificates_dir;
  DCHECK(!certificates_dir_.empty());
}

bool BaseTestServer::ParseServerData(const std::string& server_data) {
  VLOG(1) << "Server data: " << server_data;
  base::JSONReader json_reader;
  scoped_ptr<base::Value> value(json_reader.ReadToValue(server_data));
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Could not parse server data: "
               << json_reader.GetErrorMessage();
    return false;
  }

  server_data_.reset(static_cast<base::DictionaryValue*>(value.release()));
  int port = 0;
  if (!server_data_->GetInteger("port", &port)) {
    LOG(ERROR) << "Could not find port value";
    return false;
  }
  if ((port <= 0) || (port > kuint16max)) {
    LOG(ERROR) << "Invalid port value: " << port;
    return false;
  }
  host_port_pair_.set_port(port);

  return true;
}

bool BaseTestServer::LoadTestRootCert() const {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  if (!root_certs)
    return false;

  // Should always use absolute path to load the root certificate.
  base::FilePath root_certificate_path = certificates_dir_;
  if (!certificates_dir_.IsAbsolute()) {
    base::FilePath src_dir;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
      return false;
    root_certificate_path = src_dir.Append(certificates_dir_);
  }

  return root_certs->AddFromFile(
      root_certificate_path.AppendASCII("root_ca_cert.pem"));
}

bool BaseTestServer::SetupWhenServerStarted() {
  DCHECK(host_port_pair_.port());

  if (UsingSSL(type_) && !LoadTestRootCert())
      return false;

  started_ = true;
  allowed_port_.reset(new ScopedPortException(host_port_pair_.port()));
  return true;
}

void BaseTestServer::CleanUpWhenStoppingServer() {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  root_certs->Clear();

  host_port_pair_.set_port(0);
  allowed_port_.reset();
  started_ = false;
}

// Generates a dictionary of arguments to pass to the Python test server via
// the test server spawner, in the form of
// { argument-name: argument-value, ... }
// Returns false if an invalid configuration is specified.
bool BaseTestServer::GenerateArguments(base::DictionaryValue* arguments) const {
  DCHECK(arguments);

  arguments->SetString("host", host_port_pair_.host());
  arguments->SetInteger("port", host_port_pair_.port());
  arguments->SetString("data-dir", document_root_.value());

  if (VLOG_IS_ON(1) || log_to_console_)
    arguments->Set("log-to-console", base::Value::CreateNullValue());

  if (UsingSSL(type_)) {
    // Check the certificate arguments of the HTTPS server.
    base::FilePath certificate_path(certificates_dir_);
    base::FilePath certificate_file(ssl_options_.GetCertificateFile());
    if (!certificate_file.value().empty()) {
      certificate_path = certificate_path.Append(certificate_file);
      if (certificate_path.IsAbsolute() &&
          !base::PathExists(certificate_path)) {
        LOG(ERROR) << "Certificate path " << certificate_path.value()
                   << " doesn't exist. Can't launch https server.";
        return false;
      }
      arguments->SetString("cert-and-key-file", certificate_path.value());
    }

    // Check the client certificate related arguments.
    if (ssl_options_.request_client_certificate)
      arguments->Set("ssl-client-auth", base::Value::CreateNullValue());
    scoped_ptr<base::ListValue> ssl_client_certs(new base::ListValue());

    std::vector<base::FilePath>::const_iterator it;
    for (it = ssl_options_.client_authorities.begin();
         it != ssl_options_.client_authorities.end(); ++it) {
      if (it->IsAbsolute() && !base::PathExists(*it)) {
        LOG(ERROR) << "Client authority path " << it->value()
                   << " doesn't exist. Can't launch https server.";
        return false;
      }
      ssl_client_certs->Append(new base::StringValue(it->value()));
    }

    if (ssl_client_certs->GetSize())
      arguments->Set("ssl-client-ca", ssl_client_certs.release());

    scoped_ptr<base::ListValue> client_cert_types(new base::ListValue());
    for (size_t i = 0; i < ssl_options_.client_cert_types.size(); i++) {
      client_cert_types->Append(new base::StringValue(
          GetClientCertType(ssl_options_.client_cert_types[i])));
    }
    if (client_cert_types->GetSize())
      arguments->Set("ssl-client-cert-type", client_cert_types.release());
  }

  if (type_ == TYPE_HTTPS) {
    arguments->Set("https", base::Value::CreateNullValue());

    std::string ocsp_arg = ssl_options_.GetOCSPArgument();
    if (!ocsp_arg.empty())
      arguments->SetString("ocsp", ocsp_arg);

    if (ssl_options_.cert_serial != 0) {
      arguments->Set("cert-serial",
                     base::Value::CreateIntegerValue(ssl_options_.cert_serial));
    }

    // Check key exchange argument.
    scoped_ptr<base::ListValue> key_exchange_values(new base::ListValue());
    GetKeyExchangesList(ssl_options_.key_exchanges, key_exchange_values.get());
    if (key_exchange_values->GetSize())
      arguments->Set("ssl-key-exchange", key_exchange_values.release());
    // Check bulk cipher argument.
    scoped_ptr<base::ListValue> bulk_cipher_values(new base::ListValue());
    GetCiphersList(ssl_options_.bulk_ciphers, bulk_cipher_values.get());
    if (bulk_cipher_values->GetSize())
      arguments->Set("ssl-bulk-cipher", bulk_cipher_values.release());
    if (ssl_options_.record_resume)
      arguments->Set("https-record-resume", base::Value::CreateNullValue());
    if (ssl_options_.tls_intolerant != SSLOptions::TLS_INTOLERANT_NONE) {
      arguments->Set("tls-intolerant",
                     new base::FundamentalValue(ssl_options_.tls_intolerant));
    }
    if (ssl_options_.fallback_scsv_enabled)
      arguments->Set("fallback-scsv", base::Value::CreateNullValue());
    if (!ssl_options_.signed_cert_timestamps_tls_ext.empty()) {
      std::string b64_scts_tls_ext;
      base::Base64Encode(ssl_options_.signed_cert_timestamps_tls_ext,
                         &b64_scts_tls_ext);
      arguments->SetString("signed-cert-timestamps-tls-ext", b64_scts_tls_ext);
    }
    if (ssl_options_.staple_ocsp_response)
      arguments->Set("staple-ocsp-response", base::Value::CreateNullValue());
    if (ssl_options_.enable_npn)
      arguments->Set("enable-npn", base::Value::CreateNullValue());
  }

  return GenerateAdditionalArguments(arguments);
}

bool BaseTestServer::GenerateAdditionalArguments(
    base::DictionaryValue* arguments) const {
  return true;
}

}  // namespace net
