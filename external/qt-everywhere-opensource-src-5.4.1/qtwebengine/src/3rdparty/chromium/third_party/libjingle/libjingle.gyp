# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'variables': {
    'enabled_libjingle_device_manager%': 0,
    'libjingle_additional_deps%': [],
    'libjingle_peerconnection_additional_deps%': [],
    'libjingle_source%': "source",
    'libpeer_target_type%': 'static_library',
    'libpeer_allocator_shim%': 0,
  },
  'target_defaults': {
    'defines': [
      'EXPAT_RELATIVE_PATH',
      'FEATURE_ENABLE_SSL',
      'GTEST_RELATIVE_PATH',
      'HAVE_SRTP',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
      'LOGGING_INSIDE_LIBJINGLE',
      'NO_MAIN_THREAD_WRAPPING',
      'NO_SOUND_SYSTEM',
      'SRTP_RELATIVE_PATH',
      'USE_WEBRTC_DEV_BRANCH',
      'ENABLE_EXTERNAL_AUTH',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
          # _DEBUG and remove this define. See below as well.
          '_DEBUG',
        ],
      }
    },
    'include_dirs': [
      './overrides',
      './<(libjingle_source)',
      '../../third_party/webrtc/overrides',
      '../..',
      '../../testing/gtest/include',
      '../../third_party',
      '../../third_party/libyuv/include',
      '../../third_party/usrsctp',
      '../../third_party/webrtc',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './overrides',
        './<(libjingle_source)',
        '../../third_party/webrtc/overrides',
        '../..',
        '../../testing/gtest/include',
        '../../third_party',
        '../../third_party/webrtc',
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
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'OSX',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'ANDROID',
          ],
        }],
        ['os_posix==1', {
          'defines': [
            'POSIX',
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
    'all_dependent_settings': {
      'configurations': {
        'Debug': {
          'defines': [
            # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
            # _DEBUG and remove this define. See above as well.
            '_DEBUG',
          ],
        }
      },
    },
    'conditions': [
      ['"<(libpeer_target_type)"=="static_library"', {
        'defines': [ 'LIBPEERCONNECTION_LIB=1' ],
      }],
      ['use_openssl==1', {
        'defines': [
          'SSL_USE_OPENSSL',
          'HAVE_OPENSSL_SSL_H',
        ],
        'dependencies': [
          '../../third_party/openssl/openssl.gyp:openssl',
        ],
      }, {
        'defines': [
          'SSL_USE_NSS',
          'HAVE_NSS_SSL_H',
          'SSL_USE_NSS_RNG',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
            'dependencies': [
              '<(DEPTH)/build/linux/system.gyp:ssl',
            ],
          }],
          ['OS == "mac" or OS == "ios" or OS == "win"', {
            'dependencies': [
              '<(DEPTH)/net/third_party/nss/ssl.gyp:libssl',
              '<(DEPTH)/third_party/nss/nss.gyp:nspr',
              '<(DEPTH)/third_party/nss/nss.gyp:nss',
            ],
          }],
        ],
      }],
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
        'conditions' : [
          ['target_arch == "ia32"', {
            'defines': [
              '_USE_32BIT_TIME_T',
            ],
          }],
        ],
      }],
      ['clang == 1', {
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            # Don't warn about string->bool used in asserts.
            '-Wstring-conversion',
          ],
        },
        'cflags!': [
          '-Wstring-conversion',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'IOS',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'POSIX',
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
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'includes': [ 'libjingle_common.gypi' ],
      'sources': [
        'overrides/talk/base/basictypes.h',
        'overrides/talk/base/constructormagic.h',
        'overrides/talk/base/win32socketinit.cc',

        # Overrides logging.h/.cc because libjingle logging should be done to
        # the same place as the chromium logging.
        'overrides/talk/base/logging.cc',
        'overrides/talk/base/logging.h',
      ],
      'sources!' : [
        # Compiled as part of libjingle_p2p_constants.
        '<(libjingle_source)/talk/p2p/base/constants.cc',
        '<(libjingle_source)/talk/p2p/base/constants.h',

        # Replaced with logging.cc in the overrides.
        '<(libjingle_source)/talk/base/logging.h',
        '<(libjingle_source)/talk/base/logging.cc',
      ],
      'dependencies': [
        'libjingle_p2p_constants',
        '<@(libjingle_additional_deps)',
      ],
    },  # target libjingle
    # This has to be is a separate project due to a bug in MSVS 2008 and the
    # current toolset on android.  The problem is that we have two files named
    # "constants.cc" and MSVS/android doesn't handle this properly.
    # GYP currently has guards to catch this, so if you want to remove it,
    # run GYP and if GYP has removed the validation check, then we can assume
    # that the toolchains have been fixed (we currently use VS2010 and later,
    # so VS2008 isn't a concern anymore).
    {
      'target_name': 'libjingle_p2p_constants',
      'type': 'static_library',
      'sources': [
        '<(libjingle_source)/talk/p2p/base/constants.cc',
        '<(libjingle_source)/talk/p2p/base/constants.h',
      ],
    },  # target libjingle_p2p_constants
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        '<(libjingle_source)/talk/examples/peerconnection/server/data_socket.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/data_socket.h',
        '<(libjingle_source)/talk/examples/peerconnection/server/main.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/peer_channel.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/peer_channel.h',
        '<(libjingle_source)/talk/examples/peerconnection/server/utils.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/utils.h',
      ],
      'include_dirs': [
        '<(libjingle_source)',
      ],
      'dependencies': [
        'libjingle',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4309, ],
    }, # target peerconnection_server
  ],
  'conditions': [
    ['enable_webrtc==1', {
      'targets': [
        {
          'target_name': 'libjingle_webrtc_common',
          'type': 'static_library',
          'all_dependent_settings': {
            'conditions': [
              ['"<(libpeer_target_type)"=="static_library"', {
                'defines': [ 'LIBPEERCONNECTION_LIB=1' ],
              }],
            ],
          },
          'sources': [
            'overrides/talk/media/webrtc/webrtcexport.h',

            '<(libjingle_source)/talk/app/webrtc/audiotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/audiotrack.h',
            '<(libjingle_source)/talk/app/webrtc/audiotrackrenderer.cc',
            '<(libjingle_source)/talk/app/webrtc/audiotrackrenderer.h',
            '<(libjingle_source)/talk/app/webrtc/datachannel.cc',
            '<(libjingle_source)/talk/app/webrtc/datachannel.h',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.cc',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.h',
            '<(libjingle_source)/talk/app/webrtc/jsep.h',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.h',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.h',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.cc',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.h',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.cc',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediastream.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastream.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreaminterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamprovider.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamproxy.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamsignaling.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamsignaling.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrack.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrackproxy.h',
            '<(libjingle_source)/talk/app/webrtc/notifier.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectioninterface.h',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.h',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiosource.cc',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiosource.h',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.cc',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.h',
            '<(libjingle_source)/talk/app/webrtc/sctputils.cc',
            '<(libjingle_source)/talk/app/webrtc/sctputils.h',
            '<(libjingle_source)/talk/app/webrtc/statscollector.cc',
            '<(libjingle_source)/talk/app/webrtc/statscollector.h',
            '<(libjingle_source)/talk/app/webrtc/statstypes.h',
            '<(libjingle_source)/talk/app/webrtc/streamcollection.h',
            '<(libjingle_source)/talk/app/webrtc/umametrics.h',
            '<(libjingle_source)/talk/app/webrtc/videosource.cc',
            '<(libjingle_source)/talk/app/webrtc/videosource.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceinterface.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceproxy.h',
            '<(libjingle_source)/talk/app/webrtc/videotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrack.h',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.h',
            '<(libjingle_source)/talk/media/base/audiorenderer.h',
            '<(libjingle_source)/talk/media/base/capturemanager.cc',
            '<(libjingle_source)/talk/media/base/capturemanager.h',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.cc',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.h',
            '<(libjingle_source)/talk/media/base/codec.cc',
            '<(libjingle_source)/talk/media/base/codec.h',
            '<(libjingle_source)/talk/media/base/constants.cc',
            '<(libjingle_source)/talk/media/base/constants.h',
            '<(libjingle_source)/talk/media/base/cryptoparams.h',
            '<(libjingle_source)/talk/media/base/filemediaengine.cc',
            '<(libjingle_source)/talk/media/base/filemediaengine.h',
            '<(libjingle_source)/talk/media/base/hybriddataengine.h',
            '<(libjingle_source)/talk/media/base/mediachannel.h',
            '<(libjingle_source)/talk/media/base/mediaengine.cc',
            '<(libjingle_source)/talk/media/base/mediaengine.h',
            '<(libjingle_source)/talk/media/base/rtpdataengine.cc',
            '<(libjingle_source)/talk/media/base/rtpdataengine.h',
            '<(libjingle_source)/talk/media/base/rtpdump.cc',
            '<(libjingle_source)/talk/media/base/rtpdump.h',
            '<(libjingle_source)/talk/media/base/rtputils.cc',
            '<(libjingle_source)/talk/media/base/rtputils.h',
            '<(libjingle_source)/talk/media/base/streamparams.cc',
            '<(libjingle_source)/talk/media/base/streamparams.h',
            '<(libjingle_source)/talk/media/base/videoadapter.cc',
            '<(libjingle_source)/talk/media/base/videoadapter.h',
            '<(libjingle_source)/talk/media/base/videocapturer.cc',
            '<(libjingle_source)/talk/media/base/videocapturer.h',
            '<(libjingle_source)/talk/media/base/videocommon.cc',
            '<(libjingle_source)/talk/media/base/videocommon.h',
            '<(libjingle_source)/talk/media/base/videoframe.cc',
            '<(libjingle_source)/talk/media/base/videoframe.h',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.cc',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.h',
            '<(libjingle_source)/talk/media/webrtc/webrtccommon.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcpassthroughrender.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcpassthroughrender.h',
            '<(libjingle_source)/talk/media/webrtc/webrtctexturevideoframe.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtctexturevideoframe.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideocapturer.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideocapturer.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvie.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoe.h',
            '<(libjingle_source)/talk/session/media/audiomonitor.cc',
            '<(libjingle_source)/talk/session/media/audiomonitor.h',
            '<(libjingle_source)/talk/session/media/bundlefilter.cc',
            '<(libjingle_source)/talk/session/media/bundlefilter.h',
            '<(libjingle_source)/talk/session/media/call.cc',
            '<(libjingle_source)/talk/session/media/call.h',
            '<(libjingle_source)/talk/session/media/channel.cc',
            '<(libjingle_source)/talk/session/media/channel.h',
            '<(libjingle_source)/talk/session/media/channelmanager.cc',
            '<(libjingle_source)/talk/session/media/channelmanager.h',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.cc',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.h',
            '<(libjingle_source)/talk/session/media/externalhmac.cc',
            '<(libjingle_source)/talk/session/media/externalhmac.h',
            '<(libjingle_source)/talk/session/media/mediamessages.cc',
            '<(libjingle_source)/talk/session/media/mediamessages.h',
            '<(libjingle_source)/talk/session/media/mediamonitor.cc',
            '<(libjingle_source)/talk/session/media/mediamonitor.h',
            '<(libjingle_source)/talk/session/media/mediasession.cc',
            '<(libjingle_source)/talk/session/media/mediasession.h',
            '<(libjingle_source)/talk/session/media/mediasessionclient.cc',
            '<(libjingle_source)/talk/session/media/mediasessionclient.h',
            '<(libjingle_source)/talk/session/media/mediasink.h',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.cc',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.h',
            '<(libjingle_source)/talk/session/media/soundclip.cc',
            '<(libjingle_source)/talk/session/media/soundclip.h',
            '<(libjingle_source)/talk/session/media/srtpfilter.cc',
            '<(libjingle_source)/talk/session/media/srtpfilter.h',
            '<(libjingle_source)/talk/session/media/typingmonitor.cc',
            '<(libjingle_source)/talk/session/media/typingmonitor.h',
            '<(libjingle_source)/talk/session/media/voicechannel.h',
            '<(libjingle_source)/talk/session/tunnel/pseudotcpchannel.cc',
            '<(libjingle_source)/talk/session/tunnel/pseudotcpchannel.h',
            '<(libjingle_source)/talk/session/tunnel/tunnelsessionclient.cc',
            '<(libjingle_source)/talk/session/tunnel/tunnelsessionclient.h',
          ],
          'conditions': [
            ['libpeer_allocator_shim==1 and '
             'libpeer_target_type!="static_library" and OS!="mac"', {
              'sources': [
                'overrides/allocator_shim/allocator_stub.cc',
                'overrides/allocator_shim/allocator_stub.h',
              ],
            }],
            # TODO(mallinath) - Enable SCTP for iOS.
            ['OS!="ios"', {
              'defines': [
                'HAVE_SCTP',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.cc',
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.h',
              ],
              'dependencies': [
                '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
              ],
            }],
            ['enabled_libjingle_device_manager==1', {
              'sources!': [
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/devices/devicemanager.cc',
                '<(libjingle_source)/talk/media/devices/devicemanager.h',
                '<(libjingle_source)/talk/sound/nullsoundsystem.cc',
                '<(libjingle_source)/talk/sound/nullsoundsystem.h',
                '<(libjingle_source)/talk/sound/nullsoundsystemfactory.cc',
                '<(libjingle_source)/talk/sound/nullsoundsystemfactory.h',
                '<(libjingle_source)/talk/sound/platformsoundsystem.cc',
                '<(libjingle_source)/talk/sound/platformsoundsystem.h',
                '<(libjingle_source)/talk/sound/platformsoundsystemfactory.cc',
                '<(libjingle_source)/talk/sound/platformsoundsystemfactory.h',
                '<(libjingle_source)/talk/sound/soundsysteminterface.cc',
                '<(libjingle_source)/talk/sound/soundsysteminterface.h',
                '<(libjingle_source)/talk/sound/soundsystemproxy.cc',
                '<(libjingle_source)/talk/sound/soundsystemproxy.h',
              ],
              'conditions': [
                ['OS=="win"', {
                  'sources': [
                    '<(libjingle_source)/talk/base/win32window.cc',
                    '<(libjingle_source)/talk/base/win32window.h',
                    '<(libjingle_source)/talk/base/win32windowpicker.cc',
                    '<(libjingle_source)/talk/base/win32windowpicker.h',
                    '<(libjingle_source)/talk/media/devices/win32deviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.h',
                  ],
                }],
                ['OS=="linux"', {
                  'sources': [
                    '<(libjingle_source)/talk/base/linuxwindowpicker.cc',
                    '<(libjingle_source)/talk/base/linuxwindowpicker.h',
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.cc',
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.h',
                    '<(libjingle_source)/talk/media/devices/linuxdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/v4llookup.cc',
                    '<(libjingle_source)/talk/media/devices/v4llookup.h',
                    '<(libjingle_source)/talk/sound/alsasoundsystem.cc',
                    '<(libjingle_source)/talk/sound/alsasoundsystem.h',
                    '<(libjingle_source)/talk/sound/alsasymboltable.cc',
                    '<(libjingle_source)/talk/sound/alsasymboltable.h',
                    '<(libjingle_source)/talk/sound/linuxsoundsystem.cc',
                    '<(libjingle_source)/talk/sound/linuxsoundsystem.h',
                    '<(libjingle_source)/talk/sound/pulseaudiosoundsystem.cc',
                    '<(libjingle_source)/talk/sound/pulseaudiosoundsystem.h',
                    '<(libjingle_source)/talk/sound/pulseaudiosymboltable.cc',
                    '<(libjingle_source)/talk/sound/pulseaudiosymboltable.h',
                  ],
                }],
                ['OS=="mac"', {
                  'sources': [
                    '<(libjingle_source)/talk/media/devices/macdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/macdevicemanagermm.mm',
                  ],
                  'xcode_settings': {
                    'WARNING_CFLAGS': [
                      # Suppres warnings about using deprecated functions in
                      # macdevicemanager.cc.
                      '-Wno-deprecated-declarations',
                    ],
                  },
                }],
              ],
            }],
          ],
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:media_file',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_capture_module',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_render_module',
            'libjingle',
          ],
        },  # target libjingle_webrtc_common
        {
          'target_name': 'libjingle_webrtc',
          'type': 'static_library',
          'sources': [
            'overrides/init_webrtc.cc',
            'overrides/init_webrtc.h',
          ],
          'dependencies': [
            'libjingle_webrtc_common',
          ],
        },
        {
          'target_name': 'libpeerconnection',
          'type': '<(libpeer_target_type)',
          'sources': [
            '<(libjingle_source)/talk/media/webrtc/webrtcmediaengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcmediaengine.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine2.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine2.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(DEPTH)/third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine',
            '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
            '<@(libjingle_peerconnection_additional_deps)',
            'libjingle_webrtc_common',
          ],
          'conditions': [
            ['libpeer_target_type!="static_library"', {
              'sources': [
                'overrides/initialize_module.cc',
              ],
              'conditions': [
                ['OS!="mac" and OS!="android"', {
                  'sources': [
                    'overrides/allocator_shim/allocator_proxy.cc',
                  ],
                }],
              ],
            }],
            ['"<(libpeer_target_type)"!="static_library"', {
              # Used to control symbol export/import.
              'defines': [ 'LIBPEERCONNECTION_IMPLEMENTATION=1' ],
            }],
            ['OS=="win" and "<(libpeer_target_type)"!="static_library"', {
              'link_settings': {
                'libraries': [
                  '-lsecur32.lib',
                  '-lcrypt32.lib',
                  '-liphlpapi.lib',
                ],
              },
            }],
            ['OS!="win" and "<(libpeer_target_type)"!="static_library"', {
              'cflags': [
                # For compatibility with how we export symbols from this
                # target on Windows.  This also prevents the linker from
                # picking up symbols from this target that should be linked
                # in from other libjingle libs.
                '-fvisibility=hidden',
              ],
            }],
            ['OS=="mac" and libpeer_target_type!="static_library"', {
              'product_name': 'libpeerconnection',
            }],
            ['OS=="android" and "<(libpeer_target_type)"=="static_library"', {
              'standalone_static_library': 1,
            }],
            ['OS=="linux" and libpeer_target_type!="static_library"', {
              # The installer and various tools depend on finding the .so
              # in this directory and not lib.target as will otherwise be
              # the case with make builds.
              'product_dir': '<(PRODUCT_DIR)/lib',
            }],
          ],
        },  # target libpeerconnection
      ],
    }],
  ],
}
