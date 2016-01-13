# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'rappor',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../third_party/smhasher/smhasher.gyp:cityhash',
        'metrics',
        'variations',
      ],
      'sources': [
        'rappor/bloom_filter.cc',
        'rappor/bloom_filter.h',
        'rappor/byte_vector_utils.cc',
        'rappor/byte_vector_utils.h',
        'rappor/log_uploader.cc',
        'rappor/log_uploader.h',
        'rappor/proto/rappor_metric.proto',
        'rappor/rappor_metric.cc',
        'rappor/rappor_metric.h',
        'rappor/rappor_parameters.cc',
        'rappor/rappor_parameters.h',
        'rappor/rappor_pref_names.cc',
        'rappor/rappor_pref_names.h',
        'rappor/rappor_service.cc',
        'rappor/rappor_service.h',
      ],
      'variables': {
        'proto_in_dir': 'rappor/proto',
        'proto_out_dir': 'components/rappor/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
}
