# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': 'shared_library',
  'android_unmangled_name': 1,
  'dependencies': [
    'android_webview_common',
  ],
  'variables': {
    'use_native_jni_exports': 1,
  },
  'sources': [
    'lib/main/webview_entry_point.cc',
  ],
}
