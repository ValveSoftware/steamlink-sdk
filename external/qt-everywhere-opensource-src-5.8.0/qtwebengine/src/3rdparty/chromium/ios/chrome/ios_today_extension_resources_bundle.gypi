# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'dependencies': [
    '<(DEPTH)/ios/chrome/ios_today_extension_resources.gyp:ios_today_extension_packed_resources',
  ],
  'mac_bundle_resources': [
    '<!@pymod_do_main(ios_repack_extension_locales -n today_extension -o '
      '-s <(SHARED_INTERMEDIATE_DIR) '
      '-x <(SHARED_INTERMEDIATE_DIR)/repack_today_extension <(locales))',
  ],
}
