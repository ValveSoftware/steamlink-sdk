# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Common variables shared amongst all ChromeVox targets.

{
  'variables': {
    'chromevox_third_party_dir': '<(DEPTH)/chrome/third_party/chromevox',
    'closure_goog_dir': '<(chromevox_third_party_dir)/third_party/closure-library/closure/goog',
    'chromevox_dest_dir': '<(PRODUCT_DIR)/resources/chromeos/chromevox',
    'js_root_flags': [
      '-r', '.',
      '-r', '<(closure_goog_dir)',
      '-x', 'testing',
      '-x', '_test.js',
    ],
    'path_rewrite_flags': [
      '-w', '<(closure_goog_dir):closure',
    ],
    'template_manifest_path': 'manifest.json.jinja2',
  },
}
