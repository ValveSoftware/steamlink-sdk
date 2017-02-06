# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //mojo
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'mojo_base.gyp:mojo_base',
        'mojo_public.gyp:mojo_public',
      ],
    },
  ]
}
