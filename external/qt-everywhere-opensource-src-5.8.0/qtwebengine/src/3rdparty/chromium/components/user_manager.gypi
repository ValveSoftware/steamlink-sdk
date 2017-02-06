# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Cross-platform user_manager sources.
    'user_manager_shared_sources': [
      'user_manager/empty_user_info.cc',
      'user_manager/empty_user_info.h',
      'user_manager/user_info.cc',
      'user_manager/user_info.h',
      'user_manager/user_info_impl.cc',
      'user_manager/user_info_impl.h',
      'user_manager/user_manager_export.h',
    ],
    # Chrome OS user_manager sources.
    'user_manager_chromeos_sources': [
      'user_manager/known_user.cc',
      'user_manager/known_user.h',
      'user_manager/remove_user_delegate.h',
      'user_manager/user.cc',
      'user_manager/user.h',
      'user_manager/user_image/user_image.cc',
      'user_manager/user_image/user_image.h',
      'user_manager/user_manager.cc',
      'user_manager/user_manager.h',
      'user_manager/user_manager_base.cc',
      'user_manager/user_manager_base.h',
      'user_manager/user_type.h',
    ],
  },
  'targets': [{
    'target_name': 'user_manager',
    'type': '<(component)',
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/components/components.gyp:signin_core_account_id',
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
    ],
    'defines': [
      'USER_MANAGER_IMPLEMENTATION',
    ],
    'include_dirs': [
      '<(DEPTH)',
      '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
      '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
    ],
    'sources': [ '<@(user_manager_shared_sources)' ],
    'conditions': [
      ['chromeos == 1', {
        'dependencies': [
          '<(DEPTH)/components/components.gyp:session_manager_component',
          '<(DEPTH)/components/prefs/prefs.gyp:prefs',
          '<(DEPTH)/google_apis/google_apis.gyp:google_apis',
          '<(DEPTH)/url/url.gyp:url_lib',
        ],
        'sources': [ '<@(user_manager_chromeos_sources)' ],
      }],
    ],
  },
  {
    # GN version: //components/user_manager:test_support
    'target_name': 'user_manager_test_support',
    'type': 'static_library',
    'conditions': [
      ['chromeos == 1', {
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base',
          '<(DEPTH)/base/base.gyp:test_support_base',
          '<(DEPTH)/skia/skia.gyp:skia',
          '<(DEPTH)/testing/gmock.gyp:gmock',
          '<(DEPTH)/testing/gtest.gyp:gtest',
          'user_manager',
        ],
        'sources': [
          'user_manager/fake_user_manager.cc',
          'user_manager/fake_user_manager.h',
        ],
      }],
     ]
  },],
}
