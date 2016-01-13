# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'metro_viewer_constants',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'viewer/metro_viewer_constants.cc',
        'viewer/metro_viewer_constants.h',
      ],
    },
    {
      'target_name': 'metro_viewer',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../ui/aura/aura.gyp:aura',
        '../ui/metro_viewer/metro_viewer.gyp:metro_viewer_messages',
        'metro_viewer_constants'
      ],
      'sources': [
        'viewer/metro_viewer_process_host.cc',
        'viewer/metro_viewer_process_host.h',
      ],
      'defines': [
        'METRO_VIEWER_IMPLEMENTATION',
      ],
    },
    {
      'target_name': 'test_support_win8',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'test_registrar_constants',
      ],
      'sources': [
        'test/metro_registration_helper.cc',
        'test/metro_registration_helper.h',
        'test/open_with_dialog_async.cc',
        'test/open_with_dialog_async.h',
        'test/open_with_dialog_controller.cc',
        'test/open_with_dialog_controller.h',
        'test/ui_automation_client.cc',
        'test/ui_automation_client.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'test_registrar_constants',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/test_registrar_constants.cc',
        'test/test_registrar_constants.h',
      ],
    },
  ],
}
