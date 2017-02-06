// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace domain_reliability {

namespace {
void LogOnBeaconDidEvictHistogram(bool evicted) {
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.OnBeaconDidEvict", evicted);
}
}  // namespace

// static
const int DomainReliabilityContext::kMaxUploadDepthToSchedule = 1;

DomainReliabilityContext::Factory::~Factory() {
}

// static
const size_t DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    const std::string& upload_reporter_string,
    const base::TimeTicks* last_network_change_time,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    std::unique_ptr<const DomainReliabilityConfig> config)
    : config_(std::move(config)),
      time_(time),
      upload_reporter_string_(upload_reporter_string),
      scheduler_(time,
                 config_->collectors.size(),
                 scheduler_params,
                 base::Bind(&DomainReliabilityContext::ScheduleUpload,
                            base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      uploading_beacons_size_(0),
      last_network_change_time_(last_network_change_time),
      weak_factory_(this) {}

DomainReliabilityContext::~DomainReliabilityContext() {
  ClearBeacons();
}

void DomainReliabilityContext::OnBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  bool success = (beacon->status == "ok");
  double sample_rate = beacon->details.quic_port_migration_detected
                           ? 1.0
                           : config().GetSampleRate(success);
  bool should_report = base::RandDouble() < sample_rate;
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.BeaconReported", should_report);
  if (!should_report) {
    // If the beacon isn't queued to be reported, it definitely cannot evict
    // an older beacon. (This histogram is also logged below based on whether
    // an older beacon was actually evicted.)
    LogOnBeaconDidEvictHistogram(false);
    return;
  }
  beacon->sample_rate = sample_rate;

  UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.ReportedBeaconError",
                              -beacon->chrome_error);
  if (!beacon->server_ip.empty()) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "DomainReliability.ReportedBeaconError_HasServerIP",
        -beacon->chrome_error);
  }
  // TODO(juliatuttle): Histogram HTTP response code?

  // Allow beacons about reports, but don't schedule an upload for more than
  // one layer of recursion, to avoid infinite report loops.
  if (beacon->upload_depth <= kMaxUploadDepthToSchedule)
    scheduler_.OnBeaconAdded();
  beacons_.push_back(beacon.release());
  bool should_evict = beacons_.size() > kMaxQueuedBeacons;
  if (should_evict)
    RemoveOldestBeacon();

  LogOnBeaconDidEvictHistogram(should_evict);
}

void DomainReliabilityContext::ClearBeacons() {
  STLDeleteElements(&beacons_);
  beacons_.clear();
  uploading_beacons_size_ = 0;
}

std::unique_ptr<Value> DomainReliabilityContext::GetWebUIData() const {
  DictionaryValue* context_value = new DictionaryValue();

  context_value->SetString("origin", config().origin.spec());
  context_value->SetInteger("beacon_count", static_cast<int>(beacons_.size()));
  context_value->SetInteger("uploading_beacon_count",
      static_cast<int>(uploading_beacons_size_));
  context_value->Set("scheduler", scheduler_.GetWebUIData());

  return std::unique_ptr<Value>(context_value);
}

void DomainReliabilityContext::GetQueuedBeaconsForTesting(
    std::vector<const DomainReliabilityBeacon*>* beacons_out) const {
  DCHECK(this);
  DCHECK(beacons_out);
  beacons_out->assign(beacons_.begin(), beacons_.end());
}

void DomainReliabilityContext::ScheduleUpload(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  dispatcher_->ScheduleTask(
      base::Bind(
          &DomainReliabilityContext::StartUpload,
          weak_factory_.GetWeakPtr()),
      min_delay,
      max_delay);
}

void DomainReliabilityContext::StartUpload() {
  MarkUpload();

  size_t collector_index = scheduler_.OnUploadStart();
  const GURL& collector_url = *config().collectors[collector_index];

  DCHECK(upload_time_.is_null());
  upload_time_ = time_->NowTicks();
  std::string report_json = "{}";
  int max_upload_depth = -1;
  bool wrote = base::JSONWriter::Write(
      *CreateReport(upload_time_,
                    collector_url,
                    &max_upload_depth),
                    &report_json);
  DCHECK(wrote);
  DCHECK_NE(-1, max_upload_depth);

  uploader_->UploadReport(
      report_json,
      max_upload_depth,
      collector_url,
      base::Bind(
          &DomainReliabilityContext::OnUploadComplete,
          weak_factory_.GetWeakPtr()));

  UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.UploadCollectorIndex",
                              static_cast<int>(collector_index));
  if (!last_upload_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadInterval",
                             upload_time_ - last_upload_time_);
  }
}

void DomainReliabilityContext::OnUploadComplete(
    const DomainReliabilityUploader::UploadResult& result) {
  if (result.is_success())
    CommitUpload();
  else
    RollbackUpload();
  base::TimeTicks first_beacon_time = scheduler_.first_beacon_time();
  scheduler_.OnUploadComplete(result);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.UploadSuccess",
      result.is_success());
  base::TimeTicks now = time_->NowTicks();
  UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadLatency",
                           now - first_beacon_time);
  DCHECK(!upload_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES("DomainReliability.UploadDuration",
                             now - upload_time_);
  UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadCollectorRetryDelay",
                           scheduler_.last_collector_retry_delay());
  last_upload_time_ = upload_time_;
  upload_time_ = base::TimeTicks();
}

std::unique_ptr<const Value> DomainReliabilityContext::CreateReport(
    base::TimeTicks upload_time,
    const GURL& collector_url,
    int* max_upload_depth_out) const {
  int max_upload_depth = 0;

  std::unique_ptr<ListValue> beacons_value(new ListValue());
  for (const auto& beacon : beacons_) {
    beacons_value->Append(beacon->ToValue(upload_time,
                                          *last_network_change_time_,
                                          collector_url,
                                          config().path_prefixes));
    if (beacon->upload_depth > max_upload_depth)
      max_upload_depth = beacon->upload_depth;
  }

  std::unique_ptr<DictionaryValue> report_value(new DictionaryValue());
  report_value->SetString("reporter", upload_reporter_string_);
  report_value->Set("entries", beacons_value.release());

  *max_upload_depth_out = max_upload_depth;
  return std::move(report_value);
}

void DomainReliabilityContext::MarkUpload() {
  DCHECK_EQ(0u, uploading_beacons_size_);
  uploading_beacons_size_ = beacons_.size();
  DCHECK_NE(0u, uploading_beacons_size_);
}

void DomainReliabilityContext::CommitUpload() {
  auto begin = beacons_.begin();
  auto end = begin + uploading_beacons_size_;
  STLDeleteContainerPointers(begin, end);
  beacons_.erase(begin, end);
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RollbackUpload() {
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RemoveOldestBeacon() {
  DCHECK(!beacons_.empty());

  VLOG(1) << "Beacon queue for " << config().origin << " full; "
          << "removing oldest beacon";

  delete beacons_.front();
  beacons_.pop_front();

  // If that just removed a beacon counted in uploading_beacons_size_, decrement
  // that.
  if (uploading_beacons_size_ > 0)
    --uploading_beacons_size_;
}

}  // namespace domain_reliability
