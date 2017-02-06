# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/variations
      'target_name': 'variations',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        # List of dependencies is intentionally very minimal. Please avoid
        # adding extra dependencies without first checking with OWNERS.
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/zlib/google/zip.gyp:compression_utils',
        'crash_core_common',
        'prefs/prefs.gyp:prefs',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'variations/active_field_trials.cc',
        'variations/active_field_trials.h',
        'variations/android/component_jni_registrar.cc',
        'variations/android/component_jni_registrar.h',
        'variations/android/variations_associated_data_android.cc',
        'variations/android/variations_associated_data_android.h',
        'variations/android/variations_seed_bridge.cc',
        'variations/android/variations_seed_bridge.h',
        'variations/caching_permuted_entropy_provider.cc',
        'variations/caching_permuted_entropy_provider.h',
        'variations/entropy_provider.cc',
        'variations/entropy_provider.h',
        'variations/experiment_labels.cc',
        'variations/experiment_labels.h',
        'variations/metrics_util.cc',
        'variations/metrics_util.h',
        'variations/pref_names.cc',
        'variations/pref_names.h',
        'variations/processed_study.cc',
        'variations/processed_study.h',
        'variations/proto/client_variations.proto',
        'variations/proto/permuted_entropy_cache.proto',
        'variations/proto/study.proto',
        'variations/proto/variations_seed.proto',
        'variations/study_filtering.cc',
        'variations/study_filtering.h',
        "variations/synthetic_trials.cc",
        "variations/synthetic_trials.h",
        'variations/variations_associated_data.cc',
        'variations/variations_associated_data.h',
        'variations/variations_experiment_util.cc',
        'variations/variations_experiment_util.h',
        'variations/variations_http_header_provider.cc',
        'variations/variations_http_header_provider.h',
        'variations/variations_request_scheduler.cc',
        'variations/variations_request_scheduler.h',
        'variations/variations_request_scheduler_mobile.cc',
        'variations/variations_request_scheduler_mobile.h',
        'variations/variations_seed_processor.cc',
        'variations/variations_seed_processor.h',
        'variations/variations_seed_simulator.cc',
        'variations/variations_seed_simulator.h',
        'variations/variations_seed_store.cc',
        'variations/variations_seed_store.h',
        'variations/variations_switches.cc',
        'variations/variations_switches.h',
        'variations/variations_url_constants.cc',
        'variations/variations_url_constants.h',
        'variations/variations_util.cc',
        'variations/variations_util.h',
      ],
      'variables': {
        'proto_in_dir': 'variations/proto',
        'proto_out_dir': 'components/variations/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'variations_jni_headers',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'sources!': [
            'variations/variations_request_scheduler_mobile.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //components/variations/service
      'target_name': 'variations_service',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        'data_use_measurement_core',
        'metrics',
        'network_time',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'variations',
        'version_info',
        'web_resource',
      ],
      'sources': [
        'variations/service/ui_string_overrider.h',
        'variations/service/ui_string_overrider.cc',
        'variations/service/variations_service.cc',
        'variations/service/variations_service.h',
        'variations/service/variations_service_client.h',
      ],
    },
    {
      # GN version: //components/variations/net:net
      'target_name': 'variations_net',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'components.gyp:google_core_browser',
        'components.gyp:metrics',
        'variations',
      ],
      'export_dependent_settings': [
        'components.gyp:metrics',
      ],
      'sources': [
        'variations/net/variations_http_headers.cc',
        'variations/net/variations_http_headers.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # GN version: //components/variations/android:variations_java
          'target_name': 'variations_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'variations/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/variations:jni
          'target_name': 'variations_jni_headers',
          'type': 'none',
          'sources': [
            'variations/android/java/src/org/chromium/components/variations/VariationsAssociatedData.java',
            'variations/android/java/src/org/chromium/components/variations/firstrun/VariationsSeedBridge.java',
          ],
          'variables': {
            'jni_gen_package': 'variations',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ]
}
