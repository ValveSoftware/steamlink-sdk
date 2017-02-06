# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'web_ui_mojo_bindings',
      # The type of this target must be none. This is so that resources can
      # depend upon this target for generating the js bindings files. Any
      # generated cpp files be listed explicitly in browser_ui.
      'type': 'none',
      'sources': [
        # GN version: //chrome/browser/ui/webui/engagement:mojo_bindings
        'browser/ui/webui/engagement/site_engagement.mojom',
        # GN version: //chrome/browser/ui/webui/omnibox:mojo_bindings
        'browser/ui/webui/omnibox/omnibox.mojom',
        # GN version: //chrome/browser/ui/webui/plugins:mojo_bindings
        'browser/ui/webui/plugins/plugins.mojom',
      ],
      'variables': {
        'mojom_typemaps': [
          '<(DEPTH)/url/mojo/gurl.typemap',
        ],
      },
      'includes': [ '../mojo/mojom_bindings_generator.gypi' ],
    },
  ],
}
