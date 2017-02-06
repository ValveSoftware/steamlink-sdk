# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/captive_portal
      'target_name': 'captive_portal',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'CAPTIVE_PORTAL_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'captive_portal/captive_portal_detector.cc',
        'captive_portal/captive_portal_detector.h',
        'captive_portal/captive_portal_export.h',
        'captive_portal/captive_portal_types.cc',
        'captive_portal/captive_portal_types.h',
      ],
    },
    {
      # GN version: //components/captive_portal:test_support
      'target_name': 'captive_portal_test_support',
      'type': 'static_library',
      'dependencies': [
        'captive_portal',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'captive_portal/captive_portal_testing_utils.cc',
        'captive_portal/captive_portal_testing_utils.h',
      ],
    },
  ],
}
