# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
  {
    # GN: //sync/tools:common
    'target_name': 'sync_tools_helper',
    'type': 'static_library',
    'include_dirs': [
      '../..',
    ],
    'dependencies': [
      '../../base/base.gyp:base',
      '../../components/components.gyp:invalidation_impl',
      '../sync.gyp:sync',
    ],
    'export_dependent_settings': [
      '../../base/base.gyp:base',
      '../sync.gyp:sync',
    ],
    'sources': [
      'null_invalidation_state_tracker.cc',
      'null_invalidation_state_tracker.h',
    ],
  },
  # A tool to listen to sync notifications and print them out.
  {
    # GN: //sync/tools:sync_listen_notifications
    'target_name': 'sync_listen_notifications',
    'type': 'executable',
    'defines': [
      'SYNC_TEST',
    ],
    'dependencies': [
      '../../base/base.gyp:base',
      '../../components/components.gyp:invalidation_impl',
      '../../components/components.gyp:sync_driver',
      '../../jingle/jingle.gyp:notifier',
      '../../net/net.gyp:net',
      '../../net/net.gyp:net_test_support',
      '../sync.gyp:sync',
      'sync_tools_helper',
    ],
    'sources': [
      'sync_listen_notifications.cc',
    ],
  },

  # A standalone command-line sync client.
  {
    # GN: //sync/tools:sync_client
    'target_name': 'sync_client',
    'type': 'executable',
    'defines': [
      'SYNC_TEST',
    ],
    'dependencies': [
      '../../base/base.gyp:base',
      '../../components/components.gyp:invalidation_impl',
      '../../components/components.gyp:sync_driver',
      '../../jingle/jingle.gyp:notifier',
      '../../net/net.gyp:net',
      '../../net/net.gyp:net_test_support',
      '../sync.gyp:sync',
      '../sync.gyp:test_support_sync_core',
      'sync_tools_helper',
    ],
    'sources': [
      'sync_client.cc',
    ],
  },
  ]
}

