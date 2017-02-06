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
      'SCTP_USE_OPENSSL_SHA1',
      '__Userspace__',
      # 'SCTP_DEBUG', # Uncomment for SCTP debugging.
    ],
    'include_dirs': [
      'usrsctplib/usrsctplib/',
      'usrsctplib/usrsctplib/netinet',
    ],
    'dependencies': [
      '<(DEPTH)/third_party/boringssl/boringssl.gyp:boringssl',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        'usrsctplib/usrsctplib/',
        'usrsctplib/usrsctplib/netinet',
      ],
    },
  },
  'targets': [
    {
      # GN version: //third_party/usrsctp
      'target_name': 'usrsctplib',
      'type': 'static_library',
      'sources': [
        # Note: sources list duplicated in GN build.
         'usrsctplib/usrsctplib/netinet/sctp.h',
         'usrsctplib/usrsctplib/netinet/sctp_asconf.c',
         'usrsctplib/usrsctplib/netinet/sctp_asconf.h',
         'usrsctplib/usrsctplib/netinet/sctp_auth.c',
         'usrsctplib/usrsctplib/netinet/sctp_auth.h',
         'usrsctplib/usrsctplib/netinet/sctp_bsd_addr.c',
         'usrsctplib/usrsctplib/netinet/sctp_bsd_addr.h',
         'usrsctplib/usrsctplib/netinet/sctp_callout.c',
         'usrsctplib/usrsctplib/netinet/sctp_callout.h',
         'usrsctplib/usrsctplib/netinet/sctp_cc_functions.c',
         'usrsctplib/usrsctplib/netinet/sctp_constants.h',
         'usrsctplib/usrsctplib/netinet/sctp_crc32.c',
         'usrsctplib/usrsctplib/netinet/sctp_crc32.h',
         'usrsctplib/usrsctplib/netinet/sctp_header.h',
         'usrsctplib/usrsctplib/netinet/sctp_indata.c',
         'usrsctplib/usrsctplib/netinet/sctp_indata.h',
         'usrsctplib/usrsctplib/netinet/sctp_input.c',
         'usrsctplib/usrsctplib/netinet/sctp_input.h',
         'usrsctplib/usrsctplib/netinet/sctp_lock_userspace.h',
         'usrsctplib/usrsctplib/netinet/sctp_os.h',
         'usrsctplib/usrsctplib/netinet/sctp_os_userspace.h',
         'usrsctplib/usrsctplib/netinet/sctp_output.c',
         'usrsctplib/usrsctplib/netinet/sctp_output.h',
         'usrsctplib/usrsctplib/netinet/sctp_pcb.c',
         'usrsctplib/usrsctplib/netinet/sctp_pcb.h',
         'usrsctplib/usrsctplib/netinet/sctp_peeloff.c',
         'usrsctplib/usrsctplib/netinet/sctp_peeloff.h',
         'usrsctplib/usrsctplib/netinet/sctp_process_lock.h',
         'usrsctplib/usrsctplib/netinet/sctp_sha1.c',
         'usrsctplib/usrsctplib/netinet/sctp_sha1.h',
         'usrsctplib/usrsctplib/netinet/sctp_ss_functions.c',
         'usrsctplib/usrsctplib/netinet/sctp_structs.h',
         'usrsctplib/usrsctplib/netinet/sctp_sysctl.c',
         'usrsctplib/usrsctplib/netinet/sctp_sysctl.h',
         'usrsctplib/usrsctplib/netinet/sctp_timer.c',
         'usrsctplib/usrsctplib/netinet/sctp_timer.h',
         'usrsctplib/usrsctplib/netinet/sctp_uio.h',
         'usrsctplib/usrsctplib/netinet/sctp_userspace.c',
         'usrsctplib/usrsctplib/netinet/sctp_usrreq.c',
         'usrsctplib/usrsctplib/netinet/sctp_var.h',
         'usrsctplib/usrsctplib/netinet/sctputil.c',
         'usrsctplib/usrsctplib/netinet/sctputil.h',
         'usrsctplib/usrsctplib/netinet6/sctp6_usrreq.c',
         'usrsctplib/usrsctplib/netinet6/sctp6_var.h',
         'usrsctplib/usrsctplib/user_atomic.h',
         'usrsctplib/usrsctplib/user_environment.c',
         'usrsctplib/usrsctplib/user_environment.h',
         'usrsctplib/usrsctplib/user_inpcb.h',
         'usrsctplib/usrsctplib/user_ip6_var.h',
         'usrsctplib/usrsctplib/user_ip_icmp.h',
         'usrsctplib/usrsctplib/user_malloc.h',
         'usrsctplib/usrsctplib/user_mbuf.c',
         'usrsctplib/usrsctplib/user_mbuf.h',
         'usrsctplib/usrsctplib/user_queue.h',
         'usrsctplib/usrsctplib/user_recv_thread.c',
         'usrsctplib/usrsctplib/user_recv_thread.h',
         'usrsctplib/usrsctplib/user_route.h',
         'usrsctplib/usrsctplib/user_socket.c',
         'usrsctplib/usrsctplib/user_socketvar.h',
         'usrsctplib/usrsctplib/user_uma.h',
         'usrsctplib/usrsctplib/usrsctp.h',
      ],  # sources
      'variables': {
        'clang_warning_flags': [
          # atomic_init in user_atomic.h is a static function in a header.
          '-Wno-unused-function',
        ],
      },
      'conditions': [
        ['OS=="linux" or OS=="android"', {
          'defines': [
            '__Userspace_os_Linux',
	    '_GNU_SOURCE'
          ],
          'cflags!': [ '-Werror', '-Wall' ],
          'cflags': [ '-w' ],
        }],
        ['OS=="mac" or OS=="ios"', {
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
            # sets WINVER to a newer version of Windows. But compiling usrsctp
            # this way would be incompatible with Windows XP.
            'WINVER=0x0502',
            '_WIN32_WINNT=0x0502',
          ],
          'defines!': [
            # Remove Chrome's WINVER defines to avoid redefinition warnings.
            'WINVER=0x0A00',
            '_WIN32_WINNT=0x0A00',
          ],
          'cflags!': [ '/W3', '/WX' ],
          'cflags': [ '/w' ],
          # TODO(ldixon) : Remove this disabling of warnings by pushing a
          # fix upstream to usrsctp
          'msvs_disabled_warnings': [ 4002, 4013, 4133, 4267, 4313, 4700 ],
        }, {  # OS != "win",
          'defines': [
            'NON_WINDOWS_DEFINE',
          ],
        }],
      ],  # conditions
    },  # target usrsctp
  ],  # targets
}
