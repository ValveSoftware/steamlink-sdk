# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_common_mojo_bindings',
      'type': 'static_library',
      'dependencies': [
        '../mojo/mojo.gyp:mojo_cpp_bindings',
        '../mojo/mojo.gyp:mojo_environment_chromium',
      ],
      'sources': [
        'common/web_ui_setup.mojom',
      ],
      'includes': [ '../mojo/public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../mojo/mojo.gyp:mojo_cpp_bindings',
        '../mojo/mojo.gyp:mojo_environment_chromium',
      ],
    },
  ],
}
