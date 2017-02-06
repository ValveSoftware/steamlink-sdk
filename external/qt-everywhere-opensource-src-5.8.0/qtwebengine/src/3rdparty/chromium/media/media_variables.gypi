# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Common media variables.
{
  'variables': {
    'conditions': [
      ['OS == "ios"', {
        # iOS doesn't use FFmpeg or libvpx by default.
        'media_use_ffmpeg%': 0,
        'media_use_libvpx%': 0,
      }, {
        'media_use_ffmpeg%': 1,
        'media_use_libvpx%': 1,
      }],
    ],
  },
}
