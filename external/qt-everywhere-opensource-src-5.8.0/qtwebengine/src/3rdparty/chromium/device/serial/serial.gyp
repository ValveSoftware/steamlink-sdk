# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/serial:serial_mojo
      'target_name': 'device_serial_mojo',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files must be listed explicitly in device_serial
      'type': 'none',
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'data_stream.mojom',
        'data_stream_serialization.mojom',
        'serial.mojom',
        'serial_serialization.mojom',
      ],
    },
    {
      # GN version: //device/serial
      'target_name': 'device_serial',
      'type': 'static_library',
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
            '../../dbus/dbus.gyp:dbus',
          ],
        }],
        ['use_udev == 1', {
          'dependencies': [
            '../udev_linux/udev.gyp:udev_linux',
          ],
        }, {
          'sources!': [
            'serial_device_enumerator_linux.cc',
            'serial_device_enumerator_linux.h',
          ],
        }],
        ['OS=="win"', {
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'setupapi.dll',
                ],
                'AdditionalDependencies': [
                  'setupapi.lib',
                ],
              },
            },
          },
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'setupapi.dll',
                ],
                'AdditionalDependencies': [
                  'setupapi.lib',
                ],
              },
            },
          },
        }],
      ],
      'dependencies': [
        'device_serial_mojo',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../net/net.gyp:net',
        '../../third_party/re2/re2.gyp:re2',
        '../core/core.gyp:device_core',
      ],
      'export_dependent_settings': [
        'device_serial_mojo',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/data_stream.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/data_stream.mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/data_stream_serialization.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/data_stream_serialization.mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial.mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial_serialization.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/device/serial/serial_serialization.mojom.h',
        'buffer.cc',
        'buffer.h',
        'data_receiver.cc',
        'data_receiver.h',
        'data_sender.cc',
        'data_sender.h',
        'data_sink_receiver.cc',
        'data_sink_receiver.h',
        'data_source_sender.cc',
        'data_source_sender.h',
        'serial_connection.cc',
        'serial_connection.h',
        'serial_connection_factory.cc',
        'serial_connection_factory.h',
        'serial_device_enumerator.cc',
        'serial_device_enumerator.h',
        'serial_device_enumerator_linux.cc',
        'serial_device_enumerator_linux.h',
        'serial_device_enumerator_mac.cc',
        'serial_device_enumerator_mac.h',
        'serial_device_enumerator_win.cc',
        'serial_device_enumerator_win.h',
        'serial_io_handler.cc',
        'serial_io_handler.h',
        'serial_io_handler_posix.cc',
        'serial_io_handler_posix.h',
        'serial_io_handler_win.cc',
        'serial_io_handler_win.h',
        'serial_service_impl.cc',
        'serial_service_impl.h',
      ],
    },
    {
      # GN version: //device/serial:test_support
      'target_name': 'device_serial_test_util',
      'type': 'static_library',
      'dependencies': [
        'device_serial',
        'device_serial_mojo',
      ],
      'sources': [
        'test_serial_io_handler.cc',
        'test_serial_io_handler.h',
      ],
    },
  ],
}
