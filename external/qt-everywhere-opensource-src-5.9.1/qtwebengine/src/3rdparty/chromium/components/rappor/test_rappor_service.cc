// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/test_rappor_service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"
#include "components/rappor/rappor_prefs.h"
#include "components/rappor/test_log_uploader.h"

namespace rappor {

namespace {

bool MockIsIncognito(bool* is_incognito) {
  return *is_incognito;
}

}  // namespace

TestSample::TestSample(RapporType type)
    : Sample(0, internal::kRapporParametersForType[type]), shadow_(type) {}

TestSample::~TestSample() {}

void TestSample::SetStringField(const std::string& field_name,
                                const std::string& value) {
  shadow_.string_fields[field_name] = value;
  Sample::SetStringField(field_name, value);
}

void TestSample::SetFlagsField(const std::string& field_name,
                               uint64_t flags,
                               size_t num_flags) {
  shadow_.flag_fields[field_name] = flags;
  Sample::SetFlagsField(field_name, flags, num_flags);
}

TestSample::Shadow::Shadow(RapporType type) : type(type) {}

TestSample::Shadow::Shadow(const TestSample::Shadow& other) {
  type = other.type;
  flag_fields = other.flag_fields;
  string_fields = other.string_fields;
}

TestSample::Shadow::~Shadow() {}

TestRapporService::TestRapporService()
    : RapporService(&test_prefs_, base::Bind(&MockIsIncognito, &is_incognito_)),
      next_rotation_(base::TimeDelta()),
      is_incognito_(false) {
  RegisterPrefs(test_prefs_.registry());
  test_uploader_ = new TestLogUploader();
  InitializeInternal(base::WrapUnique(test_uploader_), 0,
                     HmacByteVectorGenerator::GenerateEntropyInput());
  Update(UMA_RAPPOR_GROUP | SAFEBROWSING_RAPPOR_GROUP, true);
}

TestRapporService::~TestRapporService() {}

std::unique_ptr<Sample> TestRapporService::CreateSample(RapporType type) {
  std::unique_ptr<TestSample> test_sample(new TestSample(type));
  return std::move(test_sample);
}

// Intercepts the sample being recorded and saves it in a test structure.
void TestRapporService::RecordSampleObj(const std::string& metric_name,
                                        std::unique_ptr<Sample> sample) {
  TestSample* test_sample = static_cast<TestSample*>(sample.get());
  // Erase the previous sample if we logged one.
  shadows_.erase(metric_name);
  shadows_.insert(std::pair<std::string, TestSample::Shadow>(
      metric_name, test_sample->GetShadow()));
  // Original version is still called.
  RapporService::RecordSampleObj(metric_name, std::move(sample));
}

void TestRapporService::RecordSample(const std::string& metric_name,
                                     RapporType type,
                                     const std::string& sample) {
  // Save the recorded sample to the local structure.
  RapporSample rappor_sample;
  rappor_sample.type = type;
  rappor_sample.value = sample;
  samples_[metric_name] = rappor_sample;
  // Original version is still called.
  RapporService::RecordSample(metric_name, type, sample);
}

int TestRapporService::GetReportsCount() {
  RapporReports reports;
  ExportMetrics(&reports);
  return reports.report_size();
}

void TestRapporService::GetReports(RapporReports* reports) {
  ExportMetrics(reports);
}

TestSample::Shadow* TestRapporService::GetRecordedSampleForMetric(
    const std::string& metric_name) {
  ShadowMap::iterator it = shadows_.find(metric_name);
  if (it == shadows_.end())
    return nullptr;
  return &it->second;
}

bool TestRapporService::GetRecordedSampleForMetric(
    const std::string& metric_name,
    std::string* sample,
    RapporType* type) {
  SamplesMap::iterator it = samples_.find(metric_name);
  if (it == samples_.end())
    return false;
  *sample = it->second.value;
  *type = it->second.type;
  return true;
}

// Cancel the next call to OnLogInterval.
void TestRapporService::CancelNextLogRotation() {
  next_rotation_ = base::TimeDelta();
}

// Schedule the next call to OnLogInterval.
void TestRapporService::ScheduleNextLogRotation(base::TimeDelta interval) {
  next_rotation_ = interval;
}

}  // namespace rappor
