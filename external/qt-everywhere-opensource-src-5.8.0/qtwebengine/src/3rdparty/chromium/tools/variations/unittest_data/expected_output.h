// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE SCHEMA DEFINITION AND DESCRIPTION IN
//   fieldtrial_testing_config_schema.json
//   test_config.json
// DO NOT EDIT.

#ifndef TEST_OUPUT_H_
#define TEST_OUPUT_H_

#include <cstddef>


struct FieldTrialGroupParams {
  const char* const key;
  const char* const value;
};

struct FieldTrialTestingGroup {
  const char* const study;
  const char* const group_name;
  const FieldTrialGroupParams * params;
  const size_t params_size;
  const char* const * enable_features;
  const size_t enable_features_size;
  const char* const * disable_features;
  const size_t disable_features_size;
};

struct FieldTrialTestingConfig {
  const FieldTrialTestingGroup * groups;
  const size_t groups_size;
};


extern const FieldTrialTestingConfig kFieldTrialConfig;

#endif  // TEST_OUPUT_H_
