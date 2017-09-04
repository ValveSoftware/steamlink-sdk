// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include "base/metrics/field_trial.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PreviewsExperimentsTest = testing::Test;

}  // namespace

namespace previews {

TEST_F(PreviewsExperimentsTest, TestFieldTrialOfflinePage) {
  EXPECT_FALSE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_FALSE(IsPreviewsTypeEnabled(PreviewsType::OFFLINE));

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(EnableOfflinePreviewsForTesting());

  EXPECT_TRUE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_TRUE(IsPreviewsTypeEnabled(PreviewsType::OFFLINE));
}

}  // namespace previews
