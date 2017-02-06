# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/metrics
      'target_name': 'metrics',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/zlib/google/zip.gyp:compression_utils',
        'component_metrics_proto',
        'prefs/prefs.gyp:prefs',
        'variations',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/call_stack_profile_metrics_provider.cc',
        'metrics/call_stack_profile_metrics_provider.h',
        'metrics/clean_exit_beacon.cc',
        'metrics/clean_exit_beacon.h',
        'metrics/client_info.cc',
        'metrics/client_info.h',
        'metrics/cloned_install_detector.cc',
        'metrics/cloned_install_detector.h',
        'metrics/daily_event.cc',
        'metrics/daily_event.h',
        'metrics/drive_metrics_provider.cc',
        'metrics/drive_metrics_provider.h',
        'metrics/drive_metrics_provider_android.cc',
        'metrics/drive_metrics_provider_ios.mm',
        'metrics/drive_metrics_provider_linux.cc',
        'metrics/drive_metrics_provider_mac.mm',
        'metrics/drive_metrics_provider_win.cc',
        'metrics/enabled_state_provider.cc',
        'metrics/enabled_state_provider.h',
        'metrics/file_metrics_provider.cc',
        'metrics/file_metrics_provider.h',
        'metrics/histogram_encoder.cc',
        'metrics/histogram_encoder.h',
        'metrics/machine_id_provider.h',
        'metrics/machine_id_provider_stub.cc',
        'metrics/machine_id_provider_win.cc',
        'metrics/data_use_tracker.cc',
        'metrics/data_use_tracker.h',
        'metrics/metrics_log.cc',
        'metrics/metrics_log.h',
        'metrics/metrics_log_manager.cc',
        'metrics/metrics_log_manager.h',
        'metrics/metrics_log_uploader.cc',
        'metrics/metrics_log_uploader.h',
        'metrics/metrics_pref_names.cc',
        'metrics/metrics_pref_names.h',
        'metrics/metrics_provider.cc',
        'metrics/metrics_provider.h',
        'metrics/metrics_reporting_default_state.cc',
        'metrics/metrics_reporting_default_state.h',
        'metrics/metrics_reporting_scheduler.cc',
        'metrics/metrics_reporting_scheduler.h',
        'metrics/metrics_service.cc',
        'metrics/metrics_service.h',
        'metrics/metrics_service_accessor.cc',
        'metrics/metrics_service_accessor.h',
        'metrics/metrics_service_client.cc',
        'metrics/metrics_service_client.h',
        'metrics/metrics_state_manager.cc',
        'metrics/metrics_state_manager.h',
        'metrics/metrics_switches.cc',
        'metrics/metrics_switches.h',
        'metrics/persisted_logs.cc',
        'metrics/persisted_logs.h',
        'metrics/stability_metrics_helper.cc',
        'metrics/stability_metrics_helper.h',
        'metrics/system_memory_stats_recorder.h',
        'metrics/system_memory_stats_recorder_linux.cc',
        'metrics/system_memory_stats_recorder_win.cc',
        'metrics/url_constants.cc',
        'metrics/url_constants.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            'metrics_serialization',
          ],
        }],
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              # The below are all needed for drive_metrics_provider_mac.mm.
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/DiskArbitration.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'sources!': [
            'metrics/machine_id_provider_stub.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //components/metrics:net
      'target_name': 'metrics_net',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'component_metrics_proto',
        'data_use_measurement_core',
        'metrics',
        'version_info',
      ],
      'sources': [
        'metrics/net/cellular_logic_helper.cc',
        'metrics/net/cellular_logic_helper.h',
        'metrics/net/net_metrics_log_uploader.cc',
        'metrics/net/net_metrics_log_uploader.h',
        'metrics/net/network_metrics_provider.cc',
        'metrics/net/network_metrics_provider.h',
        'metrics/net/version_utils.cc',
        'metrics/net/version_utils.h',
        'metrics/net/wifi_access_point_info_provider.cc',
        'metrics/net/wifi_access_point_info_provider.h',
        'metrics/net/wifi_access_point_info_provider_chromeos.cc',
        'metrics/net/wifi_access_point_info_provider_chromeos.h',
      ],
    },
    {
      # GN version: //components/metrics:ui
      'target_name': 'metrics_ui',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/display/display.gyp:display',
        '../ui/gfx/gfx.gyp:gfx',
        'metrics',
      ],
      'sources': [
        'metrics/ui/screen_info_metrics_provider.cc',
        'metrics/ui/screen_info_metrics_provider.h',
      ],
    },
    {
      # Protobuf compiler / generator for UMA (User Metrics Analysis).
      #
      # GN version: //components/metrics/proto:proto
      'target_name': 'component_metrics_proto',
      'type': 'static_library',
      'sources': [
        'metrics/proto/call_stack_profile.proto',
        'metrics/proto/cast_logs.proto',
        'metrics/proto/chrome_user_metrics_extension.proto',
        'metrics/proto/histogram_event.proto',
        'metrics/proto/memory_leak_report.proto',
        'metrics/proto/omnibox_event.proto',
        'metrics/proto/omnibox_input_type.proto',
        'metrics/proto/perf_data.proto',
        'metrics/proto/perf_stat.proto',
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
      # GN version: //components/metrics:test_support
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
        'metrics/test_enabled_state_provider.cc',
        'metrics/test_enabled_state_provider.h',
        'metrics/test_metrics_provider.cc',
        'metrics/test_metrics_provider.h',
        'metrics/test_metrics_service_client.cc',
        'metrics/test_metrics_service_client.h',
      ],
    },
    {
      # GN version: //components/metrics:profiler
      'target_name': 'metrics_profiler',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'component_metrics_proto',
        'metrics',
        'variations',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/profiler/profiler_metrics_provider.cc',
        'metrics/profiler/profiler_metrics_provider.h',
        'metrics/profiler/tracking_synchronizer.cc',
        'metrics/profiler/tracking_synchronizer.h',
        'metrics/profiler/tracking_synchronizer_delegate.h',
        'metrics/profiler/tracking_synchronizer_observer.cc',
        'metrics/profiler/tracking_synchronizer_observer.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'metrics_serialization',
          'type': 'static_library',
          'sources': [
            'metrics/serialization/metric_sample.cc',
            'metrics/serialization/metric_sample.h',
            'metrics/serialization/serialization_utils.cc',
            'metrics/serialization/serialization_utils.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
        },
      ],
    }],
    ['chromeos==1', {
      'targets': [
        {
          # GN version: //components/metrics:leak_detector
          'target_name': 'metrics_leak_detector',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            'component_metrics_proto',
          ],
          'sources': [
            'metrics/leak_detector/call_stack_manager.cc',
            'metrics/leak_detector/call_stack_manager.h',
            'metrics/leak_detector/call_stack_table.cc',
            'metrics/leak_detector/call_stack_table.h',
            'metrics/leak_detector/custom_allocator.cc',
            'metrics/leak_detector/custom_allocator.h',
            'metrics/leak_detector/leak_analyzer.cc',
            'metrics/leak_detector/leak_analyzer.h',
            'metrics/leak_detector/leak_detector.cc',
            'metrics/leak_detector/leak_detector.h',
            'metrics/leak_detector/leak_detector_impl.cc',
            'metrics/leak_detector/leak_detector_impl.h',
            'metrics/leak_detector/leak_detector_value_type.cc',
            'metrics/leak_detector/leak_detector_value_type.h',
            'metrics/leak_detector/ranked_set.cc',
            'metrics/leak_detector/ranked_set.h',
          ],
        },
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/metrics:gpu
          'target_name': 'metrics_gpu',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            'component_metrics_proto',
            'metrics',
          ],
          'sources': [
            'metrics/gpu/gpu_metrics_provider.cc',
            'metrics/gpu/gpu_metrics_provider.h',
          ],
        },
        {
          # GN version: //components/metrics:profiler_content
          'target_name': 'metrics_profiler_content',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            'metrics_profiler',
          ],
          'sources': [
            'metrics/profiler/content/content_tracking_synchronizer_delegate.cc',
            'metrics/profiler/content/content_tracking_synchronizer_delegate.h',
          ],
        },
      ],
    }, {  # OS==ios
      'targets': [
        {
          # GN version: //components/metrics:profiler_ios
          'target_name': 'metrics_profiler_ios',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'metrics_profiler',
          ],
          'sources': [
            'metrics/profiler/ios/ios_tracking_synchronizer_delegate.cc',
            'metrics/profiler/ios/ios_tracking_synchronizer_delegate.h',
          ],
        },
      ],
    }],
  ],
}
