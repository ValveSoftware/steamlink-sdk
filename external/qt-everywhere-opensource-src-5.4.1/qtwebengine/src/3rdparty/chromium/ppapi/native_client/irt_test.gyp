# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'variables': {
        'irt_test_nexe': '',
        'irt_test_dep': 'native_client.gyp:nacl_irt',
      },
      'includes': [
        '../../native_client/build/common.gypi',
        '../../native_client/src/untrusted/irt/check_tls.gypi',
      ],
    }],
  ],
}
