# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': [
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'usrsctplib_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libusrsctplib_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'dependencies': [
        '<(DEPTH)/native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
        '<(DEPTH)/third_party/boringssl/boringssl_nacl.gyp:boringssl_nacl',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
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
        'usrsctplib/user_socket.c',
        'usrsctplib/user_socketvar.h',
        'usrsctplib/user_uma.h',
        'usrsctplib/usrsctp.h'
      ],  # sources
      'defines': [
        'SCTP_USE_OPENSSL_SHA1',
        'SCTP_PROCESS_LEVEL_LOCKS',
        'SCTP_SIMPLE_ALLOCATOR',
        '__Userspace__',
        '__Userspace_os_NaCl',
        'NON_WINDOWS_DEFINE',
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
      'pnacl_cflags!': [ '-Werror', '-Wall' ],
      'pnacl_cflags': [ '-w' ],
    },  # target usrsctp
  ],  # targets
}
