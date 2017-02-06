# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/web_resource
      'target_name': 'web_resource',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/base/ui_base.gyp:ui_base',
        '../net/net.gyp:net',
        'pref_registry',
        'version_info',
      ],
      'sources': [
        'web_resource/eula_accepted_notifier.cc',
        'web_resource/eula_accepted_notifier.h',
        'web_resource/promo_resource_service.cc',
        'web_resource/promo_resource_service.h',
        'web_resource/resource_request_allowed_notifier.cc',
        'web_resource/resource_request_allowed_notifier.h',
        'web_resource/web_resource_pref_names.cc',
        'web_resource/web_resource_pref_names.h',
        'web_resource/web_resource_service.cc',
        'web_resource/web_resource_service.h',
      ],
    },
    {
      # GN version: //components/web_resources:test_support
      'target_name': 'web_resource_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'web_resource',
      ],
      'sources': [
        'web_resource/resource_request_allowed_notifier_test_util.cc',
        'web_resource/resource_request_allowed_notifier_test_util.h',
      ],
    },
  ],
}
