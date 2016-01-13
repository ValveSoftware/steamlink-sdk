# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libsctp_target_type%': 'static_library',
  },
  'target_defaults': {
    'defines': [
      'SCTP_PROCESS_LEVEL_LOCKS',
      'SCTP_SIMPLE_ALLOCATOR',
      '__Userspace__',
      # 'SCTP_DEBUG', # Uncomment for SCTP debugging.
    ],
    'include_dirs': [
      'usrsctplib/',
      'usrsctplib/netinet',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        'usrsctplib/',
        'usrsctplib/netinet',
      ],
    },
    'conditions': [
      ['use_openssl==1', {
        'defines': [
          'SCTP_USE_OPENSSL_SHA1',
        ],
        'dependencies': [
          '<(DEPTH)/third_party/openssl/openssl.gyp:openssl',
        ],
      },
      {  # else use_openssl==0, use NSS.
        'defines' : [
          'SCTP_USE_NSS_SHA1',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
            'dependencies': [  # The system.gyp:ssl dependency includes nss
              '<(DEPTH)/build/linux/system.gyp:ssl',
            ],
          }],
          ['OS == "mac" or OS == "ios" or OS == "win"', {
            'dependencies': [
              '<(DEPTH)/third_party/nss/nss.gyp:nspr',
              '<(DEPTH)/third_party/nss/nss.gyp:nss',
            ],
          }],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'usrsctplib',
      'type': 'static_library',
      'sources': [
        'usrsctplib/netinet/sctp.h',
        'usrsctplib/netinet/sctp_asconf.c',
        'usrsctplib/netinet/sctp_asconf.h',
        'usrsctplib/netinet/sctp_auth.c',
        'usrsctplib/netinet/sctp_auth.h',
        'usrsctplib/netinet/sctp_bsd_addr.c',
        'usrsctplib/netinet/sctp_bsd_addr.h',
        'usrsctplib/netinet/sctp_callout.c',
        'usrsctplib/netinet/sctp_callout.h',
        'usrsctplib/netinet/sctp_cc_functions.c',
        'usrsctplib/netinet/sctp_constants.h',
        'usrsctplib/netinet/sctp_crc32.c',
        'usrsctplib/netinet/sctp_crc32.h',
        'usrsctplib/netinet/sctp_header.h',
        'usrsctplib/netinet/sctp_indata.c',
        'usrsctplib/netinet/sctp_indata.h',
        'usrsctplib/netinet/sctp_input.c',
        'usrsctplib/netinet/sctp_input.h',
        'usrsctplib/netinet/sctp_lock_userspace.h',
        'usrsctplib/netinet/sctp_os.h',
        'usrsctplib/netinet/sctp_os_userspace.h',
        'usrsctplib/netinet/sctp_output.c',
        'usrsctplib/netinet/sctp_output.h',
        'usrsctplib/netinet/sctp_pcb.c',
        'usrsctplib/netinet/sctp_pcb.h',
        'usrsctplib/netinet/sctp_peeloff.c',
        'usrsctplib/netinet/sctp_peeloff.h',
        'usrsctplib/netinet/sctp_process_lock.h',
        'usrsctplib/netinet/sctp_sha1.c',
        'usrsctplib/netinet/sctp_sha1.h',
        'usrsctplib/netinet/sctp_ss_functions.c',
        'usrsctplib/netinet/sctp_structs.h',
        'usrsctplib/netinet/sctp_sysctl.c',
        'usrsctplib/netinet/sctp_sysctl.h',
        'usrsctplib/netinet/sctp_timer.c',
        'usrsctplib/netinet/sctp_timer.h',
        'usrsctplib/netinet/sctp_uio.h',
        'usrsctplib/netinet/sctp_userspace.c',
        'usrsctplib/netinet/sctp_usrreq.c',
        'usrsctplib/netinet/sctp_var.h',
        'usrsctplib/netinet/sctputil.c',
        'usrsctplib/netinet/sctputil.h',
        'usrsctplib/netinet6/sctp6_usrreq.c',
        'usrsctplib/netinet6/sctp6_var.h',
        'usrsctplib/user_atomic.h',
        'usrsctplib/user_environment.c',
        'usrsctplib/user_environment.h',
        'usrsctplib/user_inpcb.h',
        'usrsctplib/user_ip6_var.h',
        'usrsctplib/user_ip_icmp.h',
        'usrsctplib/user_malloc.h',
        'usrsctplib/user_mbuf.c',
        'usrsctplib/user_mbuf.h',
        'usrsctplib/user_queue.h',
        'usrsctplib/user_recv_thread.c',
        'usrsctplib/user_recv_thread.h',
        'usrsctplib/user_route.h',
        'usrsctplib/user_sctp_timer_iterate.c',
        'usrsctplib/user_socket.c',
        'usrsctplib/user_socketvar.h',
        'usrsctplib/user_uma.h',
        'usrsctplib/usrsctp.h'
      ],  # sources
      'conditions': [
        ['OS=="linux" or OS=="android"', {
          'defines': [
            '__Userspace_os_Linux',
          ],
          'cflags!': [ '-Werror', '-Wall' ],
          'cflags': [ '-w' ],
        }],
        ['OS=="mac"', {
          'defines': [
            'HAVE_SA_LEN',
            'HAVE_SCONN_LEN',
            '__APPLE_USE_RFC_2292',
            '__Userspace_os_Darwin',
          ],
          # usrsctp requires that __APPLE__ is undefined for compilation (for
          # historical reasons). There is a plan to change this, and when it
          # happens and we re-roll DEPS for usrsctp, we can remove the manual
          # undefining of __APPLE__.
          'xcode_settings': {
            'OTHER_CFLAGS!': [ '-Werror', '-Wall' ],
            'OTHER_CFLAGS': [ '-U__APPLE__', '-w' ],
          },
        }],
        ['OS=="win"', {
          'defines': [
            '__Userspace_os_Windows',
            # Manually setting WINVER and _WIN32_WINNT is needed because Chrome
            # sets WINVER to a newer version of  windows. But compiling usrsctp
            # this way would is incompatible  with windows XP.
            'WINVER=0x0502',
            '_WIN32_WINNT=0x0502',
          ],
          'defines!': [
            # Remove Chrome's WINVER defines to avoid redefinition warnings.
            'WINVER=0x0602',
            '_WIN32_WINNT=0x0602',
          ],
          'cflags!': [ '/W3', '/WX' ],
          'cflags': [ '/w' ],
          # TODO(ldixon) : Remove this disabling of warnings by pushing a
          # fix upstream to usrsctp
          'msvs_disabled_warnings': [ 4002, 4013, 4018, 4133, 4267, 4313, 4700 ],
        }, {  # OS != "win",
          'defines': [
            'NON_WINDOWS_DEFINE',
          ],
        }],
      ],  # conditions
    },  # target usrsctp
  ],  # targets
}
