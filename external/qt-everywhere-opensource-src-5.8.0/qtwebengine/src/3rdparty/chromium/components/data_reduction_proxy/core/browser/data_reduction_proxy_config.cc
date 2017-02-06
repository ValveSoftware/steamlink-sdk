// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using base::FieldTrialList;

namespace {

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  IP_CHANGED = 0,         // The client IP address changed.
  DISABLED_ON_VPN = 1,    // [Deprecated] Proxy is disabled because a VPN is
                          // running.
  CHANGE_EVENT_COUNT = 2  // This must always be last.
};

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

// Key of the UMA DataReductionProxy.ProbeURLNetError histogram.
const char kUMAProxyProbeURLNetError[] = "DataReductionProxy.ProbeURLNetError";

// Key of the UMA DataReductionProxy.SecureProxyCheck.Latency histogram.
const char kUMAProxySecureProxyCheckLatency[] =
    "DataReductionProxy.SecureProxyCheck.Latency";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}

// Returns a descriptive name corresponding to |connection_type|.
const char* GetNameForConnectionType(
    net::NetworkChangeNotifier::ConnectionType connection_type) {
  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return "Unknown";
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
  }
  NOTREACHED();
  return "";
}

// Returns an enumerated histogram that should be used to record the given
// statistic. |max_limit| is the maximum value that can be stored in the
// histogram. Number of buckets in the enumerated histogram are one more than
// |max_limit|.
base::HistogramBase* GetEnumeratedHistogram(
    base::StringPiece prefix,
    net::NetworkChangeNotifier::ConnectionType type,
    int32_t max_limit) {
  DCHECK_GT(max_limit, 0);

  base::StringPiece name_for_connection_type(GetNameForConnectionType(type));
  std::string histogram_name;
  histogram_name.reserve(prefix.size() + name_for_connection_type.size());
  histogram_name.append(prefix.data(), prefix.size());
  histogram_name.append(name_for_connection_type.data(),
                        name_for_connection_type.size());

  return base::Histogram::FactoryGet(
      histogram_name, 0, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// Following UMA is plotted to measure how frequently Lo-Fi state changes.
// Too frequent changes are undesirable.
void RecordAutoLoFiRequestHeaderStateChange(bool previous_header_low,
                                            bool current_header_low) {
  // Auto Lo-Fi request header state changes.
  // Possible Lo-Fi header directives are empty ("") and low ("q=low").
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiRequestHeaderState {
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY = 0,
    AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW = 1,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY = 2,
    AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW = 3,
    AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY
  };

  AutoLoFiRequestHeaderState state;
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();

  if (!previous_header_low) {
    if (current_header_low)
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_LOW;
    else
      state = AUTO_LOFI_REQUEST_HEADER_STATE_EMPTY_TO_EMPTY;
  } else {
    if (current_header_low) {
      // Low to low in useful in checking how many consecutive page loads
      // are done with Lo-Fi enabled.
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_LOW;
    } else {
      state = AUTO_LOFI_REQUEST_HEADER_STATE_LOW_TO_EMPTY;
    }
  }

  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Unknown", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Ethernet", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.WiFi", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.2G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.3G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.4G", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.None", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Bluetooth", state,
          AUTO_LOFI_REQUEST_HEADER_STATE_INDEX_BOUNDARY);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

namespace data_reduction_proxy {

// Checks if the secure proxy is allowed by the carrier by sending a probe.
class SecureProxyChecker : public net::URLFetcherDelegate {
 public:
  SecureProxyChecker(const scoped_refptr<net::URLRequestContextGetter>&
                         url_request_context_getter)
      : url_request_context_getter_(url_request_context_getter) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, fetcher_.get());
    net::URLRequestStatus status = source->GetStatus();

    std::string response;
    source->GetResponseAsString(&response);

    base::TimeDelta secure_proxy_check_latency =
        base::Time::Now() - secure_proxy_check_start_time_;
    if (secure_proxy_check_latency >= base::TimeDelta()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(kUMAProxySecureProxyCheckLatency,
                                 secure_proxy_check_latency);
    }

    fetcher_callback_.Run(response, status, source->GetResponseCode());
  }

  void CheckIfSecureProxyIsAllowed(const GURL& secure_proxy_check_url,
                                   FetcherResponseCallback fetcher_callback) {
    fetcher_ = net::URLFetcher::Create(secure_proxy_check_url,
                                       net::URLFetcher::GET, this);
    fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY);
    fetcher_->SetRequestContext(url_request_context_getter_.get());
    // Configure max retries to be at most kMaxRetries times for 5xx errors.
    static const int kMaxRetries = 5;
    fetcher_->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
    // The secure proxy check should not be redirected. Since the secure proxy
    // check will inevitably fail if it gets redirected somewhere else (e.g. by
    // a captive portal), short circuit that by giving up on the secure proxy
    // check if it gets redirected.
    fetcher_->SetStopOnRedirect(true);

    fetcher_callback_ = fetcher_callback;

    secure_proxy_check_start_time_ = base::Time::Now();
    fetcher_->Start();
  }

  ~SecureProxyChecker() override {}

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The URLFetcher being used for the secure proxy check.
  std::unique_ptr<net::URLFetcher> fetcher_;
  FetcherResponseCallback fetcher_callback_;

  // Used to determine the latency in performing the Data Reduction Proxy secure
  // proxy check.
  base::Time secure_proxy_check_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SecureProxyChecker);
};

DataReductionProxyConfig::DataReductionProxyConfig(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : secure_proxy_allowed_(params::ShouldUseSecureProxyByDefault()),
      unreachable_(false),
      enabled_by_user_(false),
      config_values_(std::move(config_values)),
      io_task_runner_(io_task_runner),
      net_log_(net_log),
      configurator_(configurator),
      event_creator_(event_creator),
      lofi_effective_connection_type_threshold_(
          net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      auto_lofi_hysteresis_(base::TimeDelta::Max()),
      network_prohibitively_slow_(false),
      connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      lofi_off_(false),
      network_quality_at_last_query_(NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN),
      previous_state_lofi_on_(false),
      weak_factory_(this) {
  DCHECK(io_task_runner_);
  DCHECK(configurator);
  DCHECK(event_creator);

  if (params::IsLoFiDisabledViaFlags())
    SetLoFiModeOff();
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void DataReductionProxyConfig::InitializeOnIOThread(const scoped_refptr<
    net::URLRequestContextGetter>& url_request_context_getter) {
  secure_proxy_checker_.reset(
      new SecureProxyChecker(url_request_context_getter));

  if (!config_values_->allowed())
    return;

  PopulateAutoLoFiParams();
  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  // Record accuracy at 3 different intervals. The values used here must remain
  // in sync with the suffixes specified in
  // tools/metrics/histograms/histograms.xml.
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(15));
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(30));
  lofi_accuracy_recording_intervals_.push_back(
      base::TimeDelta::FromSeconds(60));
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateConfigurator(enabled_by_user_, secure_proxy_allowed_);
}

bool DataReductionProxyConfig::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);
  return IsDataReductionProxy(request->proxy_server(), proxy_info);
}

bool DataReductionProxyConfig::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::vector<net::ProxyServer>& proxy_list =
      config_values_->proxies_for_http();
  auto proxy_it =
      std::find_if(proxy_list.begin(), proxy_list.end(),
                   [&host_port_pair](const net::ProxyServer& proxy) {
                     return proxy.is_valid() &&
                            proxy.host_port_pair().Equals(host_port_pair);
                   });

  if (proxy_it != proxy_list.end()) {
    if (proxy_info) {
      proxy_info->proxy_servers =
          std::vector<net::ProxyServer>(proxy_it, proxy_list.end());
      proxy_info->proxy_index =
          static_cast<size_t>(proxy_it - proxy_list.begin());
    }
    return true;
  }
  return false;
}

bool DataReductionProxyConfig::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request.context());
  DCHECK(request.context()->proxy_service());
  net::ProxyInfo result;
  data_reduction_proxy_config.proxy_rules().Apply(
      request.url(), &result);
  if (!result.proxy_server().is_valid())
    return true;
  if (result.proxy_server().is_direct())
    return true;
  return !IsDataReductionProxy(result.proxy_server().host_port_pair(), NULL);
}

bool DataReductionProxyConfig::AreDataReductionProxiesBypassed(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (request.context() != NULL &&
      request.context()->proxy_service() != NULL) {
    return AreProxiesBypassed(
        request.context()->proxy_service()->proxy_retry_info(),
        data_reduction_proxy_config.proxy_rules(),
        request.url().SchemeIsCryptographic(), min_retry_delay);
  }

  return false;
}

bool DataReductionProxyConfig::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  if (is_https)
    return false;

  const net::ProxyList* proxies =
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool bypassed = false;

  for (const net::ProxyServer& proxy : proxies->GetAll()) {
    if (!proxy.is_valid() || proxy.is_direct())
      continue;

    base::TimeDelta delay;
    if (IsDataReductionProxy(proxy.host_port_pair(), NULL)) {
      if (!IsProxyBypassed(retry_map, proxy, &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial() ||
         params::IsLoFiSlowConnectionsOnlyViaFlags());

  if (!network_quality_estimator)
    return false;

  // True iff network type changed since the last call to
  // IsNetworkQualityProhibitivelySlow(). This call happens only on main frame
  // requests.
  bool network_type_changed = false;
  if (net::NetworkChangeNotifier::GetConnectionType() != connection_type_) {
    connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
    network_type_changed = true;
  }

  const net::NetworkQualityEstimator::EffectiveConnectionType
      effective_connection_type =
          network_quality_estimator->GetEffectiveConnectionType();

  const bool is_network_quality_available =
      effective_connection_type !=
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // True only if the network is currently estimated to be slower than the
  // defined thresholds.
  const bool is_network_currently_slow =
      is_network_quality_available &&
      IsEffectiveConnectionTypeSlowerThanThreshold(effective_connection_type);

  if (is_network_quality_available) {
    network_quality_at_last_query_ =
        is_network_currently_slow ? NETWORK_QUALITY_AT_LAST_QUERY_SLOW
                                  : NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW;

    if ((params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial()) &&
        !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
      // Post tasks to record accuracy of network quality prediction at
      // different intervals.
      for (const base::TimeDelta& measuring_delay :
           GetLofiAccuracyRecordingIntervals()) {
        io_task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&DataReductionProxyConfig::RecordAutoLoFiAccuracyRate,
                       weak_factory_.GetWeakPtr(), network_quality_estimator,
                       measuring_delay),
            measuring_delay);
      }
    }
  }

  // Return the cached entry if the last update was within the hysteresis
  // duration and if the connection type has not changed.
  if (!network_type_changed && !network_quality_last_checked_.is_null() &&
      GetTicksNow() - network_quality_last_checked_ <= auto_lofi_hysteresis_) {
    return network_prohibitively_slow_;
  }

  network_quality_last_checked_ = GetTicksNow();

  if (!is_network_quality_available)
    return false;

  network_prohibitively_slow_ = is_network_currently_slow;
  return network_prohibitively_slow_;
}

void DataReductionProxyConfig::PopulateAutoLoFiParams() {
  std::string field_trial = params::GetLoFiFieldTrialName();

    // Default parameters to use.
  const net::NetworkQualityEstimator::EffectiveConnectionType
      default_effective_connection_type_threshold =
          net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  const base::TimeDelta default_hysterisis = base::TimeDelta::FromSeconds(60);

  if (params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    // Use the default parameters.
    lofi_effective_connection_type_threshold_ =
        default_effective_connection_type_threshold;
    auto_lofi_hysteresis_ = default_hysterisis;
    field_trial = params::GetLoFiFlagFieldTrialName();
  }

  if (!params::IsIncludedInLoFiControlFieldTrial() &&
      !params::IsIncludedInLoFiEnabledFieldTrial() &&
      !params::IsLoFiSlowConnectionsOnlyViaFlags()) {
    return;
  }

  std::string variation_value = variations::GetVariationParamValue(
      field_trial, "effective_connection_type");
  if (!variation_value.empty()) {
    lofi_effective_connection_type_threshold_ =
        net::NetworkQualityEstimator::GetEffectiveConnectionTypeForName(
            variation_value);
  } else {
    // Use the default parameters.
    lofi_effective_connection_type_threshold_ =
        default_effective_connection_type_threshold;
  }

  uint32_t auto_lofi_hysteresis_period_seconds;
  variation_value = variations::GetVariationParamValue(
      field_trial, "hysteresis_period_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value,
                         &auto_lofi_hysteresis_period_seconds)) {
    auto_lofi_hysteresis_ =
        base::TimeDelta::FromSeconds(auto_lofi_hysteresis_period_seconds);
  } else {
    // Use the default parameters.
    auto_lofi_hysteresis_ = default_hysterisis;
  }
  DCHECK_GE(auto_lofi_hysteresis_, base::TimeDelta());
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyRetryInfoMap::const_iterator found =
      retry_map.find(proxy_server.ToURI());

  if (found == retry_map.end() || found->second.bad_until < GetTicksNow()) {
    return false;
  }

  if (retry_delay)
     *retry_delay = found->second.current_delay;

  return true;
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Data Reduction Proxy configurations are always TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      IsDataReductionProxy(http_proxy_list->Get().host_port_pair(), NULL)) {
    return true;
  }

  return false;
}

// Returns true if the Data Reduction Proxy configuration may be used.
bool DataReductionProxyConfig::allowed() const {
  return config_values_->allowed();
}

// Returns true if the Data Reduction Proxy promo may be shown. This is not
// tied to whether the Data Reduction Proxy is enabled.
bool DataReductionProxyConfig::promo_allowed() const {
  return config_values_->promo_allowed();
}

void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
  UpdateConfigurator(enabled_by_user_, secure_proxy_allowed_);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled) {
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        config_values_->secure_proxy_check_url(),
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::UpdateConfigurator(bool enabled,
                                                  bool secure_proxy_allowed) {
  DCHECK(configurator_);
  const std::vector<net::ProxyServer>& proxies_for_http =
      config_values_->proxies_for_http();
  if (enabled && !config_values_->holdback() && !proxies_for_http.empty()) {
    configurator_->Enable(!secure_proxy_allowed, proxies_for_http);
  } else {
    configurator_->Disable();
  }
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response,
    const net::URLRequestStatus& status,
    int http_response_code) {
  bool success_response =
      base::StartsWith(response, "OK", base::CompareCase::SENSITIVE);
  if (event_creator_)
    event_creator_->EndSecureProxyCheck(bound_net_log_, status.error(),
                                        http_response_code, success_response);

  if (!status.is_success()) {
    if (status.error() == net::ERR_INTERNET_DISCONNECTED) {
      RecordSecureProxyCheckFetchResult(INTERNET_DISCONNECTED);
      return;
    }
    // TODO(bengr): Remove once we understand the reasons secure proxy checks
    // are failing. Secure proxy check errors are either due to fetcher-level
    // errors or modified responses. This only tracks the former.
    UMA_HISTOGRAM_SPARSE_SLOWLY(kUMAProxyProbeURLNetError,
                                std::abs(status.error()));
  }

  if (success_response) {
    DVLOG(1) << "The data reduction proxy is unrestricted.";

    if (enabled_by_user_) {
      if (!secure_proxy_allowed_) {
        secure_proxy_allowed_ = true;
        // The user enabled the proxy, but sometime previously in the session,
        // the network operator had blocked the secure proxy check and
        // restricted the user. The current network doesn't block the secure
        // proxy check, so don't restrict the proxy configurations.
        ReloadConfig();
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ENABLED);
      } else {
        RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
      }
    }
    secure_proxy_allowed_ = true;
    return;
  }
  DVLOG(1) << "The data reduction proxy is restricted to the configured "
           << "fallback proxy.";
  if (enabled_by_user_) {
    if (secure_proxy_allowed_) {
      // Restrict the proxy.
      secure_proxy_allowed_ = false;
      ReloadConfig();
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordSecureProxyCheckFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  secure_proxy_allowed_ = false;
}

void DataReductionProxyConfig::OnIPAddressChanged() {
  if (enabled_by_user_) {
    DCHECK(config_values_->allowed());
    RecordNetworkChangeEvent(IP_CHANGED);

    // Reset |network_quality_at_last_query_| to prevent recording of network
    // quality prediction accuracy if there was a change in the IP address.
    network_quality_at_last_query_ = NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN;

    bool should_use_secure_proxy = params::ShouldUseSecureProxyByDefault();
    if (!should_use_secure_proxy && secure_proxy_allowed_) {
      secure_proxy_allowed_ = false;
      RecordSecureProxyCheckFetchResult(PROXY_DISABLED_BEFORE_CHECK);
      ReloadConfig();
    }

    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        config_values_->secure_proxy_check_url(),
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::AddDefaultProxyBypassRules() {
  // localhost
  DCHECK(configurator_);
  configurator_->AddHostPatternToBypass("<local>");
  // RFC6890 loopback addresses.
  // TODO(tbansal): Remove this once crbug/446705 is fixed.
  configurator_->AddHostPatternToBypass("127.0.0.0/8");

  // RFC6890 current network (only valid as source address).
  configurator_->AddHostPatternToBypass("0.0.0.0/8");

  // RFC1918 private addresses.
  configurator_->AddHostPatternToBypass("10.0.0.0/8");
  configurator_->AddHostPatternToBypass("172.16.0.0/12");
  configurator_->AddHostPatternToBypass("192.168.0.0/16");

  // RFC3513 unspecified address.
  configurator_->AddHostPatternToBypass("::/128");

  // RFC4193 private addresses.
  configurator_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  configurator_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  configurator_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
}

void DataReductionProxyConfig::RecordSecureProxyCheckFetchResult(
    SecureProxyCheckFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyProbeURL, result,
                            SECURE_PROXY_CHECK_FETCH_RESULT_COUNT);
}

void DataReductionProxyConfig::SecureProxyCheck(
    const GURL& secure_proxy_check_url,
    FetcherResponseCallback fetcher_callback) {
  bound_net_log_ = net::BoundNetLog::Make(
      net_log_, net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  if (event_creator_) {
    event_creator_->BeginSecureProxyCheck(
        bound_net_log_, config_values_->secure_proxy_check_url());
  }

  secure_proxy_checker_->CheckIfSecureProxyIsAllowed(secure_proxy_check_url,
                                                     fetcher_callback);
}

void DataReductionProxyConfig::SetLoFiModeOff() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lofi_off_ = true;
}

void DataReductionProxyConfig::RecordAutoLoFiAccuracyRate(
    const net::NetworkQualityEstimator* network_quality_estimator,
    const base::TimeDelta& measuring_duration) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_quality_estimator);
  DCHECK((params::IsIncludedInLoFiEnabledFieldTrial() ||
          params::IsIncludedInLoFiControlFieldTrial()) &&
         !params::IsLoFiSlowConnectionsOnlyViaFlags());
  DCHECK_EQ(0, measuring_duration.InMilliseconds() % 1000);
  DCHECK(
      ContainsValue(GetLofiAccuracyRecordingIntervals(), measuring_duration));

  if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN)
    return;

  const base::TimeTicks now = GetTicksNow();

  // Return if the time since |last_query_| is less than |measuring_duration|.
  // This may happen if another main frame request started during last
  // |measuring_duration|.
  if (now - last_query_ < measuring_duration)
    return;

  // Return if the time since |last_query_| is off by a factor of 2.
  if (now - last_query_ > 2 * measuring_duration)
    return;

  const net::NetworkQualityEstimator::EffectiveConnectionType
      recent_effective_connection_type =
          network_quality_estimator->GetRecentEffectiveConnectionType(
              last_query_);
  if (recent_effective_connection_type ==
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return;
  }

  // Values of Auto Lo-Fi accuracy.
  // This enum must remain synchronized with the enum of the same name in
  // metrics/histograms/histograms.xml.
  enum AutoLoFiAccuracy {
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW = 0,
    AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW = 1,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW = 2,
    AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW = 3,
    AUTO_LOFI_ACCURACY_INDEX_BOUNDARY
  };

  const bool should_have_used_lofi =
      IsEffectiveConnectionTypeSlowerThanThreshold(
          recent_effective_connection_type);

  AutoLoFiAccuracy accuracy = AUTO_LOFI_ACCURACY_INDEX_BOUNDARY;

  if (should_have_used_lofi) {
    if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_SLOW;
    } else if (network_quality_at_last_query_ ==
               NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_SLOW;
    } else {
      NOTREACHED();
    }
  } else {
    if (network_quality_at_last_query_ == NETWORK_QUALITY_AT_LAST_QUERY_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_SLOW_ACTUAL_NOT_SLOW;
    } else if (network_quality_at_last_query_ ==
               NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW) {
      accuracy = AUTO_LOFI_ACCURACY_ESTIMATED_NOT_SLOW_ACTUAL_NOT_SLOW;
    } else {
      NOTREACHED();
    }
  }

  base::HistogramBase* accuracy_histogram = GetEnumeratedHistogram(
      base::StringPrintf("DataReductionProxy.LoFi.Accuracy.%d.",
                         static_cast<int>(measuring_duration.InSeconds())),
      connection_type_, AUTO_LOFI_ACCURACY_INDEX_BOUNDARY - 1);

  accuracy_histogram->Add(accuracy);
}

bool DataReductionProxyConfig::IsEffectiveConnectionTypeSlowerThanThreshold(
    net::NetworkQualityEstimator::EffectiveConnectionType
        effective_connection_type) const {
  return effective_connection_type >=
             net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_OFFLINE &&
         effective_connection_type <= lofi_effective_connection_type_threshold_;
}

bool DataReductionProxyConfig::ShouldEnableLoFiMode(
    const net::URLRequest& request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((request.load_flags() & net::LOAD_MAIN_FRAME) != 0);
  DCHECK(!request.url().SchemeIsCryptographic());

  net::NetworkQualityEstimator* network_quality_estimator;
  network_quality_estimator =
      request.context() ? request.context()->network_quality_estimator()
                        : nullptr;

  bool enable_lofi = ShouldEnableLoFiModeInternal(network_quality_estimator);

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      params::IsIncludedInLoFiEnabledFieldTrial()) {
    RecordAutoLoFiRequestHeaderStateChange(previous_state_lofi_on_,
                                           enable_lofi);
    previous_state_lofi_on_ = enable_lofi;
  }

  return enable_lofi;
}

bool DataReductionProxyConfig::enabled_by_user_and_reachable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return enabled_by_user_ && !unreachable_;
}

bool DataReductionProxyConfig::ShouldEnableLoFiModeInternal(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(thread_checker_.CalledOnValidThread());

  last_query_ = GetTicksNow();
  network_quality_at_last_query_ = NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN;

  // If Lo-Fi has been turned off, its status can't change.
  if (lofi_off_)
    return false;

  if (params::IsLoFiAlwaysOnViaFlags())
    return true;

  if (params::IsLoFiCellularOnlyViaFlags()) {
    return net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
  }

  if (params::IsLoFiSlowConnectionsOnlyViaFlags() ||
      params::IsIncludedInLoFiEnabledFieldTrial() ||
      params::IsIncludedInLoFiControlFieldTrial()) {
    return IsNetworkQualityProhibitivelySlow(network_quality_estimator);
  }

  // If Lo-Fi is not enabled through command line and the user is not in
  // Lo-Fi field trials, set Lo-Fi to off.
  lofi_off_ = true;
  return false;
}

void DataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  net::GetNetworkList(interfaces, policy);
}

const std::vector<base::TimeDelta>&
DataReductionProxyConfig::GetLofiAccuracyRecordingIntervals() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return lofi_accuracy_recording_intervals_;
}

base::TimeTicks DataReductionProxyConfig::GetTicksNow() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::TimeTicks::Now();
}

net::ProxyConfig DataReductionProxyConfig::ProxyConfigIgnoringHoldback() const {
  std::vector<net::ProxyServer> proxies_for_http =
      config_values_->proxies_for_http();
  if (!enabled_by_user_ || proxies_for_http.empty())
    return net::ProxyConfig::CreateDirect();
  return configurator_->CreateProxyConfig(!secure_proxy_allowed_,
                                          proxies_for_http);
}

}  // namespace data_reduction_proxy
