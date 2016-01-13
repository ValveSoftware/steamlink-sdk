# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'metrics',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/zlib/zlib.gyp:zlib',
        'component_metrics_proto',
        'variations',
      ],
      'sources': [
        'metrics/compression_utils.cc',
        'metrics/compression_utils.h',
        'metrics/cloned_install_detector.cc',
        'metrics/cloned_install_detector.h',
        'metrics/machine_id_provider.h',
        'metrics/machine_id_provider_stub.cc',
        'metrics/machine_id_provider_win.cc',
        'metrics/metrics_hashes.cc',
        'metrics/metrics_hashes.h',
        'metrics/metrics_log.cc',
        'metrics/metrics_log.h',
        'metrics/metrics_log_uploader.cc',
        'metrics/metrics_log_uploader.h',
        'metrics/metrics_log_manager.cc',
        'metrics/metrics_log_manager.h',
        'metrics/metrics_pref_names.cc',
        'metrics/metrics_pref_names.h',
        'metrics/metrics_provider.h',
        'metrics/metrics_reporting_scheduler.cc',
        'metrics/metrics_reporting_scheduler.h',
        'metrics/metrics_service.cc',
        'metrics/metrics_service.h',
        'metrics/metrics_service_client.h',
        'metrics/metrics_service_observer.cc',
        'metrics/metrics_service_observer.h',
        'metrics/metrics_state_manager.cc',
        'metrics/metrics_state_manager.h',
        'metrics/metrics_switches.cc',
        'metrics/metrics_switches.h',
        'metrics/persisted_logs.cc',
        'metrics/persisted_logs.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            'metrics_chromeos',
          ],
        }],
        ['OS=="win"', {
          'sources!': [
            'metrics/machine_id_provider_stub.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'metrics_net',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../net/net.gyp:net',
        'metrics',
      ],
      'sources': [
        'metrics/net/net_metrics_log_uploader.cc',
        'metrics/net/net_metrics_log_uploader.h',
      ],
    },
    {
      # Protobuf compiler / generator for UMA (User Metrics Analysis).
      #
      # GN version: //component/metrics/proto:proto
      'target_name': 'component_metrics_proto',
      'type': 'static_library',
      'sources': [
        'metrics/proto/chrome_user_metrics_extension.proto',
        'metrics/proto/histogram_event.proto',
        'metrics/proto/omnibox_event.proto',
        'metrics/proto/omnibox_input_type.proto',
        'metrics/proto/perf_data.proto',
        'metrics/proto/profiler_event.proto',
        'metrics/proto/sampled_profile.proto',
        'metrics/proto/system_profile.proto',
        'metrics/proto/user_action_event.proto',
      ],
      'variables': {
        'proto_in_dir': 'metrics/proto',
        'proto_out_dir': 'components/metrics/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
    {
      # TODO(isherman): Remove all //chrome dependencies on this target, and
      # merge the files in this target with components_unittests.
      'target_name': 'metrics_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'component_metrics_proto',
        'metrics',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/test_metrics_service_client.cc',
        'metrics/test_metrics_service_client.h',
      ],
    },
  ],
  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'metrics_chromeos',
          'type': 'static_library',
          'sources': [
            'metrics/chromeos/serialization_utils.cc',
            'metrics/chromeos/serialization_utils.h',
            'metrics/chromeos/metric_sample.cc',
            'metrics/chromeos/metric_sample.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
        },
      ],
    }],
  ],
}
