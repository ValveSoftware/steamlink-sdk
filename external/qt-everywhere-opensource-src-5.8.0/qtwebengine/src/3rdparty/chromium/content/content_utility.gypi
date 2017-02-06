# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../mojo/mojo_base.gyp:mojo_common_lib',
    '../mojo/mojo_public.gyp:mojo_cpp_bindings',
    '../services/shell/shell.gyp:shell_lib',
    '../services/shell/shell_public.gyp:shell_public',
    '../third_party/WebKit/public/blink.gyp:blink',
    '../url/url.gyp:url_lib',
  ],
  'variables': {
    'utility_sources': [
      'utility/in_process_utility_thread.cc',
      'utility/in_process_utility_thread.h',
      'utility/utility_blink_platform_impl.cc',
      'utility/utility_blink_platform_impl.h',
      'utility/utility_main.cc',
      'utility/utility_process_control_impl.cc',
      'utility/utility_process_control_impl.h',
      'utility/utility_thread_impl.cc',
      'utility/utility_thread_impl.h',
      'utility/webthread_impl_for_utility_thread.cc',
      'utility/webthread_impl_for_utility_thread.h',
    ],
    'public_utility_sources': [
      'public/utility/content_utility_client.cc',
      'public/utility/content_utility_client.h',
      'public/utility/utility_thread.cc',
      'public/utility/utility_thread.h',
    ],
  },
  'include_dirs': [
    '..',
  ],
  'sources': [
    '<@(public_utility_sources)',
    '<@(utility_sources)',
  ],
}
