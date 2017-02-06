// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/assignment_source.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "blimp/client/app/blimp_client_switches.h"
#include "blimp/common/get_client_token.h"
#include "blimp/common/protocol_version.h"
#include "blimp/common/switches.h"
#include "components/safe_json/testing_json_parser.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

namespace blimp {
namespace client {
namespace {

const uint8_t kTestIpAddress[] = {127, 0, 0, 1};
const uint16_t kTestPort = 8086;
const char kTestIpAddressString[] = "127.0.0.1";
const char kTcpTransportName[] = "tcp";
const char kSslTransportName[] = "ssl";
const char kCertRelativePath[] =
    "blimp/client/session/test_selfsigned_cert.pem";
const char kTestClientToken[] = "secrett0ken";
const char kTestAuthToken[] = "UserAuthT0kenz";
const char kAssignerUrl[] = "http://www.assigner.test/";
const char kTestClientTokenPath[] = "blimp/test/data/test_client_token";

MATCHER_P(AssignmentEquals, assignment, "") {
  return arg.transport_protocol == assignment.transport_protocol &&
         arg.engine_endpoint == assignment.engine_endpoint &&
         arg.client_token == assignment.client_token &&
         ((!assignment.cert && !arg.cert) ||
          (arg.cert && assignment.cert &&
           arg.cert->Equals(assignment.cert.get())));
}

// Converts |value| to a JSON string.
std::string ValueToString(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

class AssignmentSourceTest : public testing::Test {
 public:
  AssignmentSourceTest()
      : source_(GURL(kAssignerUrl),
                message_loop_.task_runner(),
                message_loop_.task_runner()) {}

  void SetUp() override {
    base::FilePath src_root;
    PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
    ASSERT_FALSE(src_root.empty());
    cert_path_ = src_root.Append(kCertRelativePath);
    client_token_path_ = src_root.Append(kTestClientTokenPath);
    ASSERT_TRUE(base::ReadFileToString(cert_path_, &cert_pem_));
    net::CertificateList cert_list =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_pem_.data(), cert_pem_.size(),
            net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    ASSERT_FALSE(cert_list.empty());
    cert_ = std::move(cert_list[0]);
    ASSERT_TRUE(cert_);
  }

  // This expects the AssignmentSource::GetAssignment to return a custom
  // endpoint without having to hit the network.  This will typically be used
  // for testing that specifying an assignment via the command line works as
  // expected.
  void GetAlternateAssignment() {
    source_.GetAssignment("",
                          base::Bind(&AssignmentSourceTest::AssignmentResponse,
                                     base::Unretained(this)));
    EXPECT_EQ(nullptr, factory_.GetFetcherByID(0));
    base::RunLoop().RunUntilIdle();
  }

  // See net/base/net_errors.h for possible status errors.
  void GetNetworkAssignmentAndWaitForResponse(
      net::HttpStatusCode response_code,
      int status,
      const std::string& response,
      const std::string& client_auth_token,
      const int protocol_version) {
    source_.GetAssignment(client_auth_token,
                          base::Bind(&AssignmentSourceTest::AssignmentResponse,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);

    EXPECT_NE(nullptr, fetcher);
    EXPECT_EQ(kAssignerUrl, fetcher->GetOriginalURL().spec());

    // Check that the request has a valid protocol_version.
    std::unique_ptr<base::Value> json =
        base::JSONReader::Read(fetcher->upload_data());
    EXPECT_NE(nullptr, json.get());

    const base::DictionaryValue* dict;
    EXPECT_TRUE(json->GetAsDictionary(&dict));

    std::string uploaded_protocol_version;
    EXPECT_TRUE(
        dict->GetString("protocol_version", &uploaded_protocol_version));
    std::string expected_protocol_version = base::IntToString(protocol_version);
    EXPECT_EQ(expected_protocol_version, uploaded_protocol_version);

    // Check that the request has a valid authentication header.
    net::HttpRequestHeaders headers;
    fetcher->GetExtraRequestHeaders(&headers);

    std::string authorization;
    EXPECT_TRUE(headers.GetHeader("Authorization", &authorization));
    EXPECT_EQ("Bearer " + client_auth_token, authorization);

    // Send the fake response back.
    fetcher->set_response_code(response_code);
    fetcher->set_status(net::URLRequestStatus::FromError(status));
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD2(AssignmentResponse,
               void(AssignmentSource::Result, const Assignment&));

 protected:
  Assignment BuildSslAssignment();

  // Builds simulated JSON response from the Assigner service.
  std::unique_ptr<base::DictionaryValue> BuildAssignerResponse();

  // Used to drive all AssignmentSource tasks.
  // MessageLoop is required by TestingJsonParser's self-deletion logic.
  // TODO(bauerb): Replace this with a TestSimpleTaskRunner once
  // TestingJsonParser no longer requires having a MessageLoop.
  base::MessageLoop message_loop_;

  net::TestURLFetcherFactory factory_;

  // Path to the PEM-encoded certificate chain.
  base::FilePath cert_path_;

  // Path to the client token;
  base::FilePath client_token_path_;

  // Payload of PEM certificate chain at |cert_path_|.
  std::string cert_pem_;

  // X509 certificate decoded from |cert_path_|.
  scoped_refptr<net::X509Certificate> cert_;

  AssignmentSource source_;

  // Allows safe_json to parse JSON in-process, instead of depending on a
  // utility proces.
  safe_json::TestingJsonParser::ScopedFactoryOverride json_parsing_factory_;
};

Assignment AssignmentSourceTest::BuildSslAssignment() {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.engine_endpoint = net::IPEndPoint(kTestIpAddress, kTestPort);
  assignment.client_token = kTestClientToken;
  assignment.cert = cert_;
  return assignment;
}

std::unique_ptr<base::DictionaryValue>
AssignmentSourceTest::BuildAssignerResponse() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("clientToken", kTestClientToken);
  dict->SetString("host", kTestIpAddressString);
  dict->SetInteger("port", kTestPort);
  dict->SetString("certificate", cert_pem_);
  return dict;
}

TEST_F(AssignmentSourceTest, TestTCPAlternateEndpointSuccess) {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::TCP;
  assignment.engine_endpoint = net::IPEndPoint(kTestIpAddress, kTestPort);
  assignment.cert = scoped_refptr<net::X509Certificate>(nullptr);

  auto cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII(switches::kEngineIP, kTestIpAddressString);
  cmd_line->AppendSwitchASCII(switches::kEnginePort,
                              std::to_string(kTestPort));
  cmd_line->AppendSwitchASCII(switches::kEngineTransport, kTcpTransportName);
  cmd_line->AppendSwitchASCII(kClientTokenPath, client_token_path_.value());

  assignment.client_token = GetClientToken(*cmd_line);

  CHECK_EQ("MyVoiceIsMyPassport", assignment.client_token);

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetAlternateAssignment();
}

TEST_F(AssignmentSourceTest, TestSSLAlternateEndpointSuccess) {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.engine_endpoint = net::IPEndPoint(kTestIpAddress, kTestPort);
  assignment.cert = cert_;

  auto cmd_line = base::CommandLine::ForCurrentProcess();

  cmd_line->AppendSwitchASCII(switches::kEngineIP, kTestIpAddressString);
  cmd_line->AppendSwitchASCII(switches::kEnginePort,
                              std::to_string(kTestPort));
  cmd_line->AppendSwitchASCII(switches::kEngineTransport, kSslTransportName);
  cmd_line->AppendSwitchASCII(switches::kEngineCertPath, cert_path_.value());
  cmd_line->AppendSwitchASCII(kClientTokenPath, client_token_path_.value());

  assignment.client_token = GetClientToken(*cmd_line);

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetAlternateAssignment();
}

TEST_F(AssignmentSourceTest, TestSuccess) {
  Assignment assignment = BuildSslAssignment();

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetNetworkAssignmentAndWaitForResponse(
      net::HTTP_OK, net::Error::OK, ValueToString(*BuildAssignerResponse()),
      kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestValidAfterError) {
  InSequence sequence;
  Assignment assignment = BuildSslAssignment();

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_NETWORK_FAILURE, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1)
      .RetiresOnSaturation();

  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK,
                                         net::Error::ERR_INSUFFICIENT_RESOURCES,
                                         "", kTestAuthToken, kProtocolVersion);

  GetNetworkAssignmentAndWaitForResponse(
      net::HTTP_OK, net::Error::OK, ValueToString(*BuildAssignerResponse()),
      kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestNetworkFailure) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_NETWORK_FAILURE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK,
                                         net::Error::ERR_INSUFFICIENT_RESOURCES,
                                         "", kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestBadRequest) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_REQUEST, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_BAD_REQUEST, net::Error::OK,
                                         "", kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestUnauthorized) {
  EXPECT_CALL(*this,
              AssignmentResponse(
                  AssignmentSource::Result::RESULT_EXPIRED_ACCESS_TOKEN, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_UNAUTHORIZED, net::Error::OK,
                                         "", kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestForbidden) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_USER_INVALID, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_FORBIDDEN, net::Error::OK,
                                         "", kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestTooManyRequests) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_OUT_OF_VMS, _));
  GetNetworkAssignmentAndWaitForResponse(static_cast<net::HttpStatusCode>(429),
                                         net::Error::OK, "", kTestAuthToken,
                                         kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestInternalServerError) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_SERVER_ERROR, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                         net::Error::OK, "", kTestAuthToken,
                                         kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestUnexpectedNetCodeFallback) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_NOT_IMPLEMENTED,
                                         net::Error::OK, "", kTestAuthToken,
                                         kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestInvalidJsonResponse) {
  Assignment assignment = BuildSslAssignment();

  // Remove half the response.
  std::string response = ValueToString(*BuildAssignerResponse());
  response = response.substr(response.size() / 2);

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK, response,
                                         kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestMissingResponsePort) {
  std::unique_ptr<base::DictionaryValue> response = BuildAssignerResponse();
  response->Remove("port", nullptr);
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK,
                                         ValueToString(*response),
                                         kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestInvalidIPAddress) {
  std::unique_ptr<base::DictionaryValue> response = BuildAssignerResponse();
  response->SetString("host", "happywhales.test");

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK,
                                         ValueToString(*response),
                                         kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestMissingCert) {
  std::unique_ptr<base::DictionaryValue> response = BuildAssignerResponse();
  response->Remove("certificate", nullptr);
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK,
                                         ValueToString(*response),
                                         kTestAuthToken, kProtocolVersion);
}

TEST_F(AssignmentSourceTest, TestInvalidCert) {
  std::unique_ptr<base::DictionaryValue> response = BuildAssignerResponse();
  response->SetString("certificate", "h4x0rz!");
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_INVALID_CERT, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK,
                                         ValueToString(*response),
                                         kTestAuthToken, kProtocolVersion);
}

}  // namespace
}  // namespace client
}  // namespace blimp
