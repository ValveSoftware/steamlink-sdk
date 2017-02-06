# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/client_update_protocol
      'target_name': 'client_update_protocol',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],

      'include_dirs': [
        '..',
      ],
      'sources': [
        'client_update_protocol/ecdsa.h',
        'client_update_protocol/ecdsa.cc',
      ],
    },
    {
      # GN version: //components/client_update_protocol:test_support
      'target_name': 'client_update_protocol_test_support',
      'type': 'static_library',
      'dependencies': [
        'update_client',
        '../testing/gtest.gyp:gtest',
      ],

      'include_dirs': [
        '..',
      ],
      'sources': [
        'client_update_protocol/ecdsa_unittest.cc',
      ],
    },
  ],
}
