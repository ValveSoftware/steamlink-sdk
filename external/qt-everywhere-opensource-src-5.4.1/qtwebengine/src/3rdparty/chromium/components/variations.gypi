# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'variations',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
      ],
      'sources': [
        'variations/active_field_trials.cc',
        'variations/active_field_trials.h',
        'variations/caching_permuted_entropy_provider.cc',
        'variations/caching_permuted_entropy_provider.h',
        'variations/entropy_provider.cc',
        'variations/entropy_provider.h',
        'variations/metrics_util.cc',
        'variations/metrics_util.h',
        'variations/pref_names.cc',
        'variations/pref_names.h',
        'variations/processed_study.cc',
        'variations/processed_study.h',
        'variations/proto/permuted_entropy_cache.proto',
        'variations/proto/study.proto',
        'variations/proto/variations_seed.proto',
        'variations/study_filtering.cc',
        'variations/study_filtering.h',
        'variations/variations_associated_data.cc',
        'variations/variations_associated_data.h',
        'variations/variations_seed_processor.cc',
        'variations/variations_seed_processor.h',
        'variations/variations_seed_simulator.cc',
        'variations/variations_seed_simulator.h',
      ],
      'variables': {
        'proto_in_dir': 'variations/proto',
        'proto_out_dir': 'components/variations/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
