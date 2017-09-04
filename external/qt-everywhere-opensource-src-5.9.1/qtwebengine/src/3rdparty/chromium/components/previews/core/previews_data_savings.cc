// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_data_savings.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_savings_recorder.h"

namespace previews {

PreviewsDataSavings::PreviewsDataSavings(
    data_reduction_proxy::DataSavingsRecorder* data_savings_recorder)
    : data_savings_recorder_(data_savings_recorder) {
  DCHECK(data_savings_recorder);
}

PreviewsDataSavings::~PreviewsDataSavings() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PreviewsDataSavings::RecordDataSavings(
    const std::string& fully_qualified_domain_name,
    int64_t data_used,
    int64_t original_size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Only record savings when data saver is enabled.
  if (!data_savings_recorder_->UpdateDataSavings(fully_qualified_domain_name,
                                                 data_used, original_size)) {
    // Record metrics in KB for previews with data saver disabled.
    UMA_HISTOGRAM_COUNTS("Previews.OriginalContentLength.DataSaverDisabled",
                         original_size >> 10);
    UMA_HISTOGRAM_COUNTS("Previews.ContentLength.DataSaverDisabled",
                         data_used >> 10);
    if (original_size >= data_used) {
      UMA_HISTOGRAM_COUNTS("Previews.DataSavings.DataSaverDisabled",
                           (original_size - data_used) >> 10);
      UMA_HISTOGRAM_PERCENTAGE(
          "Previews.DataSavingsPercent.DataSaverDisabled",
          (original_size - data_used) * 100 / original_size);
    } else {
      UMA_HISTOGRAM_COUNTS("Previews.DataInflation.DataSaverDisabled",
                           (data_used - original_size) >> 10);
      UMA_HISTOGRAM_PERCENTAGE(
          "Previews.DataInflationPercent.DataSaverDisabled",
          (data_used - original_size) * 100 / data_used);
    }
    return;
  }

  // Record metrics in KB for previews with data saver enabled.
  UMA_HISTOGRAM_COUNTS("Previews.OriginalContentLength.DataSaverEnabled",
                       original_size >> 10);
  UMA_HISTOGRAM_COUNTS("Previews.ContentLength.DataSaverEnabled",
                       data_used >> 10);
  if (original_size >= data_used) {
    UMA_HISTOGRAM_COUNTS("Previews.DataSavings.DataSaverEnabled",
                         (original_size - data_used) >> 10);
    UMA_HISTOGRAM_PERCENTAGE("Previews.DataSavingsPercent.DataSaverEnabled",
                             (original_size - data_used) * 100 / original_size);
  } else {
    UMA_HISTOGRAM_COUNTS("Previews.DataInflation.DataSaverEnabled",
                         (data_used - original_size) >> 10);
    UMA_HISTOGRAM_PERCENTAGE("Previews.DataInflationPercent.DataSaverEnabled",
                             (data_used - original_size) * 100 / data_used);
  }
}

}  // namespace previews
