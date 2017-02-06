// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/printing/cloud_print/privet_http_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "printing/pwg_raster_settings.h"
#endif  // ENABLE_PRINT_PREVIEW

namespace cloud_print {

namespace {

using testing::NiceMock;
using testing::StrictMock;
using testing::TestWithParam;
using testing::ValuesIn;

using content::BrowserThread;
using net::EmbeddedTestServer;

const char kSampleInfoResponse[] = "{"
    "       \"version\": \"1.0\","
    "       \"name\": \"Common printer\","
    "       \"description\": \"Printer connected through Chrome connector\","
    "       \"url\": \"https://www.google.com/cloudprint\","
    "       \"type\": ["
    "               \"printer\""
    "       ],"
    "       \"id\": \"\","
    "       \"device_state\": \"idle\","
    "       \"connection_state\": \"online\","
    "       \"manufacturer\": \"Google\","
    "       \"model\": \"Google Chrome\","
    "       \"serial_number\": \"1111-22222-33333-4444\","
    "       \"firmware\": \"24.0.1312.52\","
    "       \"uptime\": 600,"
    "       \"setup_url\": \"http://support.google.com/\","
    "       \"support_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"update_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"x-privet-token\": \"SampleTokenForTesting\","
    "       \"api\": ["
    "               \"/privet/accesstoken\","
    "               \"/privet/capabilities\","
    "               \"/privet/printer/submitdoc\","
    "       ]"
    "}";

const char kSampleInfoResponseRegistered[] = "{"
    "       \"version\": \"1.0\","
    "       \"name\": \"Common printer\","
    "       \"description\": \"Printer connected through Chrome connector\","
    "       \"url\": \"https://www.google.com/cloudprint\","
    "       \"type\": ["
    "               \"printer\""
    "       ],"
    "       \"id\": \"MyDeviceID\","
    "       \"device_state\": \"idle\","
    "       \"connection_state\": \"online\","
    "       \"manufacturer\": \"Google\","
    "       \"model\": \"Google Chrome\","
    "       \"serial_number\": \"1111-22222-33333-4444\","
    "       \"firmware\": \"24.0.1312.52\","
    "       \"uptime\": 600,"
    "       \"setup_url\": \"http://support.google.com/\","
    "       \"support_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"update_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"x-privet-token\": \"SampleTokenForTesting\","
    "       \"api\": ["
    "               \"/privet/accesstoken\","
    "               \"/privet/capabilities\","
    "               \"/privet/printer/submitdoc\","
    "       ]"
    "}";

const char kSampleRegisterStartResponse[] = "{"
    "\"user\": \"example@google.com\","
    "\"action\": \"start\""
    "}";

const char kSampleRegisterGetClaimTokenResponse[] = "{"
    "       \"action\": \"getClaimToken\","
    "       \"user\": \"example@google.com\","
    "       \"token\": \"MySampleToken\","
    "       \"claim_url\": \"https://domain.com/SoMeUrL\""
    "}";

const char kSampleRegisterCompleteResponse[] = "{"
    "\"user\": \"example@google.com\","
    "\"action\": \"complete\","
    "\"device_id\": \"MyDeviceID\""
    "}";

const char kSampleXPrivetErrorResponse[] =
    "{ \"error\": \"invalid_x_privet_token\" }";

const char kSampleRegisterErrorTransient[] =
    "{ \"error\": \"device_busy\", \"timeout\": 1}";

const char kSampleRegisterErrorPermanent[] =
    "{ \"error\": \"user_cancel\" }";

const char kSampleInfoResponseBadJson[] = "{";

const char kSampleRegisterCancelResponse[] = "{"
    "\"user\": \"example@google.com\","
    "\"action\": \"cancel\""
    "}";

const char kSampleCapabilitiesResponse[] = "{"
    "\"version\" : \"1.0\","
    "\"printer\" : {"
    "  \"supported_content_type\" : ["
    "   { \"content_type\" : \"application/pdf\" },"
    "   { \"content_type\" : \"image/pwg-raster\" }"
    "  ]"
    "}"
    "}";

#if defined(ENABLE_PRINT_PREVIEW)
const char kSampleInfoResponseWithCreatejob[] = "{"
    "       \"version\": \"1.0\","
    "       \"name\": \"Common printer\","
    "       \"description\": \"Printer connected through Chrome connector\","
    "       \"url\": \"https://www.google.com/cloudprint\","
    "       \"type\": ["
    "               \"printer\""
    "       ],"
    "       \"id\": \"\","
    "       \"device_state\": \"idle\","
    "       \"connection_state\": \"online\","
    "       \"manufacturer\": \"Google\","
    "       \"model\": \"Google Chrome\","
    "       \"serial_number\": \"1111-22222-33333-4444\","
    "       \"firmware\": \"24.0.1312.52\","
    "       \"uptime\": 600,"
    "       \"setup_url\": \"http://support.google.com/\","
    "       \"support_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"update_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"x-privet-token\": \"SampleTokenForTesting\","
    "       \"api\": ["
    "               \"/privet/accesstoken\","
    "               \"/privet/capabilities\","
    "               \"/privet/printer/createjob\","
    "               \"/privet/printer/submitdoc\","
    "       ]"
    "}";

const char kSampleLocalPrintResponse[] = "{"
    "\"job_id\": \"123\","
    "\"expires_in\": 500,"
    "\"job_type\": \"application/pdf\","
    "\"job_size\": 16,"
    "\"job_name\": \"Sample job name\","
    "}";

const char kSampleCapabilitiesResponsePWGOnly[] = "{"
    "\"version\" : \"1.0\","
    "\"printer\" : {"
    "  \"supported_content_type\" : ["
    "   { \"content_type\" : \"image/pwg-raster\" }"
    "  ]"
    "}"
    "}";

const char kSampleErrorResponsePrinterBusy[] = "{"
    "\"error\": \"invalid_print_job\","
    "\"timeout\": 1 "
    "}";

const char kSampleInvalidDocumentTypeResponse[] = "{"
    "\"error\" : \"invalid_document_type\""
    "}";

const char kSampleCreatejobResponse[] = "{ \"job_id\": \"1234\" }";

const char kSampleCapabilitiesResponseWithAnyMimetype[] = "{"
    "\"version\" : \"1.0\","
    "\"printer\" : {"
    "  \"supported_content_type\" : ["
    "   { \"content_type\" : \"*/*\" },"
    "   { \"content_type\" : \"image/pwg-raster\" }"
    "  ]"
    "}"
    "}";

const char kSampleCJT[] = "{ \"version\" : \"1.0\" }";

const char kSampleCapabilitiesResponsePWGSettings[] =
    "{"
    "\"version\" : \"1.0\","
    "\"printer\" : {"
    " \"pwg_raster_config\" : {"
    "   \"document_sheet_back\" : \"MANUAL_TUMBLE\","
    "   \"reverse_order_streaming\": true"
    "  },"
    "  \"supported_content_type\" : ["
    "   { \"content_type\" : \"image/pwg-raster\" }"
    "  ]"
    "}"
    "}";

const char kSampleCJTDuplex[] =
    "{"
    "\"version\" : \"1.0\","
    "\"print\": { \"duplex\": {\"type\": \"SHORT_EDGE\"} }"
    "}";
#endif  // ENABLE_PRINT_PREVIEW

const char* const kTestParams[] = {"8.8.4.4", "2001:4860:4860::8888"};

// Return the representation of the given JSON that would be outputted by
// JSONWriter. This ensures the same JSON values are represented by the same
// string.
std::string NormalizeJson(const std::string& json) {
  std::string result = json;
  std::unique_ptr<base::Value> value = base::JSONReader::Read(result);
  DCHECK(value) << result;
  base::JSONWriter::Write(*value, &result);
  return result;
}

class MockTestURLFetcherFactoryDelegate
    : public net::TestURLFetcher::DelegateForTests {
 public:
  // Callback issued correspondingly to the call to the |Start()| method.
  MOCK_METHOD1(OnRequestStart, void(int fetcher_id));

  // Callback issued correspondingly to the call to |AppendChunkToUpload|.
  // Uploaded chunks can be retrieved with the |upload_chunks()| getter.
  MOCK_METHOD1(OnChunkUpload, void(int fetcher_id));

  // Callback issued correspondingly to the destructor.
  MOCK_METHOD1(OnRequestEnd, void(int fetcher_id));
};

class PrivetHTTPTest : public TestWithParam<const char*> {
 public:
  PrivetHTTPTest() {
    PrivetURLFetcher::ResetTokenMapForTests();

    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    privet_client_ = PrivetV1HTTPClient::CreateDefault(
        base::WrapUnique<PrivetHTTPClient>(new PrivetHTTPClientImpl(
            "sampleDevice._privet._tcp.local",
            net::HostPortPair(GetParam(), 6006), request_context_.get())));
    fetcher_factory_.SetDelegateForTests(&fetcher_delegate_);
  }

  GURL GetUrl(const std::string& path) const {
    std::string host = GetParam();
    if (host.find(":") != std::string::npos)
      host = "[" + host + "]";
    return GURL("http://" + host + ":6006" + path);
  }

  bool SuccessfulResponseToURL(const GURL& url,
                               const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_EQ(url, fetcher->GetOriginalURL());

    if (!fetcher || url != fetcher->GetOriginalURL())
      return false;

    fetcher->SetResponseString(response);
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    return true;
  }

  bool SuccessfulResponseToURLAndData(const GURL& url,
                                      const std::string& data,
                                      const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_EQ(url, fetcher->GetOriginalURL());

    if (!fetcher) return false;

    EXPECT_EQ(data, fetcher->upload_data());
    if (data != fetcher->upload_data()) return false;

    return SuccessfulResponseToURL(url, response);
  }

  bool SuccessfulResponseToURLAndJSONData(const GURL& url,
                                          const std::string& data,
                                          const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_EQ(url, fetcher->GetOriginalURL());

    if (!fetcher)
      return false;

    std::string normalized_data = NormalizeJson(data);
    std::string normalized_upload_data = NormalizeJson(fetcher->upload_data());
    EXPECT_EQ(normalized_data, normalized_upload_data);
    if (normalized_data != normalized_upload_data)
      return false;

    return SuccessfulResponseToURL(url, response);
  }

  bool SuccessfulResponseToURLAndFilePath(const GURL& url,
                                          const base::FilePath& file_path,
                                          const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_EQ(url, fetcher->GetOriginalURL());

    if (!fetcher) return false;

    EXPECT_EQ(file_path, fetcher->upload_file_path());
    if (file_path != fetcher->upload_file_path()) return false;

    return SuccessfulResponseToURL(url, response);
  }


  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &PrivetHTTPTest::Stop, base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::RunLoop().Run();
    callback.Cancel();
  }

  void Stop() { base::MessageLoop::current()->QuitWhenIdle(); }

 protected:
  base::MessageLoop loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  std::unique_ptr<PrivetV1HTTPClient> privet_client_;
  NiceMock<MockTestURLFetcherFactoryDelegate> fetcher_delegate_;
};

class MockJSONCallback{
 public:
  void OnPrivetJSONDone(const base::DictionaryValue* value) {
    if (!value) {
      value_.reset();
    } else {
      value_.reset(value->DeepCopy());
    }

    OnPrivetJSONDoneInternal();
  }

  MOCK_METHOD0(OnPrivetJSONDoneInternal, void());

  const base::DictionaryValue* value() { return value_.get(); }
  PrivetJSONOperation::ResultCallback callback() {
    return base::Bind(&MockJSONCallback::OnPrivetJSONDone,
                      base::Unretained(this));
  }
 protected:
  std::unique_ptr<base::DictionaryValue> value_;
};

class MockRegisterDelegate : public PrivetRegisterOperation::Delegate {
 public:
  void OnPrivetRegisterClaimToken(
      PrivetRegisterOperation* operation,
      const std::string& token,
      const GURL& url) override {
    OnPrivetRegisterClaimTokenInternal(token, url);
  }

  MOCK_METHOD2(OnPrivetRegisterClaimTokenInternal, void(
      const std::string& token,
      const GURL& url));

  void OnPrivetRegisterError(
      PrivetRegisterOperation* operation,
      const std::string& action,
      PrivetRegisterOperation::FailureReason reason,
      int printer_http_code,
      const base::DictionaryValue* json) override {
    // TODO(noamsml): Save and test for JSON?
    OnPrivetRegisterErrorInternal(action, reason, printer_http_code);
  }

  MOCK_METHOD3(OnPrivetRegisterErrorInternal,
               void(const std::string& action,
                    PrivetRegisterOperation::FailureReason reason,
                    int printer_http_code));

  void OnPrivetRegisterDone(
      PrivetRegisterOperation* operation,
      const std::string& device_id) override {
    OnPrivetRegisterDoneInternal(device_id);
  }

  MOCK_METHOD1(OnPrivetRegisterDoneInternal,
               void(const std::string& device_id));
};

class MockLocalPrintDelegate : public PrivetLocalPrintOperation::Delegate {
 public:
  virtual void OnPrivetPrintingDone(
      const PrivetLocalPrintOperation* print_operation) {
    OnPrivetPrintingDoneInternal();
  }

  MOCK_METHOD0(OnPrivetPrintingDoneInternal, void());

  virtual void OnPrivetPrintingError(
      const PrivetLocalPrintOperation* print_operation, int http_code) {
    OnPrivetPrintingErrorInternal(http_code);
  }

  MOCK_METHOD1(OnPrivetPrintingErrorInternal, void(int http_code));
};

class PrivetInfoTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    info_operation_ = privet_client_->CreateInfoOperation(
        info_callback_.callback());
  }

 protected:
  std::unique_ptr<PrivetJSONOperation> info_operation_;
  StrictMock<MockJSONCallback> info_callback_;
};

INSTANTIATE_TEST_CASE_P(PrivetTests, PrivetInfoTest, ValuesIn(kTestParams));

TEST_P(PrivetInfoTest, SuccessfulInfo) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  EXPECT_EQ(GetUrl("/privet/info"), fetcher->GetOriginalURL());

  fetcher->SetResponseString(kSampleInfoResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(info_callback_, OnPrivetJSONDoneInternal());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_P(PrivetInfoTest, InfoFailureHTTP) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(404);

  EXPECT_CALL(info_callback_, OnPrivetJSONDoneInternal());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

class PrivetRegisterTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    info_operation_ = privet_client_->CreateInfoOperation(
        info_callback_.callback());
    register_operation_ =
        privet_client_->CreateRegisterOperation("example@google.com",
                                                &register_delegate_);
  }

 protected:
  bool SuccessfulResponseToURL(const GURL& url,
                               const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    EXPECT_EQ(url, fetcher->GetOriginalURL());
    if (!fetcher || url != fetcher->GetOriginalURL())
      return false;

    fetcher->SetResponseString(response);
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    return true;
  }

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  NiceMock<MockJSONCallback> info_callback_;
  std::unique_ptr<PrivetRegisterOperation> register_operation_;
  StrictMock<MockRegisterDelegate> register_delegate_;
};

TEST_P(PrivetRegisterTest, RegisterSuccessSimple) {
  register_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterStartResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterClaimTokenInternal(
      "MySampleToken",
      GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GetUrl("/privet/register?"
             "action=getClaimToken&user=example%40google.com"),
      kSampleRegisterGetClaimTokenResponse));

  register_operation_->CompleteRegistration();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GetUrl("/privet/register?"
             "action=complete&user=example%40google.com"),
      kSampleRegisterCompleteResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterDoneInternal(
      "MyDeviceID"));

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseRegistered));
}

TEST_P(PrivetRegisterTest, RegisterXSRFFailure) {
  register_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterStartResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GetUrl("/privet/register?"
             "action=getClaimToken&user=example%40google.com"),
      kSampleXPrivetErrorResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterClaimTokenInternal(
      "MySampleToken", GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GetUrl("/privet/register?"
             "action=getClaimToken&user=example%40google.com"),
      kSampleRegisterGetClaimTokenResponse));
}

TEST_P(PrivetRegisterTest, TransientFailure) {
  register_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterErrorTransient));

  EXPECT_CALL(fetcher_delegate_, OnRequestStart(0));

  RunFor(base::TimeDelta::FromSeconds(2));

  testing::Mock::VerifyAndClearExpectations(&fetcher_delegate_);

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterStartResponse));
}

TEST_P(PrivetRegisterTest, PermanentFailure) {
  register_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterStartResponse));

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterErrorInternal(
                  "getClaimToken",
                  PrivetRegisterOperation::FAILURE_JSON_ERROR,
                  200));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GetUrl("/privet/register?"
             "action=getClaimToken&user=example%40google.com"),
      kSampleRegisterErrorPermanent));
}

TEST_P(PrivetRegisterTest, InfoFailure) {
  register_operation_->Start();

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterErrorInternal(
                  "start",
                  PrivetRegisterOperation::FAILURE_TOKEN,
                  -1));

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseBadJson));
}

TEST_P(PrivetRegisterTest, RegisterCancel) {
  register_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=start&user=example%40google.com"),
                              kSampleRegisterStartResponse));

  register_operation_->Cancel();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/register?"
                                     "action=cancel&user=example%40google.com"),
                              kSampleRegisterCancelResponse));

  // Must keep mocks alive for 3 seconds so the cancelation object can be
  // deleted.
  RunFor(base::TimeDelta::FromSeconds(3));
}

class PrivetCapabilitiesTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    capabilities_operation_ = privet_client_->CreateCapabilitiesOperation(
        capabilities_callback_.callback());
  }

 protected:
  std::unique_ptr<PrivetJSONOperation> capabilities_operation_;
  StrictMock<MockJSONCallback> capabilities_callback_;
};

INSTANTIATE_TEST_CASE_P(PrivetTests,
                        PrivetCapabilitiesTest,
                        ValuesIn(kTestParams));

TEST_P(PrivetCapabilitiesTest, SuccessfulCapabilities) {
  capabilities_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/capabilities"),
                                      kSampleCapabilitiesResponse));

  std::string version;
  EXPECT_TRUE(capabilities_callback_.value()->GetString("version", &version));
  EXPECT_EQ("1.0", version);
}

TEST_P(PrivetCapabilitiesTest, CacheToken) {
  capabilities_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/capabilities"),
                                      kSampleCapabilitiesResponse));

  capabilities_operation_ = privet_client_->CreateCapabilitiesOperation(
      capabilities_callback_.callback());

  capabilities_operation_->Start();

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/capabilities"),
                                      kSampleCapabilitiesResponse));
}

TEST_P(PrivetCapabilitiesTest, BadToken) {
  capabilities_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/capabilities"),
                                      kSampleXPrivetErrorResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/capabilities"),
                                      kSampleCapabilitiesResponse));
}

#if defined(ENABLE_PRINT_PREVIEW)
// A note on PWG raster conversion: The PWG raster converter used simply
// converts strings to file paths based on them by appending "test.pdf", since
// it's easier to test that way. Instead of using a mock, we simply check if the
// request is uploading a file that is based on this pattern.
class FakePWGRasterConverter : public printing::PWGRasterConverter {
 public:
  void Start(base::RefCountedMemory* data,
             const printing::PdfRenderSettings& conversion_settings,
             const printing::PwgRasterSettings& bitmap_settings,
             const ResultCallback& callback) override {
    bitmap_settings_ = bitmap_settings;
    std::string data_str(data->front_as<char>(), data->size());
    callback.Run(true, base::FilePath().AppendASCII(data_str + "test.pdf"));
  }

  const printing::PwgRasterSettings& bitmap_settings() {
    return bitmap_settings_;
  }

 private:
  printing::PwgRasterSettings bitmap_settings_;
};

class PrivetLocalPrintTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    PrivetURLFetcher::ResetTokenMapForTests();

    local_print_operation_ = privet_client_->CreateLocalPrintOperation(
        &local_print_delegate_);

    std::unique_ptr<FakePWGRasterConverter> pwg_converter(
        new FakePWGRasterConverter);
    pwg_converter_ = pwg_converter.get();
    local_print_operation_->SetPWGRasterConverterForTesting(
        std::move(pwg_converter));
  }

  scoped_refptr<base::RefCountedBytes> RefCountedBytesFromString(
      std::string str) {
    std::vector<unsigned char> str_vec;
    str_vec.insert(str_vec.begin(), str.begin(), str.end());
    return base::RefCountedBytes::TakeVector(&str_vec);
  }

 protected:
  std::unique_ptr<PrivetLocalPrintOperation> local_print_operation_;
  StrictMock<MockLocalPrintDelegate> local_print_delegate_;
  FakePWGRasterConverter* pwg_converter_;
};

INSTANTIATE_TEST_CASE_P(PrivetTests,
                        PrivetLocalPrintTest,
                        ValuesIn(kTestParams));

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrint) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString(
      "Sample print data"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name"),
      "Sample print data", kSampleLocalPrintResponse));
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithAnyMimetype) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponseWithAnyMimetype);
  local_print_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name"),
      "Sample print data", kSampleLocalPrintResponse));
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrint) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(
      RefCountedBytesFromString("path/to/"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponsePWGOnly);
  local_print_operation_->Start();

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndFilePath(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com"
             "&job_name=Sample+job+name"),
      base::FilePath(FILE_PATH_LITERAL("path/to/test.pdf")),
      kSampleLocalPrintResponse));

  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().reverse_page_order);
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrintDuplex) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString("path/to/"));
  local_print_operation_->SetTicket(kSampleCJTDuplex);
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponsePWGSettings);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURLAndJSONData(
      GetUrl("/privet/printer/createjob"), kSampleCJTDuplex,
      kSampleCreatejobResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndFilePath(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com"
             "&job_name=Sample+job+name&job_id=1234"),
      base::FilePath(FILE_PATH_LITERAL("path/to/test.pdf")),
      kSampleLocalPrintResponse));

  EXPECT_EQ(printing::TRANSFORM_ROTATE_180,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_converter_->bitmap_settings().reverse_page_order);
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithCreatejob) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetTicket(kSampleCJT);
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURLAndJSONData(GetUrl("/privet/printer/createjob"),
                                         kSampleCJT, kSampleCreatejobResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name&job_id=1234"),
      "Sample print data", kSampleLocalPrintResponse));
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithOverlongName) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname(
      "123456789:123456789:123456789:123456789:123456789:123456789:123456789:");
  local_print_operation_->SetTicket(kSampleCJT);
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURLAndJSONData(GetUrl("/privet/printer/createjob"),
                                         kSampleCJT, kSampleCreatejobResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=123456789%3A123456789%3A123456789%3A1...123456789"
             "%3A123456789%3A123456789%3A&job_id=1234"),
      "Sample print data", kSampleLocalPrintResponse));
}

TEST_P(PrivetLocalPrintTest, PDFPrintInvalidDocumentTypeRetry) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetTicket(kSampleCJT);
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(
      RefCountedBytesFromString("sample/path/"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURLAndJSONData(GetUrl("/privet/printer/createjob"),
                                         kSampleCJT, kSampleCreatejobResponse));

  // TODO(noamsml): Is encoding spaces as pluses standard?
  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name&job_id=1234"),
      "sample/path/", kSampleInvalidDocumentTypeResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDoneInternal());

  EXPECT_TRUE(SuccessfulResponseToURLAndFilePath(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name&job_id=1234"),
      base::FilePath(FILE_PATH_LITERAL("sample/path/test.pdf")),
      kSampleLocalPrintResponse));
}

TEST_P(PrivetLocalPrintTest, LocalPrintRetryOnInvalidJobID) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetTicket(kSampleCJT);
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/info"),
                                      kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(
      SuccessfulResponseToURL(GetUrl("/privet/info"), kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponseToURLAndJSONData(GetUrl("/privet/printer/createjob"),
                                         kSampleCJT, kSampleCreatejobResponse));

  EXPECT_TRUE(SuccessfulResponseToURLAndData(
      GetUrl("/privet/printer/submitdoc?"
             "client_name=Chrome&user_name=sample%40gmail.com&"
             "job_name=Sample+job+name&job_id=1234"),
      "Sample print data", kSampleErrorResponsePrinterBusy));

  RunFor(base::TimeDelta::FromSeconds(3));

  EXPECT_TRUE(SuccessfulResponseToURL(GetUrl("/privet/printer/createjob"),
                                      kSampleCreatejobResponse));
}
#endif  // ENABLE_PRINT_PREVIEW

class PrivetHttpWithServerTest : public ::testing::Test,
                                 public PrivetURLFetcher::Delegate {
 protected:
  PrivetHttpWithServerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  void SetUp() override {
    context_getter_ = new net::TestURLRequestContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

    server_.reset(new EmbeddedTestServer(EmbeddedTestServer::TYPE_HTTP));
    ASSERT_TRUE(server_->Start());

    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    server_->ServeFilesFromDirectory(
        test_data_dir.Append(FILE_PATH_LITERAL("chrome/test/data")));

    client_.reset(new PrivetHTTPClientImpl("test", server_->host_port_pair(),
                                           context_getter_));
  }

  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override {
    callback.Run("abc");
  }

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override {
    done_ = true;
    success_ = false;
    error_ = error;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_);
  }

  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override {
    NOTREACHED();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_);
  }

  bool OnRawData(PrivetURLFetcher* fetcher,
                 bool response_is_file,
                 const std::string& data_string,
                 const base::FilePath& data_file) override {
    done_ = true;
    success_ = true;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_);
    return true;
  }

  bool Run() {
    success_ = false;
    done_ = false;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();

    std::unique_ptr<PrivetURLFetcher> fetcher = client_->CreateURLFetcher(
        server_->GetURL("/simple.html"), net::URLFetcher::GET, this);

    fetcher->SetMaxRetries(1);
    fetcher->Start();

    run_loop.Run();

    EXPECT_TRUE(done_);
    return success_;
  }

  bool success_ = false;
  bool done_ = false;
  PrivetURLFetcher::ErrorType error_ = PrivetURLFetcher::ErrorType();
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;
  std::unique_ptr<EmbeddedTestServer> server_;
  std::unique_ptr<PrivetHTTPClientImpl> client_;

  base::Closure quit_;
};

TEST_F(PrivetHttpWithServerTest, HttpServer) {
  EXPECT_TRUE(Run());
}

}  // namespace

}  // namespace cloud_print
