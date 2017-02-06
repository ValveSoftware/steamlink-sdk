// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyMessageFilterTest : public testing::Test {
 public:
  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed)
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .WithMockConfig()
            .Build();
    message_filter_ = new DataReductionProxyMessageFilter(
        test_context_->settings());
  }

 protected:
  DataReductionProxyMessageFilter* message_filter() const {
    return message_filter_.get();
  }

  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  scoped_refptr<DataReductionProxyMessageFilter> message_filter_;
};

TEST_F(DataReductionProxyMessageFilterTest, TestOnIsDataReductionProxy) {
  net::HostPortPair proxy_server =
      net::HostPortPair::FromString("www.google.com:443");
  bool is_data_reduction_proxy = false;
  EXPECT_CALL(*config(), IsDataReductionProxy(testing::_, nullptr))
      .Times(1)
      .WillOnce(testing::Return(true));
  message_filter()->OnIsDataReductionProxy(proxy_server,
                                           &is_data_reduction_proxy);
  EXPECT_TRUE(is_data_reduction_proxy);

  EXPECT_CALL(*config(), IsDataReductionProxy(testing::_, nullptr))
      .Times(1)
      .WillOnce(testing::Return(false));
  message_filter()->OnIsDataReductionProxy(proxy_server,
                                           &is_data_reduction_proxy);
  EXPECT_FALSE(is_data_reduction_proxy);
}

}  // namespace data_reduction_proxy
