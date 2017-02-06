# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'dependencies': [
    '<(DEPTH)/ios/chrome/ios_chrome_resources.gyp:ios_packed_resources',
  ],
  'mac_bundle_resources': [
    '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_100_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_200_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_300_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack_ios/resources.pak',
    '<!@pymod_do_main(ios_repack_locales -o -s <(SHARED_INTERMEDIATE_DIR) '
      '-x <(SHARED_INTERMEDIATE_DIR)/repack_ios <(locales))',
  ],
}
