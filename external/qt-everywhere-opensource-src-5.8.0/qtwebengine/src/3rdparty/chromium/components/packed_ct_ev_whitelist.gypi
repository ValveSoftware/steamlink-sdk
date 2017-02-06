# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/packed_ct_ev_whitelist
      'target_name': 'packed_ct_ev_whitelist',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../net/net.gyp:net',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'packed_ct_ev_whitelist/bit_stream_reader.cc',
        'packed_ct_ev_whitelist/bit_stream_reader.h',
        'packed_ct_ev_whitelist/packed_ct_ev_whitelist.cc',
        'packed_ct_ev_whitelist/packed_ct_ev_whitelist.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
