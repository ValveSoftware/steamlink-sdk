# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'audio_modem',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
        '../third_party/webrtc/common_audio/common_audio.gyp:common_audio',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'audio_modem/audio_player.h',
        'audio_modem/audio_player_impl.cc',
        'audio_modem/audio_player_impl.h',
        'audio_modem/audio_recorder.h',
        'audio_modem/audio_recorder_impl.cc',
        'audio_modem/audio_recorder_impl.h',
        'audio_modem/constants.cc',
        'audio_modem/modem_impl.cc',
        'audio_modem/modem_impl.h',
        'audio_modem/public/modem.h',
        'audio_modem/public/audio_modem_types.h',
        'audio_modem/public/whispernet_client.h',
        'audio_modem/audio_modem_switches.cc',
        'audio_modem/audio_modem_switches.h',
      ],
    },
    {
      'target_name': 'audio_modem_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'audio_modem/test/random_samples.cc',
        'audio_modem/test/random_samples.h',
        'audio_modem/test/stub_modem.cc',
        'audio_modem/test/stub_modem.h',
        'audio_modem/test/stub_whispernet_client.cc',
        'audio_modem/test/stub_whispernet_client.h',
      ],
    },
  ],
}
