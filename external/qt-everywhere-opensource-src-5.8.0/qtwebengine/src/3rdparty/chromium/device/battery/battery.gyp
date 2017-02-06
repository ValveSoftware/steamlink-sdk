# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/battery:mojo_bindings
      'target_name': 'device_battery_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'battery_monitor.mojom',
        'battery_status.mojom',
      ],
    },
    {
      # GN version: //device/battery:mojo_bindings_blink
      'target_name': 'device_battery_mojo_bindings_for_blink',
      'type': 'static_library',
      'variables': {
        'for_blink': 'true',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'battery_monitor.mojom',
        'battery_status.mojom',
      ],
    },
    {
      # This is needed only for a build within Android tree. TODO(ppi): remove
      # when Android tree build is deprecated.
      'target_name': 'device_battery_mojo_bindings_for_webview',
      'type': 'none',
      'dependencies': [
        'device_battery_mojo_bindings',
      ],
      'actions': [
        {
          # Dummy action that triggers the bindings generation and explicitly
          # declares the java outputs, so that they are discoverable to make.
          'action_name': 'device_battery_mojo_bindings_dummy_action',
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/device/battery/battery_monitor.mojom.h',
            '<(SHARED_INTERMEDIATE_DIR)/device/battery/battery_status.mojom.h',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/java_mojo/device_battery_mojo_bindings/src/org/chromium/mojom/device/BatteryMonitor.java',
            '<(SHARED_INTERMEDIATE_DIR)/java_mojo/device_battery_mojo_bindings/src/org/chromium/mojom/device/BatteryMonitor_Internal.java',
            '<(SHARED_INTERMEDIATE_DIR)/java_mojo/device_battery_mojo_bindings/src/org/chromium/mojom/device/BatteryStatusObserver.java',
            '<(SHARED_INTERMEDIATE_DIR)/java_mojo/device_battery_mojo_bindings/src/org/chromium/mojom/device/BatteryStatusObserver_Internal.java',
            '<(SHARED_INTERMEDIATE_DIR)/java_mojo/device_battery_mojo_bindings/src/org/chromium/mojom/device/BatteryStatus.java',
          ],
          'action': ['touch', '<@(_outputs)'],
        },
      ],
      'hard_dependency': 1,
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_battery_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../mojo/mojo_public.gyp:mojo_bindings_java',
            'device_battery_mojo_bindings',
          ],
          'variables': {
            'java_in_dir': '../../device/battery/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'device_battery_javatests',
          'type': 'none',
          'variables': {
            'java_in_dir': '../../device/battery/android/javatests',
          },
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:base_java_test_support',
            'device_battery_java',
          ],
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }, {  # OS != "android"
      # On android, BatteryManager mojo service is implemented directly in Java.
      'targets': [
        {
          # GN version: //device/battery
          'target_name': 'device_battery',
          'type': '<(component)',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../../mojo/mojo_edk.gyp:mojo_system_impl',
            '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
            'device_battery_mojo_bindings',
          ],
          'defines': [
            'DEVICE_BATTERY_IMPLEMENTATION',
          ],
          'sources': [
            'battery_monitor_impl.cc',
            'battery_monitor_impl.h',
            'battery_status_manager_chromeos.cc',
            'battery_status_manager_default.cc',
            'battery_status_manager_linux.cc',
            'battery_status_manager_linux.h',
            'battery_status_manager_mac.cc',
            'battery_status_manager_win.cc',
            'battery_status_manager_win.h',
            'battery_status_manager.h',
            'battery_status_service.cc',
            'battery_status_service.h',
          ],
          'conditions': [
            ['chromeos==1', {
              'dependencies': [
                '../../build/linux/system.gyp:dbus',
                '../../chromeos/chromeos.gyp:chromeos',
                '../../chromeos/chromeos.gyp:power_manager_proto',
              ],
              'sources!': [
                'battery_status_manager_default.cc',
                'battery_status_manager_linux.cc',
              ],
            }],
            ['OS == "linux" and use_dbus==1', {
              'sources!': [
                'battery_status_manager_default.cc',
              ],
              'dependencies': [
                '../../build/linux/system.gyp:dbus',
                '../../dbus/dbus.gyp:dbus',
              ],
            }, {  # OS != "linux" or use_dbus==0
              'sources!': [
                'battery_status_manager_linux.cc',
              ],
            }],
            ['OS == "mac"', {
              'sources!': [
                'battery_status_manager_default.cc',
              ],
            }],
            ['OS == "win"', {
              'sources!': [
                'battery_status_manager_default.cc',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
