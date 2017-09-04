// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/assignment_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "blimp/common/get_client_auth_token.h"
#include "blimp/common/protocol_version.h"
#include "components/safe_json/safe_json_parser.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

namespace blimp {
namespace client {

namespace {

// Assignment request JSON keys.
const char kProtocolVersionKey[] = "protocol_version";

// Assignment response JSON keys.
const char kClientAuthTokenKey[] = "clientToken";
const char kHostKey[] = "host";
const char kPortKey[] = "port";
const char kCertificateKey[] = "certificate";

// Possible arguments for the "--engine-transport" command line parameter.
const char kGrpcTransportValue[] = "grpc";
const char kSSLTransportValue[] = "ssl";
const char kTCPTransportValue[] = "tcp";

class SimpleURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  SimpleURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
      : io_task_runner_(io_task_runner), file_task_runner_(file_task_runner) {}

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override {
    DCHECK(io_task_runner_->BelongsToCurrentThread());

    if (!proxy_config_service_) {
      proxy_config_service_ = net::ProxyService::CreateSystemProxyConfigService(
          io_task_runner_, file_task_runner_);
    }

    if (!url_request_context_) {
      net::URLRequestContextBuilder builder;
      builder.set_proxy_config_service(std::move(proxy_config_service_));
      builder.DisableHttpCache();
      url_request_context_ = builder.Build();
    }

    return url_request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return io_task_runner_;
  }

 private:
  ~SimpleURLRequestContextGetter() override {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  // Constructed lazily on the IO thread by GetURLRequestContext().
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SimpleURLRequestContextGetter);
};

bool IsValidIpPortNumber(unsigned port) {
  return port > 0 && port <= 65535;
}

// Populates an Assignment using command-line parameters, if provided.
// Returns a null Assignment if no parameters were set.
// Must be called on a thread suitable for file IO.
Assignment GetAssignmentFromCommandLine() {
  Assignment assignment;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  assignment.client_auth_token = GetClientAuthToken(*cmd_line);
  CHECK(!assignment.client_auth_token.empty())
      << "No client auth token provided.";

  unsigned port_parsed = 0;
  if (!base::StringToUint(
          cmd_line->GetSwitchValueASCII(switches::kEnginePort),
          &port_parsed) || !IsValidIpPortNumber(port_parsed)) {
    DLOG(FATAL) << "--engine-port must be a value between 1 and 65535.";
    return Assignment();
  }

  net::IPAddress ip_address;
  std::string ip_str = cmd_line->GetSwitchValueASCII(switches::kEngineIP);
  if (!ip_address.AssignFromIPLiteral(ip_str)) {
    DLOG(FATAL) << "Invalid engine IP " << ip_str;
    return Assignment();
  }
  assignment.engine_endpoint =
      net::IPEndPoint(ip_address, base::checked_cast<uint16_t>(port_parsed));

  std::string transport_str =
      cmd_line->GetSwitchValueASCII(switches::kEngineTransport);
  if (transport_str == kSSLTransportValue) {
    assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  } else if (transport_str == kTCPTransportValue) {
    assignment.transport_protocol = Assignment::TransportProtocol::TCP;
  } else if (transport_str == kGrpcTransportValue) {
    assignment.transport_protocol = Assignment::TransportProtocol::GRPC;
  } else {
    DLOG(FATAL) << "Invalid engine transport " << transport_str;
    return Assignment();
  }

  scoped_refptr<net::X509Certificate> cert;
  if (assignment.transport_protocol == Assignment::TransportProtocol::SSL) {
    base::FilePath cert_path =
        cmd_line->GetSwitchValuePath(switches::kEngineCertPath);
    if (cert_path.empty()) {
      DLOG(FATAL) << "Missing required parameter --"
                  << switches::kEngineCertPath << ".";
      return Assignment();
    }
    std::string cert_str;
    if (!base::ReadFileToString(cert_path, &cert_str)) {
      DLOG(FATAL) << "Couldn't read from file: "
                  << cert_path.LossyDisplayName();
      return Assignment();
    }
    net::CertificateList cert_list =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_str.data(), cert_str.size(),
            net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    DLOG_IF(FATAL, (cert_list.size() != 1u))
        << "Only one cert is allowed in PEM cert list.";
    assignment.cert = std::move(cert_list[0]);
  }

  if (!assignment.IsValid()) {
    DLOG(FATAL) << "Invalid command-line assignment.";
    return Assignment();
  }

  return assignment;
}

}  // namespace

AssignmentSource::AssignmentSource(
    const GURL& assigner_endpoint,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
    : assigner_endpoint_(assigner_endpoint),
      file_task_runner_(file_task_runner),
      url_request_context_(
          new SimpleURLRequestContextGetter(network_task_runner,
                                            file_task_runner)),
      weak_factory_(this) {
  DCHECK(assigner_endpoint_.is_valid());
}

AssignmentSource::~AssignmentSource() {}

void AssignmentSource::GetAssignment(const std::string& client_auth_token,
                                     const AssignmentCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = AssignmentCallback(callback);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEngineIP)) {
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(), FROM_HERE,
        base::Bind(&GetAssignmentFromCommandLine),
        base::Bind(&AssignmentSource::OnGetAssignmentFromCommandLineDone,
                   weak_factory_.GetWeakPtr(), client_auth_token));
  } else {
    QueryAssigner(client_auth_token);
  }
}

void AssignmentSource::OnGetAssignmentFromCommandLineDone(
    const std::string& client_auth_token,
    Assignment parsed_assignment) {
  // If GetAssignmentFromCommandLine succeeded, then return its output.
  if (parsed_assignment.IsValid()) {
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_OK, parsed_assignment);
    return;
  }

  // If no assignment was passed via the command line, then fall back on
  // querying the Assigner service.
  QueryAssigner(client_auth_token);
}

void AssignmentSource::QueryAssigner(const std::string& client_auth_token) {
  // Call out to the network for a real assignment.  Build the network request
  // to hit the assigner.
  url_fetcher_ =
      net::URLFetcher::Create(assigner_endpoint_, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(url_request_context_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->AddExtraRequestHeader("Authorization: Bearer " +
                                      client_auth_token);

  // Write the JSON for the request data.
  base::DictionaryValue dictionary;
  dictionary.SetString(kProtocolVersionKey,
                       base::IntToString(kProtocolVersion));
  std::string json;
  base::JSONWriter::Write(dictionary, &json);
  url_fetcher_->SetUploadData("application/json", json);
  url_fetcher_->Start();
}

void AssignmentSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!callback_.is_null());
  DCHECK_EQ(url_fetcher_.get(), source);

  if (!source->GetStatus().is_success()) {
    DVLOG(1) << "Assignment request failed due to network error: "
             << net::ErrorToString(source->GetStatus().error());
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_NETWORK_FAILURE, Assignment());
    return;
  }

  switch (source->GetResponseCode()) {
    case net::HTTP_OK:
      ParseAssignerResponse();
      break;
    case net::HTTP_BAD_REQUEST:
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_BAD_REQUEST, Assignment());
      break;
    case net::HTTP_UNAUTHORIZED:
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_EXPIRED_ACCESS_TOKEN, Assignment());
      break;
    case net::HTTP_FORBIDDEN:
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_USER_INVALID, Assignment());
      break;
    case 429:  // Too Many Requests
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_OUT_OF_VMS, Assignment());
      break;
    case net::HTTP_INTERNAL_SERVER_ERROR:
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_SERVER_ERROR, Assignment());
      break;
    default:
      LOG(WARNING) << "Defaulting to BAD_RESPONSE for HTTP response code "
                   << source->GetResponseCode();
      base::ResetAndReturn(&callback_)
          .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
      break;
  }
}

void AssignmentSource::ParseAssignerResponse() {
  DCHECK(url_fetcher_.get());
  DCHECK(url_fetcher_->GetStatus().is_success());
  DCHECK_EQ(net::HTTP_OK, url_fetcher_->GetResponseCode());

  // Grab the response from the assigner request.
  std::string response;
  if (!url_fetcher_->GetResponseAsString(&response)) {
    LOG(WARNING) << "Unable to retrieve response as string from URLFetcher.";
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  safe_json::SafeJsonParser::Parse(
      response,
      base::Bind(&AssignmentSource::OnJsonParsed, weak_factory_.GetWeakPtr()),
      base::Bind(&AssignmentSource::OnJsonParseError,
                 weak_factory_.GetWeakPtr()));
}

void AssignmentSource::OnJsonParsed(std::unique_ptr<base::Value> json) {
  const base::DictionaryValue* dict;
  if (!json->GetAsDictionary(&dict)) {
    LOG(WARNING) << "Unable to parse JSON data as a dictionary.";
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  // Validate that all the expected fields are present.
  std::string client_auth_token;
  std::string host;
  int port;
  std::string cert_str;
  if (!(dict->GetString(kClientAuthTokenKey, &client_auth_token) &&
        dict->GetString(kHostKey, &host) && dict->GetInteger(kPortKey, &port) &&
        dict->GetString(kCertificateKey, &cert_str))) {
    LOG(WARNING) << "Not all fields present in assignment JSON data.";
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(host)) {
    LOG(WARNING) << "Unable to assign IP address from string literal " << host;
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(port)) {
    LOG(WARNING) << "Assignment port number not in range for uint16_t";
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  net::CertificateList cert_list =
      net::X509Certificate::CreateCertificateListFromBytes(
          cert_str.data(), cert_str.size(),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  if (cert_list.size() != 1) {
    base::ResetAndReturn(&callback_)
        .Run(ASSIGNMENT_REQUEST_RESULT_INVALID_CERT, Assignment());
    return;
  }

  // The assigner assumes SSL-only and all engines it assigns only communicate
  // over SSL.
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.engine_endpoint = net::IPEndPoint(ip_address, port);
  assignment.client_auth_token = client_auth_token;
  assignment.cert = std::move(cert_list[0]);

  base::ResetAndReturn(&callback_)
      .Run(ASSIGNMENT_REQUEST_RESULT_OK, assignment);
}

void AssignmentSource::OnJsonParseError(const std::string& error) {
  DLOG(ERROR) << "Error while parsing assigner JSON: " << error;
  base::ResetAndReturn(&callback_)
      .Run(ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE, Assignment());
}

}  // namespace client
}  // namespace blimp
