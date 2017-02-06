// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/build_time.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_url_constants.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace variations {

namespace {

// TODO(mad): To be removed when we stop updating the NetworkTimeTracker.
// For the HTTP date headers, the resolution of the server time is 1 second.
const int64_t kServerTimeResolutionMs = 1000;

// Maximum age permitted for a variations seed, in days.
const int kMaxVariationsSeedAgeDays = 30;

// Wrapper around channel checking, used to enable channel mocking for
// testing. If the current browser channel is not UNKNOWN, this will return
// that channel value. Otherwise, if the fake channel flag is provided, this
// will return the fake channel. Failing that, this will return the UNKNOWN
// channel.
variations::Study_Channel GetChannelForVariations(
    version_info::Channel product_channel) {
  switch (product_channel) {
    case version_info::Channel::CANARY:
      return variations::Study_Channel_CANARY;
    case version_info::Channel::DEV:
      return variations::Study_Channel_DEV;
    case version_info::Channel::BETA:
      return variations::Study_Channel_BETA;
    case version_info::Channel::STABLE:
      return variations::Study_Channel_STABLE;
    case version_info::Channel::UNKNOWN:
      break;
  }
  const std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (forced_channel == "stable")
    return variations::Study_Channel_STABLE;
  if (forced_channel == "beta")
    return variations::Study_Channel_BETA;
  if (forced_channel == "dev")
    return variations::Study_Channel_DEV;
  if (forced_channel == "canary")
    return variations::Study_Channel_CANARY;
  DVLOG(1) << "Invalid channel provided: " << forced_channel;
  return variations::Study_Channel_UNKNOWN;
}

// Returns a string that will be used for the value of the 'osname' URL param
// to the variations server.
std::string GetPlatformString() {
#if defined(OS_WIN)
  return "win";
#elif defined(OS_IOS)
  return "ios";
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_ANDROID)
  return "android";
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return "linux";
#else
#error Unknown platform
#endif
}

// Gets the restrict parameter from either the client or |policy_pref_service|.
std::string GetRestrictParameterPref(VariationsServiceClient* client,
                                     PrefService* policy_pref_service) {
  std::string parameter;
  if (client->OverridesRestrictParameter(&parameter) || !policy_pref_service)
    return parameter;

  return policy_pref_service->GetString(prefs::kVariationsRestrictParameter);
}

enum ResourceRequestsAllowedState {
  RESOURCE_REQUESTS_ALLOWED,
  RESOURCE_REQUESTS_NOT_ALLOWED,
  RESOURCE_REQUESTS_ALLOWED_NOTIFIED,
  RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED,
  RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN,
  RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED,
  RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE,
};

// Records UMA histogram with the current resource requests allowed state.
void RecordRequestsAllowedHistogram(ResourceRequestsAllowedState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.ResourceRequestsAllowed", state,
                            RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE);
}

enum VariationsSeedExpiry {
  VARIATIONS_SEED_EXPIRY_NOT_EXPIRED,
  VARIATIONS_SEED_EXPIRY_FETCH_TIME_MISSING,
  VARIATIONS_SEED_EXPIRY_EXPIRED,
  VARIATIONS_SEED_EXPIRY_ENUM_SIZE,
};

// Records UMA histogram with the result of the variations seed expiry check.
void RecordCreateTrialsSeedExpiry(VariationsSeedExpiry expiry_check_result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.CreateTrials.SeedExpiry",
                            expiry_check_result,
                            VARIATIONS_SEED_EXPIRY_ENUM_SIZE);
}

// Converts ResourceRequestAllowedNotifier::State to the corresponding
// ResourceRequestsAllowedState value.
ResourceRequestsAllowedState ResourceRequestStateToHistogramValue(
    web_resource::ResourceRequestAllowedNotifier::State state) {
  using web_resource::ResourceRequestAllowedNotifier;
  switch (state) {
    case ResourceRequestAllowedNotifier::DISALLOWED_EULA_NOT_ACCEPTED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED;
    case ResourceRequestAllowedNotifier::DISALLOWED_NETWORK_DOWN:
      return RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN;
    case ResourceRequestAllowedNotifier::DISALLOWED_COMMAND_LINE_DISABLED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED;
    case ResourceRequestAllowedNotifier::ALLOWED:
      return RESOURCE_REQUESTS_ALLOWED;
  }
  NOTREACHED();
  return RESOURCE_REQUESTS_NOT_ALLOWED;
}


// Gets current form factor and converts it from enum DeviceFormFactor to enum
// Study_FormFactor.
variations::Study_FormFactor GetCurrentFormFactor() {
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return variations::Study_FormFactor_PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return variations::Study_FormFactor_TABLET;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return variations::Study_FormFactor_DESKTOP;
  }
  NOTREACHED();
  return variations::Study_FormFactor_DESKTOP;
}

// Gets the hardware class and returns it as a string. This returns an empty
// string if the client is not ChromeOS.
std::string GetHardwareClass() {
#if defined(OS_CHROMEOS)
  return base::SysInfo::GetLsbReleaseBoard();
#endif  // OS_CHROMEOS
  return std::string();
}

// Returns the date that should be used by the VariationsSeedProcessor to do
// expiry and start date checks.
base::Time GetReferenceDateForExpiryChecks(PrefService* local_state) {
  const int64_t date_value = local_state->GetInt64(prefs::kVariationsSeedDate);
  const base::Time seed_date = base::Time::FromInternalValue(date_value);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;
  return reference_date;
}

// Returns the header value for |name| from |headers| or an empty string if not
// set.
std::string GetHeaderValue(const net::HttpResponseHeaders* headers,
                           const base::StringPiece& name) {
  std::string value;
  headers->EnumerateHeader(nullptr, name, &value);
  return value;
}

// Returns the list of values for |name| from |headers|. If the header in not
// set, return an empty list.
std::vector<std::string> GetHeaderValuesList(
    const net::HttpResponseHeaders* headers,
    const base::StringPiece& name) {
  std::vector<std::string> values;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, name, &value)) {
    values.push_back(value);
  }
  return values;
}

// Looks for delta and gzip compression instance manipulation flags set by the
// server in |headers|. Checks the order of flags and presence of unknown
// instance manipulations. If successful, |is_delta_compressed| and
// |is_gzip_compressed| contain compression flags and true is returned.
bool GetInstanceManipulations(const net::HttpResponseHeaders* headers,
                              bool* is_delta_compressed,
                              bool* is_gzip_compressed) {
  std::vector<std::string> ims = GetHeaderValuesList(headers, "IM");
  const auto delta_im = std::find(ims.begin(), ims.end(), "x-bm");
  const auto gzip_im = std::find(ims.begin(), ims.end(), "gzip");
  *is_delta_compressed = delta_im != ims.end();
  *is_gzip_compressed = gzip_im != ims.end();

  // The IM field should not have anything but x-bm and gzip.
  size_t im_count = (*is_delta_compressed ? 1 : 0) +
      (*is_gzip_compressed ? 1 : 0);
  if (im_count != ims.size()) {
    DVLOG(1) << "Unrecognized instance manipulations in "
             << base::JoinString(ims, ",")
             << "; only x-bm and gzip are supported";
    return false;
  }

  // The IM field defines order in which instance manipulations were applied.
  // The client requests and supports gzip-compressed delta-compressed seeds,
  // but not vice versa.
  if (*is_delta_compressed && *is_gzip_compressed && delta_im > gzip_im) {
    DVLOG(1) << "Unsupported instance manipulations order: "
             << "requested x-bm,gzip but received gzip,x-bm";
    return false;
  }

  return true;
}

}  // namespace

VariationsService::VariationsService(
    std::unique_ptr<VariationsServiceClient> client,
    std::unique_ptr<web_resource::ResourceRequestAllowedNotifier> notifier,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager,
    const UIStringOverrider& ui_string_overrider)
    : client_(std::move(client)),
      ui_string_overrider_(ui_string_overrider),
      local_state_(local_state),
      state_manager_(state_manager),
      policy_pref_service_(local_state),
      seed_store_(local_state),
      create_trials_from_seed_called_(false),
      initial_request_completed_(false),
      disable_deltas_for_next_request_(false),
      resource_request_allowed_notifier_(std::move(notifier)),
      request_count_(0),
      weak_ptr_factory_(this) {
  DCHECK(client_.get());
  DCHECK(resource_request_allowed_notifier_.get());

  resource_request_allowed_notifier_->Init(this);
}

VariationsService::~VariationsService() {
}

bool VariationsService::CreateTrialsFromSeed(base::FeatureList* feature_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!create_trials_from_seed_called_);

  create_trials_from_seed_called_ = true;

  variations::VariationsSeed seed;
  if (!LoadSeed(&seed))
    return false;

  const int64_t last_fetch_time_internal =
      local_state_->GetInt64(prefs::kVariationsLastFetchTime);
  const base::Time last_fetch_time =
      base::Time::FromInternalValue(last_fetch_time_internal);
  if (last_fetch_time.is_null()) {
    // If the last fetch time is missing and we have a seed, then this must be
    // the first run of Chrome. Store the current time as the last fetch time.
    RecordLastFetchTime();
    RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_FETCH_TIME_MISSING);
  } else {
    // Reject the seed if it is more than 30 days old.
    const base::TimeDelta seed_age = base::Time::Now() - last_fetch_time;
    if (seed_age.InDays() > kMaxVariationsSeedAgeDays) {
      RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_EXPIRED);
      return false;
    }
    RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_NOT_EXPIRED);
  }

  const base::Version current_version(version_info::GetVersionNumber());
  if (!current_version.IsValid())
    return false;

  variations::Study_Channel channel =
      GetChannelForVariations(client_->GetChannel());
  UMA_HISTOGRAM_SPARSE_SLOWLY("Variations.UserChannel", channel);

  const std::string latest_country =
      local_state_->GetString(prefs::kVariationsCountry);

  std::unique_ptr<const base::FieldTrial::EntropyProvider> low_entropy_provider(
      CreateLowEntropyProvider());
  // Note that passing |&ui_string_overrider_| via base::Unretained below is
  // safe because the callback is executed synchronously. It is not possible
  // to pass UIStringOverrider itself to VariationSeedProcesor as variations
  // components should not depends on //ui/base.
  variations::VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, client_->GetApplicationLocale(),
      GetReferenceDateForExpiryChecks(local_state_), current_version, channel,
      GetCurrentFormFactor(), GetHardwareClass(), latest_country,
      LoadPermanentConsistencyCountry(current_version, latest_country),
      base::Bind(&UIStringOverrider::OverrideUIString,
                 base::Unretained(&ui_string_overrider_)),
      low_entropy_provider.get(), feature_list);

  const base::Time now = base::Time::Now();

  // Log the "freshness" of the seed that was just used. The freshness is the
  // time between the last successful seed download and now.
  if (last_fetch_time_internal) {
    const base::TimeDelta delta =
        now - base::Time::FromInternalValue(last_fetch_time_internal);
    // Log the value in number of minutes.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", delta.InMinutes(),
        1, base::TimeDelta::FromDays(30).InMinutes(), 50);
  }

  return true;
}

void VariationsService::PerformPreMainMessageLoopStartup() {
  DCHECK(thread_checker_.CalledOnValidThread());

  StartRepeatedVariationsSeedFetch();
  client_->OnInitialStartup();
}

void VariationsService::StartRepeatedVariationsSeedFetch() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize the Variations server URL.
  variations_server_url_ =
      GetVariationsServerURL(policy_pref_service_, restrict_mode_);

  // Check that |CreateTrialsFromSeed| was called, which is necessary to
  // retrieve the serial number that will be sent to the server.
  DCHECK(create_trials_from_seed_called_);

  DCHECK(!request_scheduler_.get());
  // Note that the act of instantiating the scheduler will start the fetch, if
  // the scheduler deems appropriate.
  request_scheduler_.reset(VariationsRequestScheduler::Create(
      base::Bind(&VariationsService::FetchVariationsSeed,
                 weak_ptr_factory_.GetWeakPtr()),
      local_state_));
  request_scheduler_->Start();
}

void VariationsService::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void VariationsService::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void VariationsService::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // On mobile platforms, initialize the fetch scheduler when we receive the
  // first app foreground notification.
  if (!request_scheduler_)
    StartRepeatedVariationsSeedFetch();
  request_scheduler_->OnAppEnterForeground();
}

void VariationsService::SetRestrictMode(const std::string& restrict_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This should be called before the server URL has been computed.
  DCHECK(variations_server_url_.is_empty());
  restrict_mode_ = restrict_mode;
}

void VariationsService::SetCreateTrialsFromSeedCalledForTesting(bool called) {
  DCHECK(thread_checker_.CalledOnValidThread());
  create_trials_from_seed_called_ = called;
}

GURL VariationsService::GetVariationsServerURL(
    PrefService* policy_pref_service,
    const std::string& restrict_mode_override) {
  std::string server_url_string(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsServerURL));
  if (server_url_string.empty())
    server_url_string = kDefaultServerUrl;
  GURL server_url = GURL(server_url_string);

  const std::string restrict_param =
      !restrict_mode_override.empty()
          ? restrict_mode_override
          : GetRestrictParameterPref(client_.get(), policy_pref_service);
  if (!restrict_param.empty()) {
    server_url = net::AppendOrReplaceQueryParameter(server_url,
                                                    "restrict",
                                                    restrict_param);
  }

  server_url = net::AppendOrReplaceQueryParameter(server_url, "osname",
                                                  GetPlatformString());

  DCHECK(server_url.is_valid());
  return server_url;
}

// static
std::string VariationsService::GetDefaultVariationsServerURLForTesting() {
  return kDefaultServerUrl;
}

// static
void VariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  VariationsSeedStore::RegisterPrefs(registry);
  registry->RegisterInt64Pref(prefs::kVariationsLastFetchTime, 0);
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
  // This preference keeps track of the country code used to filter
  // permanent-consistency studies.
  registry->RegisterListPref(prefs::kVariationsPermanentConsistencyCountry);
}

// static
void VariationsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
}

// static
std::unique_ptr<VariationsService> VariationsService::Create(
    std::unique_ptr<VariationsServiceClient> client,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager,
    const char* disable_network_switch,
    const UIStringOverrider& ui_string_overrider) {
  std::unique_ptr<VariationsService> result;
#if !defined(GOOGLE_CHROME_BUILD)
  // Unless the URL was provided, unsupported builds should return NULL to
  // indicate that the service should not be used.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVariationsServerURL)) {
    DVLOG(1) << "Not creating VariationsService in unofficial build without --"
             << switches::kVariationsServerURL << " specified.";
    return result;
  }
#endif
  result.reset(new VariationsService(
      std::move(client),
      base::WrapUnique(new web_resource::ResourceRequestAllowedNotifier(
          local_state, disable_network_switch)),
      local_state, state_manager, ui_string_overrider));
  return result;
}

// static
std::unique_ptr<VariationsService> VariationsService::CreateForTesting(
    std::unique_ptr<VariationsServiceClient> client,
    PrefService* local_state) {
  return base::WrapUnique(new VariationsService(
      std::move(client),
      base::WrapUnique(new web_resource::ResourceRequestAllowedNotifier(
          local_state, nullptr)),
      local_state, nullptr, UIStringOverrider()));
}

void VariationsService::DoActualFetch() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!pending_seed_request_);

  pending_seed_request_ = net::URLFetcher::Create(0, variations_server_url_,
                                                  net::URLFetcher::GET, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      pending_seed_request_.get(),
      data_use_measurement::DataUseUserData::VARIATIONS);
  pending_seed_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
  pending_seed_request_->SetRequestContext(client_->GetURLRequestContext());
  bool enable_deltas = false;
  if (!seed_store_.variations_serial_number().empty() &&
      !disable_deltas_for_next_request_) {
    // If the current seed includes a country code, deltas are not supported (as
    // the serial number doesn't take into account the country code). The server
    // will update us with a seed that doesn't include a country code which will
    // enable deltas to work.
    // TODO(asvitkine): Remove the check in M50+ when the percentage of clients
    // that have an old seed with a country code becomes miniscule.
    if (!seed_store_.seed_has_country_code()) {
      // Tell the server that delta-compressed seeds are supported.
      enable_deltas = true;
    }
    // Get the seed only if its serial number doesn't match what we have.
    pending_seed_request_->AddExtraRequestHeader(
        "If-None-Match:" + seed_store_.variations_serial_number());
  }
  // Tell the server that delta-compressed and gzipped seeds are supported.
  const char* supported_im = enable_deltas ? "A-IM:x-bm,gzip" : "A-IM:gzip";
  pending_seed_request_->AddExtraRequestHeader(supported_im);

  pending_seed_request_->Start();

  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_fetch;
  // Record a time delta of 0 (default value) if there was no previous fetch.
  if (!last_request_started_time_.is_null())
    time_since_last_fetch = now - last_request_started_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.TimeSinceLastFetchAttempt",
                              time_since_last_fetch.InMinutes(), 0,
                              base::TimeDelta::FromDays(7).InMinutes(), 50);
  UMA_HISTOGRAM_COUNTS_100("Variations.RequestCount", request_count_);
  ++request_count_;
  last_request_started_time_ = now;
  disable_deltas_for_next_request_ = false;
}

bool VariationsService::StoreSeed(const std::string& seed_data,
                                  const std::string& seed_signature,
                                  const std::string& country_code,
                                  const base::Time& date_fetched,
                                  bool is_delta_compressed,
                                  bool is_gzip_compressed) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<variations::VariationsSeed> seed(
      new variations::VariationsSeed);
  if (!seed_store_.StoreSeedData(seed_data, seed_signature, country_code,
                                 date_fetched, is_delta_compressed,
                                 is_gzip_compressed, seed.get())) {
    return false;
  }
  RecordLastFetchTime();

  // Perform seed simulation only if |state_manager_| is not-NULL. The state
  // manager may be NULL for some unit tests.
  if (!state_manager_)
    return true;

  base::PostTaskAndReplyWithResult(
      client_->GetBlockingPool(), FROM_HERE,
      client_->GetVersionForSimulationCallback(),
      base::Bind(&VariationsService::PerformSimulationWithVersion,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&seed)));
  return true;
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
VariationsService::CreateLowEntropyProvider() {
  return state_manager_->CreateLowEntropyProvider();
}

bool VariationsService::LoadSeed(VariationsSeed* seed) {
  return seed_store_.LoadSeed(seed);
}

void VariationsService::FetchVariationsSeed() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const web_resource::ResourceRequestAllowedNotifier::State state =
      resource_request_allowed_notifier_->GetResourceRequestsAllowedState();
  RecordRequestsAllowedHistogram(ResourceRequestStateToHistogramValue(state));
  if (state != web_resource::ResourceRequestAllowedNotifier::ALLOWED) {
    DVLOG(1) << "Resource requests were not allowed. Waiting for notification.";
    return;
  }

  DoActualFetch();
}

void VariationsService::NotifyObservers(
    const variations::VariationsSeedSimulator::Result& result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result.kill_critical_group_change_count > 0) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnExperimentChangesDetected(Observer::CRITICAL));
  } else if (result.kill_best_effort_group_change_count > 0) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnExperimentChangesDetected(Observer::BEST_EFFORT));
  }
}

void VariationsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(pending_seed_request_.get(), source);

  const bool is_first_request = !initial_request_completed_;
  initial_request_completed_ = true;

  // The fetcher will be deleted when the request is handled.
  std::unique_ptr<const net::URLFetcher> request(
      pending_seed_request_.release());
  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Variations.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Variations server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    // It's common for the very first fetch attempt to fail (e.g. the network
    // may not yet be available). In such a case, try again soon, rather than
    // waiting the full time interval.
    if (is_first_request)
      request_scheduler_->ScheduleFetchShortly();
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Variations.SeedFetchResponseCode",
                              response_code);

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;

  base::Time response_date;
  if (response_code == net::HTTP_OK ||
      response_code == net::HTTP_NOT_MODIFIED) {
    bool success = request->GetResponseHeaders()->GetDateValue(&response_date);
    DCHECK(success || response_date.is_null());

    if (!response_date.is_null()) {
      client_->GetNetworkTimeTracker()->UpdateNetworkTime(
          response_date,
          base::TimeDelta::FromMilliseconds(kServerTimeResolutionMs), latency,
          base::TimeTicks::Now());
    }
  }

  if (response_code != net::HTTP_OK) {
    DVLOG(1) << "Variations server request returned non-HTTP_OK response code: "
             << response_code;
    if (response_code == net::HTTP_NOT_MODIFIED) {
      RecordLastFetchTime();
      // Update the seed date value in local state (used for expiry check on
      // next start up), since 304 is a successful response.
      seed_store_.UpdateSeedDateAndLogDayChange(response_date);
    }
    return;
  }

  std::string seed_data;
  bool success = request->GetResponseAsString(&seed_data);
  DCHECK(success);

  net::HttpResponseHeaders* headers = request->GetResponseHeaders();
  bool is_delta_compressed;
  bool is_gzip_compressed;
  if (!GetInstanceManipulations(headers, &is_delta_compressed,
                                &is_gzip_compressed)) {
    // The header does not specify supported instance manipulations, unable to
    // process data. Details of errors were logged by GetInstanceManipulations.
    seed_store_.ReportUnsupportedSeedFormatError();
    return;
  }

  const std::string signature = GetHeaderValue(headers, "X-Seed-Signature");
  const std::string country_code = GetHeaderValue(headers, "X-Country");
  const bool store_success = StoreSeed(seed_data, signature, country_code,
                                       response_date, is_delta_compressed,
                                       is_gzip_compressed);
  if (!store_success && is_delta_compressed) {
    disable_deltas_for_next_request_ = true;
    request_scheduler_->ScheduleFetchShortly();
  }
}

void VariationsService::OnResourceRequestsAllowed() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note that this only attempts to fetch the seed at most once per period
  // (kSeedFetchPeriodHours). This works because
  // |resource_request_allowed_notifier_| only calls this method if an
  // attempt was made earlier that fails (which implies that the period had
  // elapsed). After a successful attempt is made, the notifier will know not
  // to call this method again until another failed attempt occurs.
  RecordRequestsAllowedHistogram(RESOURCE_REQUESTS_ALLOWED_NOTIFIED);
  DVLOG(1) << "Retrying fetch.";
  DoActualFetch();

  // This service must have created a scheduler in order for this to be called.
  DCHECK(request_scheduler_.get());
  request_scheduler_->Reset();
}

void VariationsService::PerformSimulationWithVersion(
    std::unique_ptr<variations::VariationsSeed> seed,
    const base::Version& version) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!version.IsValid())
    return;

  const base::ElapsedTimer timer;

  std::unique_ptr<const base::FieldTrial::EntropyProvider> default_provider =
      state_manager_->CreateDefaultEntropyProvider();
  std::unique_ptr<const base::FieldTrial::EntropyProvider> low_provider =
      state_manager_->CreateLowEntropyProvider();
  variations::VariationsSeedSimulator seed_simulator(*default_provider,
                                                     *low_provider);

  const std::string latest_country =
      local_state_->GetString(prefs::kVariationsCountry);
  const variations::VariationsSeedSimulator::Result result =
      seed_simulator.SimulateSeedStudies(
          *seed, client_->GetApplicationLocale(),
          GetReferenceDateForExpiryChecks(local_state_), version,
          GetChannelForVariations(client_->GetChannel()),
          GetCurrentFormFactor(), GetHardwareClass(), latest_country,
          LoadPermanentConsistencyCountry(version, latest_country));

  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.NormalChanges",
                           result.normal_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillBestEffortChanges",
                           result.kill_best_effort_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillCriticalChanges",
                           result.kill_critical_group_change_count);

  UMA_HISTOGRAM_TIMES("Variations.SimulateSeed.Duration", timer.Elapsed());

  NotifyObservers(result);
}

void VariationsService::RecordLastFetchTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // local_state_ is NULL in tests, so check it first.
  if (local_state_) {
    local_state_->SetInt64(prefs::kVariationsLastFetchTime,
                           base::Time::Now().ToInternalValue());
  }
}

std::string VariationsService::GetInvalidVariationsSeedSignature() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return seed_store_.GetInvalidSignature();
}

std::string VariationsService::LoadPermanentConsistencyCountry(
    const base::Version& version,
    const std::string& latest_country) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(version.IsValid());

  const base::ListValue* list_value =
      local_state_->GetList(prefs::kVariationsPermanentConsistencyCountry);
  std::string stored_version_string;
  std::string stored_country;

  // Determine if the saved pref value is present and valid.
  const bool is_pref_empty = list_value->empty();
  const bool is_pref_valid = list_value->GetSize() == 2 &&
                             list_value->GetString(0, &stored_version_string) &&
                             list_value->GetString(1, &stored_country) &&
                             base::Version(stored_version_string).IsValid();

  // Determine if the version from the saved pref matches |version|.
  const bool does_version_match =
      is_pref_valid && version == base::Version(stored_version_string);

  // Determine if the country in the saved pref matches the country in
  // |latest_country|.
  const bool does_country_match = is_pref_valid && !latest_country.empty() &&
                                  stored_country == latest_country;

  // Record a histogram for how the saved pref value compares to the current
  // version and the country code in the variations seed.
  LoadPermanentConsistencyCountryResult result;
  if (is_pref_empty) {
    result = !latest_country.empty() ? LOAD_COUNTRY_NO_PREF_HAS_SEED
                                     : LOAD_COUNTRY_NO_PREF_NO_SEED;
  } else if (!is_pref_valid) {
    result = !latest_country.empty() ? LOAD_COUNTRY_INVALID_PREF_HAS_SEED
                                     : LOAD_COUNTRY_INVALID_PREF_NO_SEED;
  } else if (latest_country.empty()) {
    result = does_version_match ? LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ
                                : LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ;
  } else if (does_version_match) {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ;
  } else {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ;
  }
  UMA_HISTOGRAM_ENUMERATION("Variations.LoadPermanentConsistencyCountryResult",
                            result, LOAD_COUNTRY_MAX);

  // Use the stored country if one is available and was fetched since the last
  // time Chrome was updated.
  if (does_version_match)
    return stored_country;

  if (latest_country.empty()) {
    if (!is_pref_valid)
      local_state_->ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    // If we've never received a country code from the server, use an empty
    // country so that it won't pass any filters that specifically include
    // countries, but so that it will pass any filters that specifically exclude
    // countries.
    return std::string();
  }

  // Otherwise, update the pref with the current Chrome version and country.
  StorePermanentCountry(version, latest_country);
  return latest_country;
}

void VariationsService::StorePermanentCountry(const base::Version& version,
                                              const std::string& country) {
  base::ListValue new_list_value;
  new_list_value.AppendString(version.GetString());
  new_list_value.AppendString(country);
  local_state_->Set(prefs::kVariationsPermanentConsistencyCountry,
                    new_list_value);
}

std::string VariationsService::GetStoredPermanentCountry() {
  const base::ListValue* list_value =
      local_state_->GetList(prefs::kVariationsPermanentConsistencyCountry);
  std::string stored_country;

  if (list_value->GetSize() == 2) {
    list_value->GetString(1, &stored_country);
  }

  return stored_country;
}

bool VariationsService::OverrideStoredPermanentCountry(
    const std::string& country_override) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (country_override.empty())
    return false;

  const base::ListValue* list_value =
      local_state_->GetList(prefs::kVariationsPermanentConsistencyCountry);

  std::string stored_country;
  const bool got_stored_country =
      list_value->GetSize() == 2 && list_value->GetString(1, &stored_country);

  if (got_stored_country && stored_country == country_override)
    return false;

  base::Version version(version_info::GetVersionNumber());
  StorePermanentCountry(version, country_override);
  return true;
}

}  // namespace variations
