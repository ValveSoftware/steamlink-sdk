# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
    'targets': [
        {
          # Depend on this target to use public blink API headers for things
          # like enums and public structures without actually linking against any Blink
          # libraries.
          'target_name': 'blink_headers',
          'type': 'none',
          'direct_dependent_settings': {
            'include_dirs': [ '..' ],
          },
        },
    ]
}

