# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'mojom_files': [
      'public/interfaces/chooser_service.mojom',
      'public/interfaces/device.mojom',
      'public/interfaces/device_manager.mojom',
    ],
  },
  'targets': [
    {
      'target_name': 'device_usb',
      'type': 'static_library',
      'dependencies': [
        'device_usb_mojo_bindings',
        '../../components/components.gyp:device_event_log_component',
        '../../net/net.gyp:net',
        '../core/core.gyp:device_core',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'android/usb_jni_registrar.cc',
        'android/usb_jni_registrar.h',
        'mojo/device_impl.cc',
        'mojo/device_impl.h',
        'mojo/device_manager_impl.cc',
        'mojo/device_manager_impl.h',
        'mojo/type_converters.cc',
        'mojo/type_converters.h',
        'mojo/permission_provider.cc',
        'mojo/permission_provider.h',
        'usb_configuration_android.cc',
        'usb_configuration_android.h',
        'usb_descriptors.cc',
        'usb_descriptors.h',
        'usb_device_linux.cc',
        'usb_device_linux.h',
        'usb_device.cc',
        'usb_device.h',
        'usb_device_android.cc',
        'usb_device_android.h',
        'usb_device_filter.cc',
        'usb_device_filter.h',
        'usb_device_handle.cc',
        'usb_device_handle.h',
        'usb_endpoint_android.cc',
        'usb_endpoint_android.h',
        'usb_ids.cc',
        'usb_ids.h',
        'usb_interface_android.cc',
        'usb_interface_android.h',
        'usb_service.cc',
        'usb_service.h',
        'usb_service_android.cc',
        'usb_service_android.h',
        'webusb_descriptors.cc',
        'webusb_descriptors.h',
      ],
      'actions': [
        {
          'action_name': 'generate_usb_ids',
          'variables': {
            'usb_ids_path%': '../../third_party/usb_ids/usb.ids',
            'usb_ids_py_path': 'tools/usb_ids.py',
          },
          'inputs': [
            '<(usb_ids_path)',
            '<(usb_ids_py_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/device/usb/usb_ids_gen.cc',
          ],
          'action': [
            'python',
            '<(usb_ids_py_path)',
            '-i', '<(usb_ids_path)',
            '-o', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
      'conditions': [
        ['use_udev == 1', {
          'sources': [
            'usb_service_linux.cc',
            'usb_service_linux.h',
          ],
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'device_usb_java',
            'device_usb_jni_headers',
          ],
        }],
        ['OS=="win" or OS=="mac"', {
          'dependencies': [
            '../../third_party/libusb/libusb.gyp:libusb',
          ],
          'sources': [
            'usb_context.cc',
            'usb_context.h',
            'usb_device_handle_impl.cc',
            'usb_device_handle_impl.h',
            'usb_device_impl.cc',
            'usb_device_impl.h',
            'usb_error.cc',
            'usb_error.h',
            'usb_service_impl.cc',
            'usb_service_impl.h',
          ],
        }],
        ['OS=="linux" or OS=="android"', {
          'sources': [
            'usb_device_handle_usbfs.cc',
            'usb_device_handle_usbfs.h',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
          ],
        }]
      ]
    },
    {
      'target_name': 'device_usb_mojo_bindings',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
    },
    {
      'target_name': 'device_usb_mojo_bindings_for_blink',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'variables': {
        'for_blink': 'true',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
    },
    {
      'target_name': 'device_usb_mocks',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_usb',
      ],
      'sources': [
        'mock_usb_device.cc',
        'mock_usb_device.h',
        'mock_usb_device_handle.cc',
        'mock_usb_device_handle.h',
        'mock_usb_service.cc',
        'mock_usb_service.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_usb_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/device/usb/ChromeUsbConfiguration.java',
            'android/java/src/org/chromium/device/usb/ChromeUsbDevice.java',
            'android/java/src/org/chromium/device/usb/ChromeUsbEndpoint.java',
            'android/java/src/org/chromium/device/usb/ChromeUsbInterface.java',
            'android/java/src/org/chromium/device/usb/ChromeUsbService.java',
          ],
          'variables': {
            'jni_gen_package': 'device_usb',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'device_usb_java',
          'type': 'none',
          'dependencies': [
            'usb_descriptors_javagen',
            '../../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': '../../device/usb/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'usb_descriptors_javagen',
          'type': 'none',
          'variables': {
            'source_file': 'usb_descriptors.h',
          },
          'includes': [ '../../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}
