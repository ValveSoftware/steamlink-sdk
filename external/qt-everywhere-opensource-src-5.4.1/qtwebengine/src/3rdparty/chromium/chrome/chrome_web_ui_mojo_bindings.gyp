# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN version: //chrome/browser/ui/webui/omnibox:mojo_bindings
      'target_name': 'web_ui_mojo_bindings',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files be listed explicitly in browser_ui.
      'type': 'none',
      'sources': [
        'browser/ui/webui/omnibox/omnibox.mojom',
      ],
      'includes': [ '../mojo/public/tools/bindings/mojom_bindings_generator.gypi' ],
    },
  ],
}
