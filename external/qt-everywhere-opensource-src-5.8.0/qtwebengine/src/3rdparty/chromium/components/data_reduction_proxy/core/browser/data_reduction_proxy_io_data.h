// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_use_group_provider.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "components/data_reduction_proxy/core/common/lofi_ui_service.h"

namespace base {
class Value;
}

namespace net {
class NetLog;
class URLRequestContextGetter;
class URLRequestInterceptor;
}

namespace data_reduction_proxy {

class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigServiceClient;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyService;

// Contains and initializes all Data Reduction Proxy objects that operate on
// the IO thread.
class DataReductionProxyIOData : public DataReductionProxyEventStorageDelegate {
 public:
  // Constructs a DataReductionProxyIOData object. |param_flags| is used to
  // set information about the DNS names used by the proxy, and allowable
  // configurations. |enabled| sets the initial state of the Data Reduction
  // Proxy.
  DataReductionProxyIOData(
      Client client,
      int param_flags,
      net::NetLog* net_log,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      bool enabled,
      const std::string& user_agent,
      const std::string& channel);

  virtual ~DataReductionProxyIOData();

  // Performs UI thread specific shutdown logic.
  void ShutdownOnUIThread();

  // Sets the Data Reduction Proxy service after it has been created.
  // Virtual for testing.
  virtual void SetDataReductionProxyService(
      base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service);

  // Creates an interceptor suitable for following the Data Reduction Proxy
  // bypass protocol.
  std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor();

  // Creates a NetworkDelegate suitable for carrying out the Data Reduction
  // Proxy protocol, including authenticating, establishing a handler to
  // override the current proxy configuration, and
  // gathering statistics for UMA.
  std::unique_ptr<DataReductionProxyNetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate,
      bool track_proxy_bypass_statistics);

  std::unique_ptr<DataReductionProxyDelegate> CreateProxyDelegate() const;

  // Sets user defined preferences for how the Data Reduction Proxy
  // configuration should be set. |at_startup| is true only
  // when DataReductionProxySettings is initialized.
  void SetProxyPrefs(bool enabled, bool at_startup);

  // Applies a serialized Data Reduction Proxy configuration.
  void SetDataReductionProxyConfiguration(const std::string& serialized_config);

  // Returns true when Lo-Fi mode should be activated. When Lo-Fi mode is
  // active, URL requests are modified to request low fidelity versions of the
  // resources, except when the user is in the Lo-Fi control group.
  bool ShouldEnableLoFiMode(const net::URLRequest& request);

  // Sets Lo-Fi mode off in |config_|.
  void SetLoFiModeOff();

  // Bridge methods to safely call to the UI thread objects.
  void UpdateContentLengths(
      int64_t data_used,
      int64_t original_size,
      bool data_reduction_proxy_enabled,
      DataReductionProxyRequestType request_type,
      const scoped_refptr<DataUseGroup>& data_usage_source,
      const std::string& mime_type);
  void SetLoFiModeActiveOnMainFrame(bool lo_fi_mode_active);

  // Overrides of DataReductionProxyEventStorageDelegate. Bridges to the UI
  // thread objects.
  void AddEvent(std::unique_ptr<base::Value> event) override;
  void AddEnabledEvent(std::unique_ptr<base::Value> event,
                       bool enabled) override;
  void AddEventAndSecureProxyCheckState(std::unique_ptr<base::Value> event,
                                        SecureProxyCheckState state) override;
  void AddAndSetLastBypassEvent(std::unique_ptr<base::Value> event,
                                int64_t expiration_ticks) override;

  // Returns true if the Data Reduction Proxy is enabled and false otherwise.
  bool IsEnabled() const;

  // Changes the reporting fraction for the pingback service to
  // |pingback_reporting_fraction|. Overridden in testing.
  virtual void SetPingbackReportingFraction(float pingback_reporting_fraction);

  // Various accessor methods.
  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  DataReductionProxyConfig* config() const {
    return config_.get();
  }

  DataReductionProxyEventCreator* event_creator() const {
    return event_creator_.get();
  }

  DataReductionProxyRequestOptions* request_options() const {
    return request_options_.get();
  }

  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }

  net::ProxyDelegate* proxy_delegate() const {
    return proxy_delegate_.get();
  }

  net::NetLog* net_log() {
    return net_log_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  // Used for testing.
  DataReductionProxyBypassStats* bypass_stats() const {
    return bypass_stats_.get();
  }

  LoFiDecider* lofi_decider() const { return lofi_decider_.get(); }

  void set_lofi_decider(std::unique_ptr<LoFiDecider> lofi_decider) const {
    lofi_decider_ = std::move(lofi_decider);
  }

  LoFiUIService* lofi_ui_service() const { return lofi_ui_service_.get(); }

  // Takes ownership of |lofi_ui_service|.
  void set_lofi_ui_service(
      std::unique_ptr<LoFiUIService> lofi_ui_service) const {
    lofi_ui_service_ = std::move(lofi_ui_service);
  }

  void set_data_usage_source_provider(
      std::unique_ptr<DataUseGroupProvider> data_usage_source_provider) {
    data_use_group_provider_ = std::move(data_usage_source_provider);
  }

  // The production channel of this build.
  std::string channel() const { return channel_; }

  // The Client type of this build.
  Client client() const { return client_; }

 private:
  friend class TestDataReductionProxyIOData;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest, TestConstruction);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest,
                           TestResetBadProxyListOnDisableDataSaver);

  // Used for testing.
  DataReductionProxyIOData();

  // Initializes the weak pointer to |this| on the IO thread. It must be done
  // on the IO thread, since it is used for posting tasks from the UI thread
  // to IO thread objects in a thread safe manner.
  void InitializeOnIOThread();

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Stores an int64_t value in preferences storage.
  void SetInt64Pref(const std::string& pref_path, int64_t value);

  // Stores a string value in preferences storage.
  void SetStringPref(const std::string& pref_path, const std::string& value);

  // Stores a serialized Data Reduction Proxy configuration in preferences
  // storage.
  void StoreSerializedConfig(const std::string& serialized_config);

  // The type of Data Reduction Proxy client.
  const Client client_;

  // Parameters including DNS names and allowable configurations.
  std::unique_ptr<DataReductionProxyConfig> config_;

  // Handles getting if a request is in Lo-Fi mode.
  mutable std::unique_ptr<LoFiDecider> lofi_decider_;

  // Handles showing Lo-Fi UI when a Lo-Fi response is received.
  mutable std::unique_ptr<LoFiUIService> lofi_ui_service_;

  // Creates Data Reduction Proxy-related events for logging.
  std::unique_ptr<DataReductionProxyEventCreator> event_creator_;

  // Setter of the Data Reduction Proxy-specific proxy configuration.
  std::unique_ptr<DataReductionProxyConfigurator> configurator_;

  // A proxy delegate. Used, for example, for Data Reduction Proxy resolution.
  // request.
  std::unique_ptr<DataReductionProxyDelegate> proxy_delegate_;

  // Data Reduction Proxy objects with a UI based lifetime.
  base::WeakPtr<DataReductionProxyService> service_;

  // Tracker of various metrics to be reported in UMA.
  std::unique_ptr<DataReductionProxyBypassStats> bypass_stats_;

  // Constructs credentials suitable for authenticating the client.
  std::unique_ptr<DataReductionProxyRequestOptions> request_options_;

  // Requests new Data Reduction Proxy configurations from a remote service.
  std::unique_ptr<DataReductionProxyConfigServiceClient> config_client_;

  // A net log.
  net::NetLog* net_log_;

  // IO and UI task runners, respectively.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Manages instances of |DataUsageSource| and maps |URLRequest| instances to
  // their appropriate |DataUsageSource|.
  std::unique_ptr<DataUseGroupProvider> data_use_group_provider_;

  // Whether the Data Reduction Proxy has been enabled or not by the user. In
  // practice, this can be overridden by the command line.
  bool enabled_;

  // The net::URLRequestContextGetter used for making URL requests.
  net::URLRequestContextGetter* url_request_context_getter_;

  // A net::URLRequestContextGetter used for making secure proxy checks. It
  // does not use alternate protocols.
  scoped_refptr<net::URLRequestContextGetter> basic_url_request_context_getter_;

  // The production channel of this build.
  const std::string channel_;

  base::WeakPtrFactory<DataReductionProxyIOData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyIOData);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
