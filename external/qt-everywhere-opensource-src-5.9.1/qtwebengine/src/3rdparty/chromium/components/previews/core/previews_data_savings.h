// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_DATA_SAVINGS_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_DATA_SAVINGS_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"

namespace data_reduction_proxy {
class DataSavingsRecorder;
}

namespace previews {

// Provides an interface for previews to report data savings.
class PreviewsDataSavings {
 public:
  // Embedder must guarantee that |data_savings_recorder| and
  // |data_reduction_proxy_settings| outlive this instance.
  PreviewsDataSavings(
      data_reduction_proxy::DataSavingsRecorder* data_savings_recorder);
  ~PreviewsDataSavings();

  // Records the amount of data used by the preview, and the amount of data
  // that would have been used if the original page had been loaded instead of
  // the preview. |fully_qualified_domain_name| is the full host name to
  // attribute the data savings to (e.g., codereview.chromium.org).
  void RecordDataSavings(const std::string& fully_qualified_domain_name,
                         int64_t data_used,
                         int64_t original_size);

 private:
  // Owned by the embedder, must be valid for the lifetime of |this|.
  // Provides a method to record data savings.
  data_reduction_proxy::DataSavingsRecorder* data_savings_recorder_;

  // Enforce usage on the UI thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsDataSavings);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_DATA_SAVINGS_H_
