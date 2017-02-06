// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"

namespace network_time {

namespace {

// Time updates happen in two ways. First, other components may call
// UpdateNetworkTime() if they happen to obtain the time securely.  This will
// likely be deprecated in favor of the second way, which is scheduled time
// queries issued by NetworkTimeTracker itself.
//
// On startup, the clock state may be read from a pref.  (This, too, may be
// deprecated.)  After that, the time is checked every
// |kCheckTimeIntervalSeconds|.  A "check" means the possibility, but not the
// certainty, of a time query.  A time query may be issued at random, or if the
// network time is believed to have become inaccurate.
//
// After issuing a query, the next check will not happen until
// |kBackoffMinutes|.  This delay is doubled in the event of an error.

// Minimum number of minutes between time queries.
const uint32_t kBackoffMinutes = 60;

// Number of seconds between time checks.  This may be overridden via Variations
// Service.
// Note that a "check" is not necessarily a network time query!
const uint32_t kCheckTimeIntervalSeconds = 360;

// Probability that a check will randomly result in a query.  This may
// be overridden via Variations Service.  Checks are made every
// |kCheckTimeIntervalSeconds|.  The default values are chosen with
// the goal of a high probability that a query will be issued every 24
// hours.
const float kRandomQueryProbability = .012f;

// Number of time measurements performed in a given network time calculation.
const uint32_t kNumTimeMeasurements = 7;

// Amount of divergence allowed between wall clock and tick clock.
const uint32_t kClockDivergenceSeconds = 60;

// Maximum time lapse before deserialized data are considered stale.
const uint32_t kSerializedDataMaxAgeDays = 7;

// Name of a pref that stores the wall clock time, via |ToJsTime|.
const char kPrefTime[] = "local";

// Name of a pref that stores the tick clock time, via |ToInternalValue|.
const char kPrefTicks[] = "ticks";

// Name of a pref that stores the time uncertainty, via |ToInternalValue|.
const char kPrefUncertainty[] = "uncertainty";

// Name of a pref that stores the network time via |ToJsTime|.
const char kPrefNetworkTime[] = "network";

// Time server's maximum allowable clock skew, in seconds.  (This is a property
// of the time server that we happen to know.  It's unlikely that it would ever
// be that badly wrong, but all the same it's included here to document the very
// rough nature of the time service provided by this class.)
const uint32_t kTimeServerMaxSkewSeconds = 10;

const char kTimeServiceURL[] = "http://clients2.google.com/time/1/current";

// Variations Service feature that enables network time service querying.
const base::Feature kNetworkTimeServiceQuerying{
    "NetworkTimeServiceQuerying", base::FEATURE_DISABLED_BY_DEFAULT};

const char kVariationsServiceCheckTimeIntervalSeconds[] =
    "CheckTimeIntervalSeconds";
const char kVariationsServiceRandomQueryProbability[] =
    "RandomQueryProbability";

// This is an ECDSA prime256v1 named-curve key.
const int kKeyVersion = 1;
const uint8_t kKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xeb, 0xd8, 0xad, 0x0b, 0x8f, 0x75, 0xe8, 0x84, 0x36,
    0x23, 0x48, 0x14, 0x24, 0xd3, 0x93, 0x42, 0x25, 0x43, 0xc1, 0xde, 0x36,
    0x29, 0xc6, 0x95, 0xca, 0xeb, 0x28, 0x85, 0xff, 0x09, 0xdc, 0x08, 0xec,
    0x45, 0x74, 0x6e, 0x4b, 0xc3, 0xa5, 0xfd, 0x8a, 0x2f, 0x02, 0xa0, 0x4b,
    0xc3, 0xc6, 0xa4, 0x7b, 0xa4, 0x41, 0xfc, 0xa7, 0x02, 0x54, 0xab, 0xe3,
    0xe4, 0xb1, 0x00, 0xf5, 0xd5, 0x09, 0x11};

std::string GetServerProof(const net::URLFetcher* source) {
  const net::HttpResponseHeaders* response_headers =
      source->GetResponseHeaders();
  if (!response_headers) {
    return std::string();
  }
  std::string proof;
  return response_headers->EnumerateHeader(nullptr, "x-cup-server-proof",
                                           &proof)
             ? proof
             : std::string();
}

// Limits the amount of data that will be buffered from the server's response.
class SizeLimitingStringWriter : public net::URLFetcherStringWriter {
 public:
  explicit SizeLimitingStringWriter(size_t limit) : limit_(limit) {}

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    if (data().length() + num_bytes > limit_) {
      return net::ERR_FILE_TOO_BIG;
    }
    return net::URLFetcherStringWriter::Write(buffer, num_bytes, callback);
  }

 private:
  size_t limit_;
};

base::TimeDelta CheckTimeInterval() {
  int64_t seconds;
  const std::string param = variations::GetVariationParamValueByFeature(
      kNetworkTimeServiceQuerying, kVariationsServiceCheckTimeIntervalSeconds);
  if (!param.empty() && base::StringToInt64(param, &seconds) && seconds > 0) {
    return base::TimeDelta::FromSeconds(seconds);
  }
  return base::TimeDelta::FromSeconds(kCheckTimeIntervalSeconds);
}

double RandomQueryProbability() {
  double probability;
  const std::string param = variations::GetVariationParamValueByFeature(
      kNetworkTimeServiceQuerying, kVariationsServiceRandomQueryProbability);
  if (!param.empty() && base::StringToDouble(param, &probability) &&
      probability >= 0.0 && probability <= 1.0) {
    return probability;
  }
  return kRandomQueryProbability;
}

}  // namespace

// static
void NetworkTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkTimeMapping,
                                   new base::DictionaryValue());
}

NetworkTimeTracker::NetworkTimeTracker(
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<base::TickClock> tick_clock,
    PrefService* pref_service,
    scoped_refptr<net::URLRequestContextGetter> getter)
    : server_url_(kTimeServiceURL),
      max_response_size_(1024),
      backoff_(base::TimeDelta::FromMinutes(kBackoffMinutes)),
      getter_(std::move(getter)),
      loop_(nullptr),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      pref_service_(pref_service) {
  const base::DictionaryValue* time_mapping =
      pref_service_->GetDictionary(prefs::kNetworkTimeMapping);
  double time_js = 0;
  double ticks_js = 0;
  double network_time_js = 0;
  double uncertainty_js = 0;
  if (time_mapping->GetDouble(kPrefTime, &time_js) &&
      time_mapping->GetDouble(kPrefTicks, &ticks_js) &&
      time_mapping->GetDouble(kPrefUncertainty, &uncertainty_js) &&
      time_mapping->GetDouble(kPrefNetworkTime, &network_time_js)) {
    time_at_last_measurement_ = base::Time::FromJsTime(time_js);
    ticks_at_last_measurement_ = base::TimeTicks::FromInternalValue(
        static_cast<int64_t>(ticks_js));
    network_time_uncertainty_ = base::TimeDelta::FromInternalValue(
        static_cast<int64_t>(uncertainty_js));
    network_time_at_last_measurement_ = base::Time::FromJsTime(network_time_js);
  }
  base::Time now = clock_->Now();
  if (ticks_at_last_measurement_ > tick_clock_->NowTicks() ||
      time_at_last_measurement_ > now ||
      now - time_at_last_measurement_ >
          base::TimeDelta::FromDays(kSerializedDataMaxAgeDays)) {
    // Drop saved mapping if either clock has run backward, or the data are too
    // old.
    pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    network_time_at_last_measurement_ = base::Time();  // Reset.
  }

  base::StringPiece public_key = {reinterpret_cast<const char*>(kKeyPubBytes),
                                  sizeof(kKeyPubBytes)};
  query_signer_ =
      client_update_protocol::Ecdsa::Create(kKeyVersion, public_key);

  QueueCheckTime(base::TimeDelta::FromSeconds(0));
}

NetworkTimeTracker::~NetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkTimeTracker::UpdateNetworkTime(base::Time network_time,
                                           base::TimeDelta resolution,
                                           base::TimeDelta latency,
                                           base::TimeTicks post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << base::UTF16ToUTF8(
                  base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_at_last_measurement_|.
  network_time_at_last_measurement_ = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now_ticks - post_time;
  DCHECK_GE(task_delay.InMilliseconds(), 0);
  DCHECK_GE(latency.InMilliseconds(), 0);
  // Estimate that the time was set midway through the latency time.
  base::TimeDelta offset = task_delay + latency / 2;
  ticks_at_last_measurement_ = now_ticks - offset;
  time_at_last_measurement_ = clock_->Now() - offset;

  // Can't assume a better time than the resolution of the given time and the
  // ticks measurements involved, each with their own uncertainty.  1 & 2 are
  // the ones used to compute the latency, 3 is the Now() from when this task
  // was posted, 4 and 5 are the Now() and NowTicks() above, and 6 and 7 will be
  // the Now() and NowTicks() in GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency + kNumTimeMeasurements *
      base::TimeDelta::FromMilliseconds(kTicksResolutionMs);

  base::DictionaryValue time_mapping;
  time_mapping.SetDouble(kPrefTime, time_at_last_measurement_.ToJsTime());
  time_mapping.SetDouble(kPrefTicks, static_cast<double>(
      ticks_at_last_measurement_.ToInternalValue()));
  time_mapping.SetDouble(kPrefUncertainty, static_cast<double>(
      network_time_uncertainty_.ToInternalValue()));
  time_mapping.SetDouble(kPrefNetworkTime,
      network_time_at_last_measurement_.ToJsTime());
  pref_service_->Set(prefs::kNetworkTimeMapping, time_mapping);
}

void NetworkTimeTracker::SetTimeServerURLForTesting(const GURL& url) {
  server_url_ = url;
}

void NetworkTimeTracker::SetMaxResponseSizeForTesting(size_t limit) {
  max_response_size_ = limit;
}

void NetworkTimeTracker::SetPublicKeyForTesting(const base::StringPiece& key) {
  query_signer_ = client_update_protocol::Ecdsa::Create(kKeyVersion, key);
}

bool NetworkTimeTracker::QueryTimeServiceForTesting() {
  CheckTime();
  loop_ = base::MessageLoop::current();  // Gets Quit on completion.
  return time_fetcher_ != nullptr;
}

void NetworkTimeTracker::WaitForFetchForTesting(uint32_t nonce) {
  query_signer_->OverrideNonceForTesting(kKeyVersion, nonce);
  base::RunLoop().Run();
}

base::TimeDelta NetworkTimeTracker::GetTimerDelayForTesting() const {
  DCHECK(timer_.IsRunning());
  return timer_.GetCurrentDelay();
}

bool NetworkTimeTracker::GetNetworkTime(base::Time* network_time,
                                        base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (network_time_at_last_measurement_.is_null()) {
    return false;
  }
  DCHECK(!ticks_at_last_measurement_.is_null());
  DCHECK(!time_at_last_measurement_.is_null());
  base::TimeDelta tick_delta =
      tick_clock_->NowTicks() - ticks_at_last_measurement_;
  base::TimeDelta time_delta = clock_->Now() - time_at_last_measurement_;
  if (time_delta.InMilliseconds() < 0) {  // Has wall clock run backward?
    DVLOG(1) << "Discarding network time due to wall clock running backward";
    network_time_at_last_measurement_ = base::Time();
    return false;
  }
  // Now we know that both |tick_delta| and |time_delta| are positive.
  base::TimeDelta divergence = (tick_delta - time_delta).magnitude();
  if (divergence > base::TimeDelta::FromSeconds(kClockDivergenceSeconds)) {
    // Most likely either the machine has suspended, or the wall clock has been
    // reset.
    DVLOG(1) << "Discarding network time due to clocks diverging";
    network_time_at_last_measurement_ = base::Time();
    return false;
  }
  *network_time = network_time_at_last_measurement_ + tick_delta;
  if (uncertainty) {
    *uncertainty = network_time_uncertainty_ + divergence;
  }
  return true;
}

void NetworkTimeTracker::CheckTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If NetworkTimeTracker is waking up after a backoff, this will reset the
  // timer to its default faster frequency.
  QueueCheckTime(CheckTimeInterval());

  if (!ShouldIssueTimeQuery()) {
    return;
  }

  std::string query_string;
  query_signer_->SignRequest(nullptr, &query_string);
  GURL url = server_url_;
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  url = url.ReplaceComponents(replacements);

  // This cancels any outstanding fetch.
  time_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
  if (!time_fetcher_) {
    DVLOG(1) << "tried to make fetch happen; failed";
    return;
  }
  time_fetcher_->SaveResponseWithWriter(
      std::unique_ptr<net::URLFetcherResponseWriter>(
          new SizeLimitingStringWriter(max_response_size_)));
  DCHECK(getter_);
  time_fetcher_->SetRequestContext(getter_.get());
  // Not expecting any cookies, but just in case.
  time_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                              net::LOAD_DO_NOT_SAVE_COOKIES |
                              net::LOAD_DO_NOT_SEND_COOKIES |
                              net::LOAD_DO_NOT_SEND_AUTH_DATA);
  time_fetcher_->Start();
  fetch_started_ = tick_clock_->NowTicks();

  timer_.Stop();  // Restarted in OnURLFetchComplete().
}

bool NetworkTimeTracker::UpdateTimeFromResponse() {
  if (time_fetcher_->GetStatus().status() != net::URLRequestStatus::SUCCESS &&
      time_fetcher_->GetResponseCode() != 200) {
    DVLOG(1) << "fetch failed, status=" << time_fetcher_->GetStatus().status()
             << ",code=" << time_fetcher_->GetResponseCode();
    return false;
  }

  std::string response_body;
  if (!time_fetcher_->GetResponseAsString(&response_body)) {
    DVLOG(1) << "failed to get response";
    return false;
  }
  DCHECK(query_signer_);
  if (!query_signer_->ValidateResponse(response_body,
                                       GetServerProof(time_fetcher_.get()))) {
    DVLOG(1) << "invalid signature";
    return false;
  }
  response_body = response_body.substr(5);  // Skips leading )]}'\n
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response_body);
  if (!value) {
    DVLOG(1) << "bad JSON";
    return false;
  }
  const base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    DVLOG(1) << "not a dictionary";
    return false;
  }
  double current_time_millis;
  if (!dict->GetDouble("current_time_millis", &current_time_millis)) {
    DVLOG(1) << "no current_time_millis";
    return false;
  }
  // There is a "server_nonce" key here too, but it serves no purpose other than
  // to make the server's response unpredictable.
  base::Time current_time = base::Time::FromJsTime(current_time_millis);
  base::TimeDelta resolution =
      base::TimeDelta::FromMilliseconds(1) +
      base::TimeDelta::FromSeconds(kTimeServerMaxSkewSeconds);
  base::TimeDelta latency = tick_clock_->NowTicks() - fetch_started_;
  UpdateNetworkTime(current_time, resolution, latency, tick_clock_->NowTicks());
  return true;
}

void NetworkTimeTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(time_fetcher_);
  DCHECK_EQ(source, time_fetcher_.get());

  // After completion of a query, whether succeeded or failed, go to sleep for a
  // long time.
  if (!UpdateTimeFromResponse()) {  // On error, back off.
    if (backoff_ < base::TimeDelta::FromDays(2)) {
      backoff_ *= 2;
    }
  } else {
    backoff_ = base::TimeDelta::FromMinutes(kBackoffMinutes);
  }
  QueueCheckTime(backoff_);
  time_fetcher_.reset();
  if (loop_ != nullptr) {
    loop_->QuitWhenIdle();
    loop_ = nullptr;
  }
}

void NetworkTimeTracker::QueueCheckTime(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &NetworkTimeTracker::CheckTime);
}

bool NetworkTimeTracker::ShouldIssueTimeQuery() {
  // Do not query the time service if not enabled via Variations Service.
  if (!base::FeatureList::IsEnabled(kNetworkTimeServiceQuerying)) {
    return false;
  }

  // If GetNetworkTime() returns false, synchronization has been lost
  // and a query is needed.
  base::Time network_time;
  if (!GetNetworkTime(&network_time, nullptr)) {
    return true;
  }

  // Otherwise, make the decision at random.
  return base::RandDouble() < RandomQueryProbability();
}

}  // namespace network_time
