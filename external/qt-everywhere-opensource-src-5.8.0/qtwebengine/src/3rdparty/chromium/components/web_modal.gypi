# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/web_modal
      'target_name': 'web_modal',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'web_modal/modal_dialog_host.cc',
        'web_modal/modal_dialog_host.h',
        'web_modal/single_web_contents_dialog_manager.h',
        'web_modal/web_contents_modal_dialog_host.cc',
        'web_modal/web_contents_modal_dialog_host.h',
        'web_modal/web_contents_modal_dialog_manager.cc',
        'web_modal/web_contents_modal_dialog_manager.h',
        'web_modal/web_contents_modal_dialog_manager_delegate.cc',
        'web_modal/web_contents_modal_dialog_manager_delegate.h',
      ],
    },
    {
      # GN version: //components/web_modal:test_support
      'target_name': 'web_modal_test_support',
      'type': 'static_library',
      'dependencies': [
        'web_modal',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'web_modal/test_web_contents_modal_dialog_host.cc',
        'web_modal/test_web_contents_modal_dialog_host.h',
        'web_modal/test_web_contents_modal_dialog_manager_delegate.cc',
        'web_modal/test_web_contents_modal_dialog_manager_delegate.h',
      ],
    },
  ],
}
