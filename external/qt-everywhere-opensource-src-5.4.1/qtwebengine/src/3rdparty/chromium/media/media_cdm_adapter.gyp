# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines a common base target for all CDM adapter implementations.
# We use 'direct_dependent_settings' to override target type and settings so
# that all CDM adapter implementations have the correct type and settings
# automatically.
#
# WARNING: Keep 'cdmadapter' target out of media.gyp. /build/all.gyp:All depends
# directly on media.gyp:*. If 'cdmadapter' is defined in media.gyp, then
# 'direct_dependent_settings' will be applied to 'All' target and bad
# things happen, e.g. the type of 'All' target becomes a plugin on Mac.
{
  'conditions': [
    ['enable_pepper_cdms==1', {
      'targets': [
        {
          'target_name': 'cdmadapter',
          'type': 'none',
          'direct_dependent_settings': {
            'sources': [
              'cdm/ppapi/api/content_decryption_module.h',
              'cdm/ppapi/cdm_adapter.cc',
              'cdm/ppapi/cdm_adapter.h',
              'cdm/ppapi/cdm_file_io_impl.cc',
              'cdm/ppapi/cdm_file_io_impl.h',
              'cdm/ppapi/cdm_helpers.cc',
              'cdm/ppapi/cdm_helpers.h',
              'cdm/ppapi/cdm_logging.cc',
              'cdm/ppapi/cdm_logging.h',
              'cdm/ppapi/cdm_wrapper.h',
              'cdm/ppapi/linked_ptr.h',
              'cdm/ppapi/supported_cdm_versions.h',
            ],
            'conditions': [
              ['os_posix == 1 and OS != "mac"', {
                'cflags': ['-fvisibility=hidden'],
                'type': 'loadable_module',
                # Allow the adapter to find the CDM in the same directory.
                'ldflags': ['-Wl,-rpath=\$$ORIGIN'],
              }],
              ['OS == "win"', {
                'type': 'shared_library',
                # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
                'msvs_disabled_warnings': [ 4267, ],
              }],
              ['OS == "mac"', {
                'type': 'loadable_module',
                'product_extension': 'plugin',
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    # Not to strip important symbols by -Wl,-dead_strip.
                    '-Wl,-exported_symbol,_PPP_GetInterface',
                    '-Wl,-exported_symbol,_PPP_InitializeModule',
                    '-Wl,-exported_symbol,_PPP_ShutdownModule'
                  ],
                  'DYLIB_INSTALL_NAME_BASE': '@loader_path',
                },
              }],
            ],
          },
        },
      ],
    }],
  ],
}
