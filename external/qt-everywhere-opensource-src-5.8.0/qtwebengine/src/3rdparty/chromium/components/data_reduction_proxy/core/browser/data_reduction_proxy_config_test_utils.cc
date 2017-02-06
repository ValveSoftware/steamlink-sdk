// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace net {
class NetworkQualityEstimator;
}

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    int params_flags,
    unsigned int params_definitions,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(
          base::WrapUnique(
              new TestDataReductionProxyParams(params_flags,
                                               params_definitions)),
          io_task_runner,
          net_log,
          configurator,
          event_creator) {}

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : DataReductionProxyConfig(io_task_runner,
                               net_log,
                               std::move(config_values),
                               configurator,
                               event_creator),
      tick_clock_(nullptr),
      network_quality_prohibitively_slow_set_(false),
      network_quality_prohibitively_slow_(false),
      lofi_accuracy_recording_intervals_set_(false) {
  network_interfaces_.reset(new net::NetworkInterfaceList());
}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

bool TestDataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  if (network_quality_prohibitively_slow_set_)
    return network_quality_prohibitively_slow_;
  return DataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
      network_quality_estimator);
}

void TestDataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  for (size_t i = 0; i < network_interfaces_->size(); ++i)
    interfaces->push_back(network_interfaces_->at(i));
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest(int flags) {
  config_values_ = base::WrapUnique(new TestDataReductionProxyParams(
      flags, TestDataReductionProxyParams::HAS_EVERYTHING));
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

DataReductionProxyConfigValues* TestDataReductionProxyConfig::config_values() {
  return config_values_.get();
}

void TestDataReductionProxyConfig::SetStateForTest(
    bool enabled_by_user,
    bool secure_proxy_allowed) {
  enabled_by_user_ = enabled_by_user;
  secure_proxy_allowed_ = secure_proxy_allowed;
}

void TestDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

void TestDataReductionProxyConfig::SetNetworkProhibitivelySlow(
    bool network_quality_prohibitively_slow) {
  network_quality_prohibitively_slow_set_ = true;
  network_quality_prohibitively_slow_ = network_quality_prohibitively_slow;
}

void TestDataReductionProxyConfig::SetLofiAccuracyRecordingIntervals(
    const std::vector<base::TimeDelta>& lofi_accuracy_recording_intervals) {
  lofi_accuracy_recording_intervals_set_ = true;
  lofi_accuracy_recording_intervals_ = lofi_accuracy_recording_intervals;
}

const std::vector<base::TimeDelta>&
TestDataReductionProxyConfig::GetLofiAccuracyRecordingIntervals() const {
  if (lofi_accuracy_recording_intervals_set_)
    return lofi_accuracy_recording_intervals_;
  return DataReductionProxyConfig::GetLofiAccuracyRecordingIntervals();
}

void TestDataReductionProxyConfig::SetTickClock(base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

base::TimeTicks TestDataReductionProxyConfig::GetTicksNow() const {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return DataReductionProxyConfig::GetTicksNow();
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(std::move(config_values),
                                   io_task_runner,
                                   net_log,
                                   configurator,
                                   event_creator) {}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

void MockDataReductionProxyConfig::UpdateConfigurator(
    bool enabled,
    bool secure_proxy_allowed) {
  DataReductionProxyConfig::UpdateConfigurator(enabled, secure_proxy_allowed);
}

void MockDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

}  // namespace data_reduction_proxy
