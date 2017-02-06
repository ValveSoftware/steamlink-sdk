// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE SCHEMA DEFINITION AND DESCRIPTION IN
//   fieldtrial_testing_config_schema.json
//   test_config.json
// DO NOT EDIT.

#include "test_ouput.h"


const char* const array_kFieldTrialConfig_enable_features_0[] = {
    "X",
};
const char* const array_kFieldTrialConfig_disable_features[] = {
    "C",
};
const char* const array_kFieldTrialConfig_enable_features[] = {
    "A",
    "B",
};
const FieldTrialGroupParams array_kFieldTrialConfig_params[] = {
    {
      "x",
      "1",
    },
    {
      "y",
      "2",
    },
};
const FieldTrialTestingGroup array_kFieldTrialConfig_groups[] = {
  {
    "TestStudy1",
    "TestGroup1",
    NULL,
    0,
    NULL,
    0,
    NULL,
    0,
  },
  {
    "TestStudy2",
    "TestGroup2",
    array_kFieldTrialConfig_params,
    2,
    array_kFieldTrialConfig_enable_features,
    2,
    array_kFieldTrialConfig_disable_features,
    1,
  },
  {
    "TestStudy3",
    "TestGroup3",
    NULL,
    0,
    array_kFieldTrialConfig_enable_features_0,
    1,
    NULL,
    0,
  },
};
const FieldTrialTestingConfig kFieldTrialConfig = {
  array_kFieldTrialConfig_groups,
  3,
};
