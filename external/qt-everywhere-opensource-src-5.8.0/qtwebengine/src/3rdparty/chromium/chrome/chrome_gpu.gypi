# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome/gpu
      'target_name': 'gpu',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '<(grit_out_dir)',
      ],
      'sources': [
        'gpu/chrome_content_gpu_client.cc',
        'gpu/chrome_content_gpu_client.h',
      ],
      'dependencies': [
        '../content/content.gyp:content_common',
        '../content/content.gyp:content_gpu',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../components/components.gyp:arc_mojo_bindings',
          ],
          'sources': [
            'gpu/arc_gpu_video_decode_accelerator.cc',
            'gpu/arc_gpu_video_decode_accelerator.h',
            'gpu/arc_video_accelerator.h',
            'gpu/gpu_arc_video_service.cc',
            'gpu/gpu_arc_video_service.h',
          ],
        }],
      ],
    },
  ],
}
