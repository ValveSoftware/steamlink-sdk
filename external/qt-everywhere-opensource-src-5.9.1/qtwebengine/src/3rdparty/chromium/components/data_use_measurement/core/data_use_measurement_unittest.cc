// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class UserRequestUserDataForTesting : public base::SupportsUserData::Data,
                                      public URLRequestClassifier {
 public:
  static const void* const kUserDataKey;

  bool IsUserRequest(const net::URLRequest& request) const override {
    return request.GetUserData(kUserDataKey) != nullptr;
  }

  static void MarkAsUserRequest(net::URLRequest* request) {
    request->SetUserData(kUserDataKey, new UserRequestUserDataForTesting());
  }
};

// static
const void* const UserRequestUserDataForTesting::kUserDataKey =
    &UserRequestUserDataForTesting::kUserDataKey;

class DataUseMeasurementTest : public testing::Test {
 public:
  DataUseMeasurementTest()
      : data_use_measurement_(
            base::MakeUnique<UserRequestUserDataForTesting>(),
            base::Bind(&DataUseMeasurementTest::FakeDataUseforwarder,
                       base::Unretained(this))) {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  // Creates a test request.
  std::unique_ptr<net::URLRequest> CreateTestRequest(bool is_user_request) {
    net::TestDelegate test_delegate;
    InitializeContext();
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                           "Content-Length: 12\r\n\r\n"),
                             net::MockRead("Test Content")};
    net::StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr,
                                              0);
    socket_factory_->AddSocketDataProvider(&socket_data);

    std::unique_ptr<net::URLRequest> request(context_->CreateRequest(
        GURL("http://foo.com"), net::DEFAULT_PRIORITY, &test_delegate));
    if (is_user_request) {
      UserRequestUserDataForTesting::MarkAsUserRequest(request.get());
    } else {
      request->SetUserData(
          data_use_measurement::DataUseUserData::kUserDataKey,
          new data_use_measurement::DataUseUserData(
              data_use_measurement::DataUseUserData::SUGGESTIONS,
              data_use_measurement_.CurrentAppState()));
    }

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  // Sends a request and reports data use attaching either user data or service
  // data based on |is_user_request|.
  void SendRequest(bool is_user_request) {
    std::unique_ptr<net::URLRequest> request =
        CreateTestRequest(is_user_request);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnCompleted(*request, true);
  }

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    SendRequest(true);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Upstream." + target_dimension +
            kConnectionType,
        0);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Downstream." + target_dimension +
            kConnectionType,
        0);
  }

  // This function makes a service request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAServiceRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    SendRequest(false);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
    // One upload and one download message, so total count should be 2.
    histogram_tester.ExpectTotalCount("DataUse.MessageSize.Suggestions", 2);
  }

  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

  bool IsDataUseForwarderCalled() { return is_data_use_forwarder_called_; }

 protected:
  void InitializeContext() {
    context_.reset(new net::TestURLRequestContext(true));
    socket_factory_.reset(new net::MockClientSocketFactory());
    context_->set_client_socket_factory(socket_factory_.get());
    context_->Init();
  }

  void FakeDataUseforwarder(const std::string& service_name,
                            int message_size,
                            bool is_celllular) {
    is_data_use_forwarder_called_ = true;
  }

  base::MessageLoopForIO loop_;
  DataUseMeasurement data_use_measurement_;
  std::unique_ptr<net::MockClientSocketFactory> socket_factory_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  const std::string kConnectionType = "NotCellular";
  bool is_data_use_forwarder_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurementTest);
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST_F(DataUseMeasurementTest, UserNotUserTest) {
#if defined(OS_ANDROID)
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  TestForAServiceRequest("Foreground.");
  TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST_F(DataUseMeasurementTest, ApplicationStateTest) {
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  TestForAServiceRequest("Background.");
  TestForAUserRequest("Background.");
}
#endif

TEST_F(DataUseMeasurementTest, DataUseForwarderIsCalled) {
  EXPECT_FALSE(IsDataUseForwarderCalled());
  SendRequest(true);
  EXPECT_TRUE(IsDataUseForwarderCalled());
}

#if defined(OS_ANDROID)
TEST_F(DataUseMeasurementTest, AppStateUnknown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
  data_use_measurement_.OnBeforeURLRequest(request.get());

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Foreground." + kConnectionType, 1);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Upstream.Foreground." + kConnectionType, 1);
  }

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Unknown." + kConnectionType, 1);
  }

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Background." + kConnectionType, 1);
  }
}

TEST_F(DataUseMeasurementTest, TimeOfBackgroundDownstreamBytes) {
  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChange(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    // Create new request when app is in foreground..
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChange(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 1);
  }

  {
    // Create new request when app is in background.
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    // Create new request when app is in background.
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(false);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.System", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.System", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(true);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChange(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }
}
#endif

}  // namespace data_use_measurement
