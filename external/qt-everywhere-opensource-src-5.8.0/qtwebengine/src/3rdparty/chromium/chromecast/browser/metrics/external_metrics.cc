// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/external_metrics.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/metrics/cast_stability_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_thread.h"

namespace chromecast {
namespace metrics {

namespace {

bool CheckValues(const std::string& name,
                 int minimum,
                 int maximum,
                 uint32_t bucket_count) {
  if (!base::Histogram::InspectConstructionArguments(
          name, &minimum, &maximum, &bucket_count))
    return false;
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return true;
  return histogram->HasConstructionArguments(minimum, maximum, bucket_count);
}

bool CheckLinearValues(const std::string& name, int maximum) {
  return CheckValues(name, 1, maximum, maximum + 1);
}

}  // namespace

// The interval between external metrics collections in seconds
static const int kExternalMetricsCollectionIntervalSeconds = 30;

ExternalMetrics::ExternalMetrics(
    CastStabilityMetricsProvider* stability_provider,
    const std::string& uma_events_file)
    : stability_provider_(stability_provider),
      uma_events_file_(uma_events_file),
      weak_factory_(this) {
  DCHECK(stability_provider);
}

ExternalMetrics::~ExternalMetrics() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
}

void ExternalMetrics::StopAndDestroy() {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::FILE, FROM_HERE, this);
}

void ExternalMetrics::Start() {
  bool result = content::BrowserThread::PostDelayedTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ExternalMetrics::CollectEventsAndReschedule,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kExternalMetricsCollectionIntervalSeconds));
  DCHECK(result);
}

void ExternalMetrics::ProcessExternalEvents(const base::Closure& cb) {
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ExternalMetrics::CollectEvents),
          weak_factory_.GetWeakPtr()),
      cb);
}

void ExternalMetrics::RecordCrash(const std::string& crash_kind) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CastStabilityMetricsProvider::LogExternalCrash,
                 base::Unretained(stability_provider_),
                 crash_kind));
}

void ExternalMetrics::RecordSparseHistogram(
    const ::metrics::MetricSample& sample) {
  CHECK_EQ(::metrics::MetricSample::SPARSE_HISTOGRAM, sample.type());
  base::HistogramBase* counter = base::SparseHistogram::FactoryGet(
      sample.name(), base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample.sample());
}

int ExternalMetrics::CollectEvents() {
  ScopedVector< ::metrics::MetricSample> samples;
  ::metrics::SerializationUtils::ReadAndTruncateMetricsFromFile(
      uma_events_file_, &samples);

  for (ScopedVector< ::metrics::MetricSample>::iterator it = samples.begin();
       it != samples.end();
       ++it) {
    const ::metrics::MetricSample& sample = **it;

    switch (sample.type()) {
      case ::metrics::MetricSample::CRASH:
        RecordCrash(sample.name());
        break;
      case ::metrics::MetricSample::USER_ACTION:
        CastMetricsHelper::GetInstance()->RecordSimpleAction(sample.name());
        break;
      case ::metrics::MetricSample::HISTOGRAM:
        if (!CheckValues(sample.name(), sample.min(), sample.max(),
                         sample.bucket_count())) {
          DLOG(ERROR) << "Invalid histogram: " << sample.name();
          break;
        }
        UMA_HISTOGRAM_CUSTOM_COUNTS_NO_CACHE(sample.name(),
                                             sample.sample(),
                                             sample.min(),
                                             sample.max(),
                                             sample.bucket_count(),
                                             1);
        break;
      case ::metrics::MetricSample::LINEAR_HISTOGRAM:
        if (!CheckLinearValues(sample.name(), sample.max())) {
          DLOG(ERROR) << "Invalid linear histogram: " << sample.name();
          break;
        }
        UMA_HISTOGRAM_ENUMERATION_NO_CACHE(
            sample.name(), sample.sample(), sample.max());
        break;
      case ::metrics::MetricSample::SPARSE_HISTOGRAM:
        RecordSparseHistogram(sample);
        break;
    }
  }

  return samples.size();
}

void ExternalMetrics::CollectEventsAndReschedule() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  CollectEvents();
  bool result = content::BrowserThread::PostDelayedTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ExternalMetrics::CollectEventsAndReschedule,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kExternalMetricsCollectionIntervalSeconds));
  DCHECK(result);
}

}  // namespace metrics
}  // namespace chromecast
