# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds an OS X command line tool to generate Localizable.strings and
# InfoPlist.strings from compiled locales.pak.

{
  'targets': [
    {
      'target_name': 'generate_localizable_strings',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../ui/base/ui_base.gyp:ui_data_pack',
      ],
      'sources': [
        'generate_localizable_strings.mm',
      ],
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
      ],
    },
  ],
}
