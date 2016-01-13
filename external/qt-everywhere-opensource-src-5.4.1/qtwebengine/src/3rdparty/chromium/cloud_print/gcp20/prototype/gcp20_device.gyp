# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
      'enable_wexit_time_destructors': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
      # To allow including "version.h"
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
  },
  'targets': [
    {
      'target_name': 'gcp20_device_lib',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/cloud_print/cloud_print_resources.gyp:cloud_print_version_header', 
        '<(DEPTH)/google_apis/google_apis.gyp:google_apis',
        '<(DEPTH)/jingle/jingle.gyp:notifier',
        '<(DEPTH)/net/net.gyp:http_server',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
      'sources': [
        'cloud_print_response_parser.cc',        
        'cloud_print_response_parser.h',
        'cloud_print_request.cc',
        'cloud_print_request.h',
        'cloud_print_requester.cc',
        'cloud_print_requester.h',
        'cloud_print_url_request_context_getter.cc',
        'cloud_print_url_request_context_getter.h',
        'cloud_print_xmpp_listener.cc',
        'cloud_print_xmpp_listener.h',
        'conio_posix.cc',  
        'conio_posix.h',  
        'command_line_reader.cc',
        'command_line_reader.h',
        'dns_packet_parser.cc',
        'dns_packet_parser.h',
        'dns_response_builder.cc',
        'dns_response_builder.h',
        'dns_sd_server.cc',
        'dns_sd_server.h',
        'local_settings.h',
        'local_print_job.cc',
        'local_print_job.h',
        'print_job_handler.cc',
        'print_job_handler.h',
        'printer_state.cc',
        'printer_state.h',
        'printer.cc',
        'printer.h',
        'privet_http_server.cc',
        'privet_http_server.h',
        'service_parameters.cc',
        'service_parameters.h',
        'special_io.h',
        'x_privet_token.cc',
        'x_privet_token.h',
      ],
    },
    {
      'target_name': 'gcp20_device',
      'type': 'executable',
      'dependencies': [
        'gcp20_device_lib',
      ],
      'sources': [
        'gcp20_device.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'AdditionalDependencies': [
# TODO(maksymb): Check which of whis libs is needed.
            'secur32.lib',
            'httpapi.lib',
            'Ws2_32.lib',
          ],
        },
      },
    },
    {
      'target_name': 'gcp20_device_unittests',
      'type': 'executable',
      'sources': [
        'printer_unittest.cc',
        'x_privet_token_unittest.cc',
      ],
      'dependencies': [
        'gcp20_device_lib',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'AdditionalDependencies': [
            'secur32.lib',
          ],
        },
      },
    },
  ],
}
