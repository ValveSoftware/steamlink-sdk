# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS == "android"', {
        # Android doesn't use ffmpeg.
        'use_ffmpeg%': 0,
      }, {  # 'OS != "android"'
        'use_ffmpeg%': 1,
      }],
    ],
    # Set |use_fake_video_decoder| to 1 to ignore input frames in |clearkeycdm|,
    # and produce video frames filled with a solid color instead.
    'use_fake_video_decoder%': 0,
    # Set |use_libvpx| to 1 to use libvpx for VP8 decoding in |clearkeycdm|.
    'use_libvpx%': 0,
  },
  'conditions': [
    ['enable_pepper_cdms==1', {
      'targets': [
        {
          'target_name': 'clearkeycdm',
          'type': 'none',
          # TODO(tomfinegan): Simplify this by unconditionally including all the
          # decoders, and changing clearkeycdm to select which decoder to use
          # based on environment variables.
          'conditions': [
            ['use_fake_video_decoder == 1' , {
              'defines': ['CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER'],
              'sources': [
                'cdm/ppapi/external_clear_key/fake_cdm_video_decoder.cc',
                'cdm/ppapi/external_clear_key/fake_cdm_video_decoder.h',
              ],
            }],
            ['use_ffmpeg == 1'  , {
              'defines': ['CLEAR_KEY_CDM_USE_FFMPEG_DECODER'],
              'dependencies': [
                '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
              ],
              'sources': [
                'cdm/ppapi/external_clear_key/ffmpeg_cdm_audio_decoder.cc',
                'cdm/ppapi/external_clear_key/ffmpeg_cdm_audio_decoder.h',
              ],
            }],
            ['use_ffmpeg == 1 and use_fake_video_decoder == 0'  , {
              'sources': [
                'cdm/ppapi/external_clear_key/ffmpeg_cdm_video_decoder.cc',
                'cdm/ppapi/external_clear_key/ffmpeg_cdm_video_decoder.h',
              ],
            }],
            ['use_libvpx == 1 and use_fake_video_decoder == 0' , {
              'defines': ['CLEAR_KEY_CDM_USE_LIBVPX_DECODER'],
              'dependencies': [
                '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
              ],
              'sources': [
                'cdm/ppapi/external_clear_key/libvpx_cdm_video_decoder.cc',
                'cdm/ppapi/external_clear_key/libvpx_cdm_video_decoder.h',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and enable_pepper_cdms==1', {
              'type': 'loadable_module',  # Must be in PRODUCT_DIR for ASAN bot.
            }],
            ['(OS == "mac" or OS == "win") and enable_pepper_cdms==1', {
              'type': 'shared_library',
            }],
            ['OS == "mac"', {
              'xcode_settings': {
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
              },
            }]
          ],
          'defines': ['CDM_IMPLEMENTATION'],
          'dependencies': [
            'media',
            '../url/url.gyp:url_lib',
            # Include the following for media::AudioBus.
            'shared_memory_support',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            'cdm/ppapi/cdm_file_io_test.cc',
            'cdm/ppapi/cdm_file_io_test.h',
            'cdm/ppapi/external_clear_key/cdm_video_decoder.cc',
            'cdm/ppapi/external_clear_key/cdm_video_decoder.h',
            'cdm/ppapi/external_clear_key/clear_key_cdm.cc',
            'cdm/ppapi/external_clear_key/clear_key_cdm.h',
            'cdm/ppapi/external_clear_key/clear_key_cdm_common.h',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
        {
          'target_name': 'clearkeycdmadapter',
          'type': 'none',
          # Check whether the plugin's origin URL is valid.
          'defines': ['CHECK_DOCUMENT_URL'],
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            'media_cdm_adapter.gyp:cdmadapter',
            'clearkeycdm',
          ],
          'conditions': [
            ['os_posix == 1 and OS != "mac" and enable_pepper_cdms==1', {
              # Because clearkeycdm has type 'loadable_module' (see comments),
              # we must explicitly specify this dependency.
              'libraries': [
                # Built by clearkeycdm.
                '<(PRODUCT_DIR)/libclearkeycdm.so',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
