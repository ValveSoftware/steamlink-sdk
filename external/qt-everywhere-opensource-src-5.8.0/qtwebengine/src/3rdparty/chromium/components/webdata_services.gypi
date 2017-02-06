# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/webdata_services
      'target_name': 'webdata_services',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../sql/sql.gyp:sql',
        '../sync/sync.gyp:sync',
        'autofill_core_browser',
        'keyed_service_core',
        'password_manager_core_browser',
        'search_engines',
        'signin_core_browser',
        'webdata_common',
      ],
      'sources': [
        'webdata_services/web_data_service_wrapper.cc',
        'webdata_services/web_data_service_wrapper.h',
      ],
    },
    {
      # GN version: //components/webdata_services:test_support
      'target_name': 'webdata_services_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'autofill_core_browser',
        'signin_core_browser',
        'webdata_services',
      ],
      'sources': [
        'webdata_services/web_data_service_test_util.cc',
        'webdata_services/web_data_service_test_util.h',
      ],
    },
  ],
}
