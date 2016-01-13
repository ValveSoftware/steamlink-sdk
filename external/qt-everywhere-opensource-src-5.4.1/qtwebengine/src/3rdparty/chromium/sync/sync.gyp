# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'includes': [
    'sync_android.gypi',
    'sync_tests.gypi',
  ],

  'conditions': [
    # Notes:
    # 1) In static mode, the public 'sync' target has a target type of 'none',
    #    and is composed of the static library targets 'sync_api', 'sync_core',
    #    'sync_internal_api', 'sync_notifier', and 'sync_proto'.
    # 2) In component mode, we build the public 'sync' target into a single DLL,
    #    which includes the contents of sync_api.gypi, sync_core.gypi,
    #    sync_internal_api.gypi, sync_notifier.gypi, and sync_proto.gypi.
    # 3) All external targets that depend on anything in sync/ must simply
    #    declare a dependency on 'sync.gyp:sync'
    ['component=="static_library"', {
      'targets': [
        # The public sync static library target.
        {
          'target_name': 'sync',
          'type': 'none',
          'dependencies': [
            'sync_api',
            'sync_core',
            'sync_internal_api',
            'sync_notifier',
            'sync_proto',
          ],
          'export_dependent_settings': [
            'sync_notifier',
            'sync_proto',
          ],
        },

        # The sync external API library.
        {
          'target_name': 'sync_api',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_api.gypi',
          ],
          'dependencies': [
            'sync_internal_api',
            'sync_proto',
          ],
        },

        # The core sync library.
        {
          'target_name': 'sync_core',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_core.gypi',
          ],
          'dependencies': [
            'sync_notifier',
            'sync_proto',
          ],
          'export_dependent_settings': [
            'sync_notifier',
            'sync_proto',
          ],
        },

        # The sync internal API library.
        {
          'target_name': 'sync_internal_api',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_internal_api.gypi',
          ],
          'dependencies': [
            'sync_core',
            'sync_notifier',
            'sync_proto',
          ],
          'export_dependent_settings': [
            'sync_core',
            'sync_proto',
          ],
        },

        # The sync notifications library.
        {
          'target_name': 'sync_notifier',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_notifier.gypi',
          ],
        },

        # The sync protocol buffer library.
        {
          # GN version: //sync/protocol
          'target_name': 'sync_proto',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_proto.gypi',
          ],
        },
      ],
    },
    {  # component != static_library
      'targets': [
        # The public sync shared library target.
        {
          'target_name': 'sync',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'sync_api.gypi',
            'sync_core.gypi',
            'sync_internal_api.gypi',
            'sync_notifier.gypi',
            'sync_proto.gypi',
          ],
        },
      ],
    }],
  ],
}
