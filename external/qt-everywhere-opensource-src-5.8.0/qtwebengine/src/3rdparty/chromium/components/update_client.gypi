# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/update_client
      'target_name': 'update_client',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../courgette/courgette.gyp:courgette_lib',
        '../crypto/crypto.gyp:crypto',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/zlib/google/zip.gyp:zip',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'client_update_protocol',
        'crx_file',
      ],

      'include_dirs': [
        '..',
      ],
      'sources': [
        'update_client/action.cc',
        'update_client/action.h',
        'update_client/action_update.cc',
        'update_client/action_update.h',
        'update_client/action_update_check.cc',
        'update_client/action_update_check.h',
        'update_client/action_wait.cc',
        'update_client/action_wait.h',
        'update_client/background_downloader_win.cc',
        'update_client/background_downloader_win.h',
        'update_client/component_patcher.cc',
        'update_client/component_patcher.h',
        'update_client/component_patcher_operation.cc',
        'update_client/component_patcher_operation.h',
        'update_client/component_unpacker.cc',
        'update_client/component_unpacker.h',
        'update_client/configurator.h',
        'update_client/crx_downloader.cc',
        'update_client/crx_downloader.h',
        'update_client/crx_update_item.h',
        'update_client/persisted_data.cc',
        'update_client/persisted_data.h',
        'update_client/ping_manager.cc',
        'update_client/ping_manager.h',
        'update_client/request_sender.cc',
        'update_client/request_sender.h',
        'update_client/task.h',
        'update_client/task_update.cc',
        'update_client/task_update.h',
        'update_client/update_checker.cc',
        'update_client/update_checker.h',
        'update_client/update_client.cc',
        'update_client/update_client.h',
        'update_client/update_client_internal.h',
        'update_client/update_engine.cc',
        'update_client/update_engine.h',
        'update_client/update_query_params.cc',
        'update_client/update_query_params.h',
        'update_client/update_query_params_delegate.cc',
        'update_client/update_query_params_delegate.h',
        'update_client/update_response.cc',
        'update_client/update_response.h',
        'update_client/url_fetcher_downloader.cc',
        'update_client/url_fetcher_downloader.h',
        'update_client/utils.cc',
        'update_client/utils.h',
      ],
    },
    {
      # GN version: //components/update_client:test_support
      'target_name': 'update_client_test_support',
      'type': 'static_library',
      'dependencies': [
        'update_client',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],

      'include_dirs': [
        '..',
      ],
      'sources': [
        'update_client/test_configurator.cc',
        'update_client/test_configurator.h',
        'update_client/test_installer.cc',
        'update_client/test_installer.h',
        'update_client/url_request_post_interceptor.cc',
        'update_client/url_request_post_interceptor.h',
      ],
    },
  ],
}
