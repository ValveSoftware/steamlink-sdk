# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //tools/cygprofile
      'target_name': 'cygprofile',
      'type': 'static_library',
      'include_dirs': [ '../..', ],
      'sources': [
        'cygprofile.cc',
        'cygprofile.h',
      ],
      'cflags!': [ '-finstrument-functions' ],
      'dependencies': [
        # This adds uninstrumented symbols to the static library from base.
        # These symbols are likely *not* to be used because there are many
        # other duplicates in other objects/libraries.
        '../../base/base.gyp:base',
      ],
    },
    {
      # GN: //tools/cygprofile:cygprofile_unittests
      'target_name': 'cygprofile_unittests',
      'type': 'executable',
      'include_dirs': [ '../..', ],
      'sources': [
        'cygprofile_unittest.cc',
      ],
      'cflags!': [ '-finstrument-functions' ],
      # TODO(lizeb,pasko): Fix the underlying problem (crbug/485542)
      'ldflags': [ '-Wl,--no-fatal-warnings' ],
      'dependencies': [
        'cygprofile',
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
      ],
    },
  ],
}
