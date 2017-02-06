# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  # Most of these settings have been split according to their scope into
  # :jingle_unexported_configs and :jingle_public_config in the GN build.
  'target_defaults': {
    'defines': [
      'ENABLE_EXTERNAL_AUTH',
      'EXPAT_RELATIVE_PATH',
      'FEATURE_ENABLE_SSL',
      'GTEST_RELATIVE_PATH',
      'HAVE_OPENSSL_SSL_H',
      'HAVE_SCTP',
      'HAVE_SRTP',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
      'LOGGING_INSIDE_WEBRTC',
      'NO_MAIN_THREAD_WRAPPING',
      'NO_SOUND_SYSTEM',
      'SRTP_RELATIVE_PATH',
      'SSL_USE_OPENSSL',
      'USE_WEBRTC_DEV_BRANCH',
      'WEBRTC_CHROMIUM_BUILD',
    ],
    'include_dirs': [
      '../../third_party/webrtc_overrides',
      '../..',
      '../../testing/gtest/include',
      '../../third_party',
      '../../third_party/libyuv/include',
      '../../third_party/usrsctp/usrsctplib',
    ],
    # These dependencies have been translated into :jingle_deps in the GN build.
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/third_party/boringssl/boringssl.gyp:boringssl',
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '../../third_party/webrtc_overrides',
        '../..',
        '../../testing/gtest/include',
        '../../third_party',
      ],
      'defines': [
        'FEATURE_ENABLE_SSL',
        'FEATURE_ENABLE_VOICEMAIL',
        'EXPAT_RELATIVE_PATH',
        'GTEST_RELATIVE_PATH',
        'NO_MAIN_THREAD_WRAPPING',
        'NO_SOUND_SYSTEM',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lsecur32.lib',
              '-lcrypt32.lib',
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/platformsdk_win7/files/Include',
          ],
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',  # Suppres warnings about _vsnprinf
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267 ],
        }],
        ['OS=="linux"', {
          'defines': [
            'LINUX',
            'WEBRTC_LINUX',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'OSX',
            'WEBRTC_MAC',
          ],
        }],
        ['OS=="ios"', {
          'defines': [
            'IOS',
            'WEBRTC_MAC',
            'WEBRTC_IOS',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'WEBRTC_WIN',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'ANDROID',
          ],
        }],
        ['os_posix==1', {
          'defines': [
            'WEBRTC_POSIX',
          ],
        }],
        ['os_bsd==1', {
          'defines': [
            'BSD',
          ],
        }],
        ['OS=="openbsd"', {
          'defines': [
            'OPENBSD',
          ],
        }],
        ['OS=="freebsd"', {
          'defines': [
            'FREEBSD',
          ],
        }],
        ['chromeos==1', {
          'defines': [
            'CHROMEOS',
          ],
        }],
      ],
    },
    'variables': {
      'clang_warning_flags_unset': [
        # Don't warn about string->bool used in asserts.
        '-Wstring-conversion',
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
          'WEBRTC_LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
          'WEBRTC_MAC',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'IOS',
          'WEBRTC_MAC',
          'WEBRTC_IOS',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'WEBRTC_POSIX',
        ],
      }],
      ['os_bsd==1', {
        'defines': [
          'BSD',
        ],
      }],
      ['OS=="openbsd"', {
        'defines': [
          'OPENBSD',
        ],
      }],
      ['OS=="freebsd"', {
        'defines': [
          'FREEBSD',
        ],
      }],
    ],
  },
  'targets': [
    # GN version: //third_party/libjingle
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'includes': [ 'libjingle_common.gypi' ],
      'dependencies': [
        '<(DEPTH)/third_party/webrtc/base/base.gyp:rtc_base',
        '<(DEPTH)/third_party/webrtc/libjingle/xmllite/xmllite.gyp:rtc_xmllite',
      ],
    },  # target libjingle
  ],
  'conditions': [
    ['enable_webrtc==1', {
      'targets': [
        {
          # GN version: //third_party/libjingle:libjingle_webrtc_common
          'target_name': 'libjingle_webrtc_common',
          'type': 'static_library',
          'sources': [
            '<(DEPTH)/third_party/webrtc/api/audiotrack.cc',
            '<(DEPTH)/third_party/webrtc/api/audiotrack.h',
            '<(DEPTH)/third_party/webrtc/api/datachannel.cc',
            '<(DEPTH)/third_party/webrtc/api/datachannel.h',
            '<(DEPTH)/third_party/webrtc/api/dtlsidentitystore.h',
            '<(DEPTH)/third_party/webrtc/api/dtmfsender.cc',
            '<(DEPTH)/third_party/webrtc/api/dtmfsender.h',
            '<(DEPTH)/third_party/webrtc/api/jsep.h',
            '<(DEPTH)/third_party/webrtc/api/jsepicecandidate.cc',
            '<(DEPTH)/third_party/webrtc/api/jsepicecandidate.h',
            '<(DEPTH)/third_party/webrtc/api/jsepsessiondescription.cc',
            '<(DEPTH)/third_party/webrtc/api/jsepsessiondescription.h',
            '<(DEPTH)/third_party/webrtc/api/localaudiosource.cc',
            '<(DEPTH)/third_party/webrtc/api/localaudiosource.h',
            '<(DEPTH)/third_party/webrtc/api/mediaconstraintsinterface.cc',
            '<(DEPTH)/third_party/webrtc/api/mediaconstraintsinterface.h',
            '<(DEPTH)/third_party/webrtc/api/mediacontroller.cc',
            '<(DEPTH)/third_party/webrtc/api/mediacontroller.h',
            '<(DEPTH)/third_party/webrtc/api/mediastream.cc',
            '<(DEPTH)/third_party/webrtc/api/mediastream.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreaminterface.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreamobserver.cc',
            '<(DEPTH)/third_party/webrtc/api/mediastreamobserver.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreamprovider.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreamproxy.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreamtrack.h',
            '<(DEPTH)/third_party/webrtc/api/mediastreamtrackproxy.h',
            '<(DEPTH)/third_party/webrtc/api/notifier.h',
            '<(DEPTH)/third_party/webrtc/api/peerconnection.cc',
            '<(DEPTH)/third_party/webrtc/api/peerconnection.h',
            '<(DEPTH)/third_party/webrtc/api/peerconnectionfactory.cc',
            '<(DEPTH)/third_party/webrtc/api/peerconnectionfactory.h',
            '<(DEPTH)/third_party/webrtc/api/peerconnectioninterface.h',
            '<(DEPTH)/third_party/webrtc/api/remoteaudiosource.cc',
            '<(DEPTH)/third_party/webrtc/api/remoteaudiosource.h',
            '<(DEPTH)/third_party/webrtc/api/rtpreceiver.cc',
            '<(DEPTH)/third_party/webrtc/api/rtpreceiver.h',
            '<(DEPTH)/third_party/webrtc/api/rtpreceiverinterface.h',
            '<(DEPTH)/third_party/webrtc/api/rtpsender.cc',
            '<(DEPTH)/third_party/webrtc/api/rtpsender.h',
            '<(DEPTH)/third_party/webrtc/api/rtpsenderinterface.h',
            '<(DEPTH)/third_party/webrtc/api/sctputils.cc',
            '<(DEPTH)/third_party/webrtc/api/sctputils.h',
            '<(DEPTH)/third_party/webrtc/api/statscollector.cc',
            '<(DEPTH)/third_party/webrtc/api/statscollector.h',
            '<(DEPTH)/third_party/webrtc/api/statstypes.cc',
            '<(DEPTH)/third_party/webrtc/api/statstypes.h',
            '<(DEPTH)/third_party/webrtc/api/streamcollection.h',
            '<(DEPTH)/third_party/webrtc/api/umametrics.h',
            '<(DEPTH)/third_party/webrtc/api/videocapturertracksource.cc',
            '<(DEPTH)/third_party/webrtc/api/videocapturertracksource.h',
            '<(DEPTH)/third_party/webrtc/api/videosourceproxy.h',
            '<(DEPTH)/third_party/webrtc/api/videotrack.cc',
            '<(DEPTH)/third_party/webrtc/api/videotrack.h',
            '<(DEPTH)/third_party/webrtc/api/videotracksource.cc',
            '<(DEPTH)/third_party/webrtc/api/videotracksource.h',
            '<(DEPTH)/third_party/webrtc/api/webrtcsdp.cc',
            '<(DEPTH)/third_party/webrtc/api/webrtcsdp.h',
            '<(DEPTH)/third_party/webrtc/api/webrtcsession.cc',
            '<(DEPTH)/third_party/webrtc/api/webrtcsession.h',
            '<(DEPTH)/third_party/webrtc/api/webrtcsessiondescriptionfactory.cc',
            '<(DEPTH)/third_party/webrtc/api/webrtcsessiondescriptionfactory.h',
            '<(DEPTH)/third_party/webrtc/media/base/codec.cc',
            '<(DEPTH)/third_party/webrtc/media/base/codec.h',
            '<(DEPTH)/third_party/webrtc/media/base/cryptoparams.h',
            '<(DEPTH)/third_party/webrtc/media/base/hybriddataengine.h',
            '<(DEPTH)/third_party/webrtc/media/base/mediachannel.h',
            '<(DEPTH)/third_party/webrtc/media/base/mediaconstants.cc',
            '<(DEPTH)/third_party/webrtc/media/base/mediaconstants.h',
            '<(DEPTH)/third_party/webrtc/media/base/mediaengine.cc',
            '<(DEPTH)/third_party/webrtc/media/base/mediaengine.h',
            '<(DEPTH)/third_party/webrtc/media/base/rtpdataengine.cc',
            '<(DEPTH)/third_party/webrtc/media/base/rtpdataengine.h',
            '<(DEPTH)/third_party/webrtc/media/base/rtpdump.cc',
            '<(DEPTH)/third_party/webrtc/media/base/rtpdump.h',
            '<(DEPTH)/third_party/webrtc/media/base/rtputils.cc',
            '<(DEPTH)/third_party/webrtc/media/base/rtputils.h',
            '<(DEPTH)/third_party/webrtc/media/base/streamparams.cc',
            '<(DEPTH)/third_party/webrtc/media/base/streamparams.h',
            '<(DEPTH)/third_party/webrtc/media/base/turnutils.cc',
            '<(DEPTH)/third_party/webrtc/media/base/turnutils.h',
            '<(DEPTH)/third_party/webrtc/media/base/videoadapter.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videoadapter.h',
            '<(DEPTH)/third_party/webrtc/media/base/videobroadcaster.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videobroadcaster.h',
            '<(DEPTH)/third_party/webrtc/media/base/videocapturer.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videocapturer.h',
            '<(DEPTH)/third_party/webrtc/media/base/videocommon.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videocommon.h',
            '<(DEPTH)/third_party/webrtc/media/base/videoframe.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videoframe.h',
            '<(DEPTH)/third_party/webrtc/media/base/videoframefactory.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videoframefactory.h',
            '<(DEPTH)/third_party/webrtc/media/base/videosourcebase.cc',
            '<(DEPTH)/third_party/webrtc/media/base/videosourcebase.h',
            '<(DEPTH)/third_party/webrtc/media/engine/simulcast.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/simulcast.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtccommon.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcmediaengine.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcmediaengine.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoengine2.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoengine2.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoframe.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoframe.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoframefactory.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvideoframefactory.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvoe.h',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvoiceengine.cc',
            '<(DEPTH)/third_party/webrtc/media/engine/webrtcvoiceengine.h',
            '<(DEPTH)/third_party/webrtc/media/sctp/sctpdataengine.cc',
            '<(DEPTH)/third_party/webrtc/media/sctp/sctpdataengine.h',
            '<(DEPTH)/third_party/webrtc/pc/audiomonitor.cc',
            '<(DEPTH)/third_party/webrtc/pc/audiomonitor.h',
            '<(DEPTH)/third_party/webrtc/pc/bundlefilter.cc',
            '<(DEPTH)/third_party/webrtc/pc/bundlefilter.h',
            '<(DEPTH)/third_party/webrtc/pc/channel.cc',
            '<(DEPTH)/third_party/webrtc/pc/channel.h',
            '<(DEPTH)/third_party/webrtc/pc/channelmanager.cc',
            '<(DEPTH)/third_party/webrtc/pc/channelmanager.h',
            '<(DEPTH)/third_party/webrtc/pc/currentspeakermonitor.cc',
            '<(DEPTH)/third_party/webrtc/pc/currentspeakermonitor.h',
            '<(DEPTH)/third_party/webrtc/pc/externalhmac.cc',
            '<(DEPTH)/third_party/webrtc/pc/externalhmac.h',
            '<(DEPTH)/third_party/webrtc/pc/mediamonitor.cc',
            '<(DEPTH)/third_party/webrtc/pc/mediamonitor.h',
            '<(DEPTH)/third_party/webrtc/pc/mediasession.cc',
            '<(DEPTH)/third_party/webrtc/pc/mediasession.h',
            '<(DEPTH)/third_party/webrtc/pc/mediasink.h',
            '<(DEPTH)/third_party/webrtc/pc/rtcpmuxfilter.cc',
            '<(DEPTH)/third_party/webrtc/pc/rtcpmuxfilter.h',
            '<(DEPTH)/third_party/webrtc/pc/srtpfilter.cc',
            '<(DEPTH)/third_party/webrtc/pc/srtpfilter.h',
            '<(DEPTH)/third_party/webrtc/pc/voicechannel.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
            '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:media_file',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_capture',
            '<(DEPTH)/third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine',
            '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
            'libjingle',
          ],
        },  # target libjingle_webrtc_common
        {
          # TODO(kjellander): Move this target into
          # //third_party/webrtc_overrides as soon as the work in
          # bugs.webrtc.org/4256 has gotten rid of the duplicated source
          # listings above.
          # GN version: //third_party/libjingle:libjingle_webrtc
          'target_name': 'libjingle_webrtc',
          'type': 'static_library',
          'sources': [
            '../webrtc_overrides/init_webrtc.cc',
            '../webrtc_overrides/init_webrtc.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:audio_processing',
            'libjingle_webrtc_common',
          ],
        },
      ],
    }],
  ],
}
