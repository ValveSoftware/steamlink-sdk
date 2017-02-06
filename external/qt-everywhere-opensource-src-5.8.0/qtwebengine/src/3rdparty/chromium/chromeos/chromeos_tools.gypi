# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets' : [
    {
      'target_name': 'onc_validator',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'chromeos.gyp:chromeos',
      ],
      'sources': [
        'tools/onc_validator/onc_validator.cc',
      ],
    },
  ],
}
