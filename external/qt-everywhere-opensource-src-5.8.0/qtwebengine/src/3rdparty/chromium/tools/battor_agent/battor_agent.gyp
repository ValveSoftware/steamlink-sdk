# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'battor_agent',
      'type': 'executable',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        'battor_agent_lib',
        '../../device/serial/serial.gyp:device_serial',
        '../../device/serial/serial.gyp:device_serial_mojo',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'battor_agent_bin.cc',
      ],
    },
    {
      'target_name': 'battor_agent_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'battor_agent.cc',
        'battor_agent.h',
        'battor_connection.cc',
        'battor_connection.h',
        'battor_connection_impl.cc',
        'battor_connection_impl.h',
        'battor_error.cc',
        'battor_error.h',
        'battor_finder.cc',
        'battor_finder.h',
        'battor_sample_converter.cc',
        'battor_sample_converter.h',
        'serial_utils.cc',
        'serial_utils.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../device/serial/serial.gyp:device_serial',
        '../../device/serial/serial.gyp:device_serial_mojo',
      ]
    },
    {
      'target_name': 'battor_agent_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'battor_agent_lib',
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../device/serial/serial.gyp:device_serial',
        '../../device/serial/serial.gyp:device_serial_test_util',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
      	'../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
	'battor_agent_unittest.cc',
        'battor_connection_impl_unittest.cc',
        'battor_protocol_types_unittest.cc',
        'battor_sample_converter_unittest.cc',
        'serial_utils_unittest.cc',        
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'battor_agent_unittests_run',
          'type': 'none',
          'dependencies': [
            'battor_agent_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'battor_agent_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
