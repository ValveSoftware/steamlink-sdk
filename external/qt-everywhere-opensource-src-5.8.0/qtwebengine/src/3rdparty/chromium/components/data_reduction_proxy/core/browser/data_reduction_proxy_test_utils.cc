// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include <map>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

namespace {

const char kTestKey[] = "test-key";

// Name of the preference that governs enabling the Data Reduction Proxy.
const char kDataReductionProxyEnabled[] = "data_reduction_proxy.enabled";

const net::BackoffEntry::Policy kTestBackoffPolicy = {
    0,               // num_errors_to_ignore
    10 * 1000,       // initial_delay_ms
    2,               // multiply_factor
    0,               // jitter_factor
    30 * 60 * 1000,  // maximum_backoff_ms
    -1,              // entry_lifetime_ms
    true,            // always_use_initial_delay
};

}  // namespace

namespace data_reduction_proxy {

TestDataReductionProxyRequestOptions::TestDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : DataReductionProxyRequestOptions(client, version, config) {
}

std::string TestDataReductionProxyRequestOptions::GetDefaultKey() const {
  return kTestKey;
}

base::Time TestDataReductionProxyRequestOptions::Now() const {
  return base::Time::UnixEpoch() + now_offset_;
}

void TestDataReductionProxyRequestOptions::RandBytes(void* output,
                                                     size_t length) const {
  char* c = static_cast<char*>(output);
  for (size_t i = 0; i < length; ++i) {
    c[i] = 'a';
  }
}

// Time after the unix epoch that Now() reports.
void TestDataReductionProxyRequestOptions::set_offset(
    const base::TimeDelta& now_offset) {
  now_offset_ = now_offset;
}

MockDataReductionProxyRequestOptions::MockDataReductionProxyRequestOptions(
    Client client,
    DataReductionProxyConfig* config)
    : TestDataReductionProxyRequestOptions(client, "1.2.3.4", config) {}

MockDataReductionProxyRequestOptions::~MockDataReductionProxyRequestOptions() {
}

TestDataReductionProxyConfigServiceClient::
    TestDataReductionProxyConfigServiceClient(
        std::unique_ptr<DataReductionProxyParams> params,
        DataReductionProxyRequestOptions* request_options,
        DataReductionProxyMutableConfigValues* config_values,
        DataReductionProxyConfig* config,
        DataReductionProxyEventCreator* event_creator,
        DataReductionProxyIOData* io_data,
        net::NetLog* net_log,
        ConfigStorer config_storer)
    : DataReductionProxyConfigServiceClient(std::move(params),
                                            kTestBackoffPolicy,
                                            request_options,
                                            config_values,
                                            config,
                                            event_creator,
                                            io_data,
                                            net_log,
                                            config_storer),
#if defined(OS_ANDROID)
      is_application_state_background_(false),
#endif
      tick_clock_(base::Time::UnixEpoch()),
      test_backoff_entry_(&kTestBackoffPolicy, &tick_clock_) {
}

TestDataReductionProxyConfigServiceClient::
    ~TestDataReductionProxyConfigServiceClient() {
}

void TestDataReductionProxyConfigServiceClient::SetNow(const base::Time& time) {
  tick_clock_.SetTime(time);
}

void TestDataReductionProxyConfigServiceClient::SetCustomReleaseTime(
    const base::TimeTicks& release_time) {
  test_backoff_entry_.SetCustomReleaseTime(release_time);
}

base::TimeDelta TestDataReductionProxyConfigServiceClient::GetDelay() const {
  return config_refresh_timer_.GetCurrentDelay();
}

int TestDataReductionProxyConfigServiceClient::GetBackoffErrorCount() {
  return test_backoff_entry_.failure_count();
}

void TestDataReductionProxyConfigServiceClient::SetConfigServiceURL(
    const GURL& service_url) {
  config_service_url_ = service_url;
}

int32_t
TestDataReductionProxyConfigServiceClient::failed_attempts_before_success()
    const {
  return failed_attempts_before_success_;
}

base::Time TestDataReductionProxyConfigServiceClient::Now() {
  return tick_clock_.Now();
}

net::BackoffEntry*
TestDataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &test_backoff_entry_;
}

TestDataReductionProxyConfigServiceClient::TestTickClock::TestTickClock(
    const base::Time& initial_time)
    : time_(initial_time) {
}

base::TimeTicks
TestDataReductionProxyConfigServiceClient::TestTickClock::NowTicks() {
  return base::TimeTicks::UnixEpoch() + (time_ - base::Time::UnixEpoch());
}

base::Time
TestDataReductionProxyConfigServiceClient::TestTickClock::Now() {
  return time_;
}

void TestDataReductionProxyConfigServiceClient::TestTickClock::SetTime(
    const base::Time& time) {
  time_ = time;
}

#if defined(OS_ANDROID)
bool TestDataReductionProxyConfigServiceClient::IsApplicationStateBackground()
    const {
  return is_application_state_background_;
}

void TestDataReductionProxyConfigServiceClient::
    TriggerApplicationStatusToForeground() {
  OnApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}
#endif

MockDataReductionProxyService::MockDataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : DataReductionProxyService(settings,
                                prefs,
                                request_context,
                                base::WrapUnique(new TestDataStore()),
                                task_runner,
                                task_runner,
                                task_runner,
                                base::TimeDelta()) {}

MockDataReductionProxyService::~MockDataReductionProxyService() {
}

TestDataReductionProxyIOData::TestDataReductionProxyIOData(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<DataReductionProxyConfig> config,
    std::unique_ptr<DataReductionProxyEventCreator> event_creator,
    std::unique_ptr<DataReductionProxyRequestOptions> request_options,
    std::unique_ptr<DataReductionProxyConfigurator> configurator,
    net::NetLog* net_log,
    bool enabled)
    : DataReductionProxyIOData(),
      service_set_(false),
      pingback_reporting_fraction_(0.0f) {
  io_task_runner_ = task_runner;
  ui_task_runner_ = task_runner;
  config_ = std::move(config);
  event_creator_ = std::move(event_creator);
  request_options_ = std::move(request_options);
  configurator_ = std::move(configurator);
  net_log_ = net_log;
  bypass_stats_.reset(new DataReductionProxyBypassStats(
      config_.get(), base::Bind(&DataReductionProxyIOData::SetUnreachable,
                                base::Unretained(this))));
  enabled_ = enabled;
}

TestDataReductionProxyIOData::~TestDataReductionProxyIOData() {
}

void TestDataReductionProxyIOData::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  pingback_reporting_fraction_ = pingback_reporting_fraction;
}

void TestDataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  if (!service_set_)
    DataReductionProxyIOData::SetDataReductionProxyService(
        data_reduction_proxy_service);

  service_set_ = true;
}

TestDataStore::TestDataStore() {}

TestDataStore::~TestDataStore() {}

DataStore::Status TestDataStore::Get(base::StringPiece key,
                                     std::string* value) {
  auto value_iter = map_.find(key.as_string());
  if (value_iter == map_.end())
    return NOT_FOUND;

  value->assign(value_iter->second);
  return OK;
}

DataStore::Status TestDataStore::Put(
    const std::map<std::string, std::string>& map) {
  for (auto iter = map.begin(); iter != map.end(); ++iter)
    map_[iter->first] = iter->second;

  return OK;
}

DataStore::Status TestDataStore::Delete(base::StringPiece key) {
  map_.erase(key.as_string());

  return OK;
}

DataReductionProxyTestContext::Builder::Builder()
    : params_flags_(DataReductionProxyParams::kAllowed |
                    DataReductionProxyParams::kFallbackAllowed |
                    DataReductionProxyParams::kPromoAllowed),
      params_definitions_(TestDataReductionProxyParams::HAS_EVERYTHING),
      client_(Client::UNKNOWN),
      request_context_(nullptr),
      mock_socket_factory_(nullptr),
      use_mock_config_(false),
      use_mock_service_(false),
      use_mock_request_options_(false),
      use_config_client_(false),
      use_test_config_client_(false),
      skip_settings_initialization_(false) {}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsFlags(int params_flags) {
  params_flags_ = params_flags;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsDefinitions(
    unsigned int params_definitions) {
  params_definitions_ = params_definitions;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithURLRequestContext(
    net::URLRequestContext* request_context) {
  request_context_ = request_context;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockClientSocketFactory(
    net::MockClientSocketFactory* mock_socket_factory) {
  mock_socket_factory_ = mock_socket_factory;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithClient(Client client) {
  client_ = client;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockConfig() {
  use_mock_config_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockDataReductionProxyService() {
  use_mock_service_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockRequestOptions() {
  use_mock_request_options_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithConfigClient() {
  use_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigClient() {
  use_config_client_ = true;
  use_test_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::SkipSettingsInitialization() {
  skip_settings_initialization_ = true;
  return *this;
}

std::unique_ptr<DataReductionProxyTestContext>
DataReductionProxyTestContext::Builder::Build() {
  // Check for invalid builder combinations.
  DCHECK(!(use_mock_config_ && use_config_client_));

  unsigned int test_context_flags = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
  std::unique_ptr<TestingPrefServiceSimple> pref_service(
      new TestingPrefServiceSimple());
  std::unique_ptr<net::TestNetLog> net_log(new net::TestNetLog());
  std::unique_ptr<TestConfigStorer> config_storer(
      new TestConfigStorer(pref_service.get()));
  if (request_context_) {
    request_context_getter = new net::TrivialURLRequestContextGetter(
        request_context_, task_runner);
  } else {
    std::unique_ptr<net::TestURLRequestContext> test_request_context(
        new net::TestURLRequestContext(true));
    if (mock_socket_factory_)
      test_request_context->set_client_socket_factory(mock_socket_factory_);
    test_request_context->Init();
    request_context_getter = new net::TestURLRequestContextGetter(
        task_runner, std::move(test_request_context));
  }

  std::unique_ptr<TestDataReductionProxyEventStorageDelegate> storage_delegate(
      new TestDataReductionProxyEventStorageDelegate());
  std::unique_ptr<DataReductionProxyEventCreator> event_creator(
      new DataReductionProxyEventCreator(storage_delegate.get()));
  std::unique_ptr<DataReductionProxyConfigurator> configurator(
      new DataReductionProxyConfigurator(net_log.get(), event_creator.get()));

  std::unique_ptr<TestDataReductionProxyConfig> config;
  std::unique_ptr<DataReductionProxyConfigServiceClient> config_client;
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams(params_flags_, params_definitions_));
  TestDataReductionProxyParams* raw_params = params.get();
  if (use_config_client_) {
    test_context_flags |= USE_CONFIG_CLIENT;
    std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        DataReductionProxyMutableConfigValues::CreateFromParams(params.get());
    raw_mutable_config = mutable_config.get();
    config.reset(new TestDataReductionProxyConfig(
        std::move(mutable_config), task_runner, net_log.get(),
        configurator.get(), event_creator.get()));
  } else if (use_mock_config_) {
    test_context_flags |= USE_MOCK_CONFIG;
    config.reset(new MockDataReductionProxyConfig(
        std::move(params), task_runner, net_log.get(), configurator.get(),
        event_creator.get()));
  } else {
    config.reset(new TestDataReductionProxyConfig(
        std::move(params), task_runner, net_log.get(), configurator.get(),
        event_creator.get()));
  }

  std::unique_ptr<DataReductionProxyRequestOptions> request_options;
  if (use_mock_request_options_) {
    test_context_flags |= USE_MOCK_REQUEST_OPTIONS;
    request_options.reset(
        new MockDataReductionProxyRequestOptions(client_, config.get()));
  } else {
    request_options.reset(
        new DataReductionProxyRequestOptions(client_, config.get()));
  }

  std::unique_ptr<DataReductionProxySettings> settings(
      new DataReductionProxySettings());
  if (skip_settings_initialization_) {
    settings->set_data_reduction_proxy_enabled_pref_name_for_test(
        kDataReductionProxyEnabled);
    test_context_flags |= SKIP_SETTINGS_INITIALIZATION;
  }

  if (use_mock_service_)
    test_context_flags |= USE_MOCK_SERVICE;

  pref_service->registry()->RegisterBooleanPref(kDataReductionProxyEnabled,
                                                false);
  RegisterSimpleProfilePrefs(pref_service->registry());

  std::unique_ptr<TestDataReductionProxyIOData> io_data(
      new TestDataReductionProxyIOData(
          task_runner, std::move(config), std::move(event_creator),
          std::move(request_options), std::move(configurator), net_log.get(),
          true /* enabled */));
  io_data->SetSimpleURLRequestContextGetter(request_context_getter);

  if (use_test_config_client_) {
    test_context_flags |= USE_TEST_CONFIG_CLIENT;
    config_client.reset(new TestDataReductionProxyConfigServiceClient(
        std::move(params), io_data->request_options(), raw_mutable_config,
        io_data->config(), io_data->event_creator(), io_data.get(),
        net_log.get(), base::Bind(&TestConfigStorer::StoreSerializedConfig,
                                  base::Unretained(config_storer.get()))));
  } else if (use_config_client_) {
    config_client.reset(new DataReductionProxyConfigServiceClient(
        std::move(params), GetBackoffPolicy(), io_data->request_options(),
        raw_mutable_config, io_data->config(), io_data->event_creator(),
        io_data.get(), net_log.get(),
        base::Bind(&TestConfigStorer::StoreSerializedConfig,
                   base::Unretained(config_storer.get()))));
  }
  io_data->set_config_client(std::move(config_client));

  std::unique_ptr<DataReductionProxyTestContext> test_context(
      new DataReductionProxyTestContext(
          task_runner, std::move(pref_service), std::move(net_log),
          request_context_getter, mock_socket_factory_, std::move(io_data),
          std::move(settings), std::move(storage_delegate),
          std::move(config_storer), raw_params, test_context_flags));

  if (!skip_settings_initialization_)
    test_context->InitSettingsWithoutCheck();

  return test_context;
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
    std::unique_ptr<net::TestNetLog> net_log,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    net::MockClientSocketFactory* mock_socket_factory,
    std::unique_ptr<TestDataReductionProxyIOData> io_data,
    std::unique_ptr<DataReductionProxySettings> settings,
    std::unique_ptr<TestDataReductionProxyEventStorageDelegate>
        storage_delegate,
    std::unique_ptr<TestConfigStorer> config_storer,
    TestDataReductionProxyParams* params,
    unsigned int test_context_flags)
    : test_context_flags_(test_context_flags),
      task_runner_(task_runner),
      simple_pref_service_(std::move(simple_pref_service)),
      net_log_(std::move(net_log)),
      request_context_getter_(request_context_getter),
      mock_socket_factory_(mock_socket_factory),
      io_data_(std::move(io_data)),
      settings_(std::move(settings)),
      storage_delegate_(std::move(storage_delegate)),
      config_storer_(std::move(config_storer)),
      params_(params) {}

DataReductionProxyTestContext::~DataReductionProxyTestContext() {
  DestroySettings();
}

const char*
DataReductionProxyTestContext::GetDataReductionProxyEnabledPrefName() const {
  return kDataReductionProxyEnabled;
}

void DataReductionProxyTestContext::RegisterDataReductionProxyEnabledPref() {
  simple_pref_service_->registry()->RegisterBooleanPref(
      kDataReductionProxyEnabled, false);
}

void DataReductionProxyTestContext::SetDataReductionProxyEnabled(bool enabled) {
  simple_pref_service_->SetBoolean(kDataReductionProxyEnabled, enabled);
}

bool DataReductionProxyTestContext::IsDataReductionProxyEnabled() const {
  return simple_pref_service_->GetBoolean(kDataReductionProxyEnabled);
}

void DataReductionProxyTestContext::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void DataReductionProxyTestContext::InitSettings() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  InitSettingsWithoutCheck();
}

void DataReductionProxyTestContext::DestroySettings() {
  // Force destruction of |DBDataOwner|, which lives on DB task runner and is
  // indirectly owned by |settings_|.
  if (settings_.get()) {
    settings_.reset();
    RunUntilIdle();
  }
}

void DataReductionProxyTestContext::InitSettingsWithoutCheck() {
  settings_->InitDataReductionProxySettings(
      kDataReductionProxyEnabled, simple_pref_service_.get(), io_data_.get(),
      CreateDataReductionProxyServiceInternal(settings_.get()));
  storage_delegate_->SetStorageDelegate(
      settings_->data_reduction_proxy_service()->event_store());
  io_data_->SetDataReductionProxyService(
      settings_->data_reduction_proxy_service()->GetWeakPtr());
  if (io_data_->config_client())
    io_data_->config_client()->InitializeOnIOThread(
        request_context_getter_.get());
  settings_->data_reduction_proxy_service()->SetIOData(io_data_->GetWeakPtr());
}

std::unique_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyService(
    DataReductionProxySettings* settings) {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  return CreateDataReductionProxyServiceInternal(settings);
}

std::unique_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyServiceInternal(
    DataReductionProxySettings* settings) {
  if (test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE) {
    return base::WrapUnique(new MockDataReductionProxyService(
        settings, simple_pref_service_.get(), request_context_getter_.get(),
        task_runner_));
  } else {
    return base::WrapUnique(new DataReductionProxyService(
        settings, simple_pref_service_.get(), request_context_getter_.get(),
        base::WrapUnique(new TestDataStore()), task_runner_, task_runner_,
        task_runner_, base::TimeDelta()));
  }
}

void DataReductionProxyTestContext::AttachToURLRequestContext(
      net::URLRequestContextStorage* request_context_storage) const {
  DCHECK(request_context_storage);

  // |request_context_storage| takes ownership of the network delegate.
  request_context_storage->set_network_delegate(
      io_data()->CreateNetworkDelegate(
          base::WrapUnique(new net::TestNetworkDelegate()), true));

  request_context_storage->set_job_factory(
      base::WrapUnique(new net::URLRequestInterceptingJobFactory(
          std::unique_ptr<net::URLRequestJobFactory>(
              new net::URLRequestJobFactoryImpl()),
          io_data()->CreateInterceptor())));
}

void DataReductionProxyTestContext::
    EnableDataReductionProxyWithSecureProxyCheckSuccess() {
  DCHECK(mock_socket_factory_);
  // |settings_| needs to have been initialized, since a
  // |DataReductionProxyService| is needed in order to issue the secure proxy
  // check.
  DCHECK(data_reduction_proxy_service());

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check.
  net::MockRead mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead("OK"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      mock_reads, arraysize(mock_reads), nullptr, 0);
  mock_socket_factory_->AddSocketDataProvider(&socket_data_provider);

  // Set the pref to cause the secure proxy check to be issued.
  pref_service()->SetBoolean(kDataReductionProxyEnabled, true);
  RunUntilIdle();
}

MockDataReductionProxyConfig* DataReductionProxyTestContext::mock_config()
    const {
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_CONFIG);
  return reinterpret_cast<MockDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyService*
DataReductionProxyTestContext::data_reduction_proxy_service() const {
  return settings_->data_reduction_proxy_service();
}

MockDataReductionProxyService*
DataReductionProxyTestContext::mock_data_reduction_proxy_service()
    const {
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE);
  return reinterpret_cast<MockDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyRequestOptions*
DataReductionProxyTestContext::mock_request_options() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_MOCK_REQUEST_OPTIONS);
  return reinterpret_cast<MockDataReductionProxyRequestOptions*>(
      io_data_->request_options());
}

TestDataReductionProxyConfig* DataReductionProxyTestContext::config() const {
  return reinterpret_cast<TestDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyMutableConfigValues*
DataReductionProxyTestContext::mutable_config_values() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_CONFIG_CLIENT);
  return reinterpret_cast<DataReductionProxyMutableConfigValues*>(
      config()->config_values());
}

TestDataReductionProxyConfigServiceClient*
DataReductionProxyTestContext::test_config_client() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIG_CLIENT);
  return reinterpret_cast<TestDataReductionProxyConfigServiceClient*>(
      io_data_->config_client());
}

DataReductionProxyBypassStats::UnreachableCallback
DataReductionProxyTestContext::unreachable_callback() const {
  return base::Bind(&DataReductionProxySettings::SetUnreachable,
                    base::Unretained(settings_.get()));
}

DataReductionProxyTestContext::TestConfigStorer::TestConfigStorer(
    PrefService* prefs)
    : prefs_(prefs) {
  DCHECK(prefs);
}

void DataReductionProxyTestContext::TestConfigStorer::StoreSerializedConfig(
    const std::string& serialized_config) {
  prefs_->SetString(prefs::kDataReductionProxyConfig, serialized_config);
}

std::vector<net::ProxyServer>
DataReductionProxyTestContext::GetConfiguredProxiesForHttp() const {
  const GURL kHttpUrl("http://test_http_url.net");
  // The test URL shouldn't match any of the bypass rules in the proxy rules.
  DCHECK(!configurator()->GetProxyConfig().proxy_rules().bypass_rules.Matches(
      kHttpUrl));

  net::ProxyInfo proxy_info;
  configurator()->GetProxyConfig().proxy_rules().Apply(kHttpUrl, &proxy_info);

  std::vector<net::ProxyServer> proxies_without_direct;
  for (const net::ProxyServer& proxy : proxy_info.proxy_list().GetAll())
    if (proxy.is_valid() && !proxy.is_direct())
      proxies_without_direct.push_back(proxy);
  return proxies_without_direct;
}

}  // namespace data_reduction_proxy
