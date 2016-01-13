# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'dependencies': [
    '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
    '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
  ],
  'mac_bundle_resources': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
    '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))',
  ],
}
