# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common_untrusted.gypi',
    'remoting_srcs.gypi',
  ],

  'variables': {
    'protoc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'proto_out_base': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'proto_out_dir': '<(proto_out_base)/remoting/proto',
    'use_nss_certs': 0,
    'nacl_untrusted_build': 1,
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'remoting_webrtc_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libremoting_webrtc_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'include_dirs': [
        '../third_party',
        '../third_party/webrtc',
      ],
      'sources': [
        '../third_party/webrtc/modules/desktop_capture/desktop_frame.cc',
        '../third_party/webrtc/modules/desktop_capture/desktop_frame.h',
        '../third_party/webrtc/modules/desktop_capture/desktop_geometry.cc',
        '../third_party/webrtc/modules/desktop_capture/desktop_geometry.h',
        '../third_party/webrtc/modules/desktop_capture/desktop_region.cc',
        '../third_party/webrtc/modules/desktop_capture/desktop_region.h',
        '../third_party/webrtc/modules/desktop_capture/shared_desktop_frame.cc',
        '../third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../third_party',
          '../third_party/webrtc',
        ],
      }
    },  # end of target 'remoting_webrtc_nacl'

    {
      'target_name': 'remoting_proto_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libremoting_proto_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'files_list': [
          '<(proto_out_dir)/audio.pb.cc',
          '<(proto_out_dir)/control.pb.cc',
          '<(proto_out_dir)/event.pb.cc',
          '<(proto_out_dir)/internal.pb.cc',
          '<(proto_out_dir)/mux.pb.cc',
          '<(proto_out_dir)/video.pb.cc',
        ],
        'extra_deps': [ '<@(files_list)' ],
        'extra_args': [ '<@(files_list)' ],
      },
      'defines': [
        'GOOGLE_PROTOBUF_HOST_ARCH_64_BIT=1'
      ],
      'dependencies': [
        '../third_party/protobuf/protobuf_nacl.gyp:protobuf_lite_nacl',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      'export_dependent_settings': [
        '../third_party/protobuf/protobuf_nacl.gyp:protobuf_lite_nacl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(proto_out_base)',
        ],
      },
    },  # end of target 'remoting_proto_nacl'

    {
      'target_name': 'remoting_client_plugin_lib_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nlib_target': 'libremoting_client_plugin_lib_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'dependencies': [
        '../base/base_nacl.gyp:base_nacl',
        '../jingle/jingle_nacl.gyp:jingle_glue_nacl',
        '../native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
        '../net/net_nacl.gyp:net_nacl',
        '../third_party/boringssl/boringssl_nacl.gyp:boringssl_nacl',
        '../third_party/expat/expat_nacl.gyp:expat_nacl',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        '../third_party/libjingle/libjingle_nacl.gyp:libjingle_nacl',
        '../third_party/libvpx/libvpx_nacl.gyp:libvpx_nacl',
        '../third_party/libyuv/libyuv_nacl.gyp:libyuv_nacl',
        '../third_party/opus/opus_nacl.gyp:opus_nacl',
        'remoting_proto_nacl',
        'remoting_webrtc_nacl',
      ],
      'sources': [
        '../ui/events/keycodes/dom/keycode_converter.cc',
        '<@(remoting_base_sources)',
        '<@(remoting_codec_sources)',
        '<@(remoting_client_plugin_sources)',
        '<@(remoting_client_sources)',
        '<@(remoting_protocol_sources)',
        '<@(remoting_signaling_sources)',
      ],
      'sources!': [
        'base/chromium_url_request.cc',
        'base/url_request_context_getter.cc',
      ],

      # Include normalizing_input_filter_*.cc excluded by the filename
      # exclusion rules. Must be in target_conditions to make sure it's
      # evaluated after the filename rules.
      'target_conditions': [
        ['1==1', {
          'sources/': [
            [ 'include', 'client/normalizing_input_filter_mac.cc' ],
            [ 'include', 'client/normalizing_input_filter_win.cc' ],
          ],
        }],
      ],
    },  # end of target 'remoting_client_plugin_lib_nacl'

    {
      'target_name': 'remoting_client_plugin_nacl',
      'type': 'none',
      'variables': {
        'nacl_untrusted_build': 1,
        'nexe_target': 'remoting_client_plugin',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'extra_deps_pnacl_newlib': [
          '>(tc_lib_dir_pnacl_newlib)/libbase_i18n_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libbase_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libexpat_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libicudata_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libcrypto_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libicui18n_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libicuuc_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libjingle_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libmodp_b64_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libboringssl_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libopus_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libppapi.a',
          '>(tc_lib_dir_pnacl_newlib)/libppapi_cpp.a',
          '>(tc_lib_dir_pnacl_newlib)/libprotobuf_lite_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libjingle_glue_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libnet_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libremoting_client_plugin_lib_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libremoting_proto_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libremoting_webrtc_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/liburl_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libvpx_nacl.a',
          '>(tc_lib_dir_pnacl_newlib)/libyuv_nacl.a',
        ],
      },
      'dependencies': [
        '../base/base_nacl.gyp:base_i18n_nacl',
        '../base/base_nacl.gyp:base_nacl',
        '../crypto/crypto_nacl.gyp:crypto_nacl',
        '../jingle/jingle_nacl.gyp:jingle_glue_nacl',
        '../native_client_sdk/native_client_sdk_untrusted.gyp:nacl_io_untrusted',
        '../net/net_nacl.gyp:net_nacl',
        '../ppapi/native_client/native_client.gyp:ppapi_lib',
        '../ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        '../third_party/expat/expat_nacl.gyp:expat_nacl',
        '../third_party/icu/icu_nacl.gyp:icudata_nacl',
        '../third_party/icu/icu_nacl.gyp:icui18n_nacl',
        '../third_party/icu/icu_nacl.gyp:icuuc_nacl',
        '../third_party/libjingle/libjingle_nacl.gyp:libjingle_nacl',
        '../third_party/libyuv/libyuv_nacl.gyp:libyuv_nacl',
        '../third_party/modp_b64/modp_b64_nacl.gyp:modp_b64_nacl',
        '../third_party/boringssl/boringssl_nacl.gyp:boringssl_nacl',
        '../url/url_nacl.gyp:url_nacl',
        'remoting_client_plugin_lib_nacl',
        'remoting_proto_nacl',
        'remoting_webrtc_nacl',
      ],
      'link_flags': [
        '-lppapi_stub',

        # Plugin code.
        '-lremoting_client_plugin_lib_nacl',
        '-lremoting_proto_nacl',

        # Chromium libraries.
        '-ljingle_glue_nacl',
        '-lnet_nacl',
        '-lcrypto_nacl',
        '-lbase_i18n_nacl',
        '-lbase_nacl',
        '-lurl_nacl',

        # Third-party libraries.
        '-lremoting_webrtc_nacl',
        '-lyuv_nacl',
        '-lvpx_nacl',
        '-ljingle_nacl',
        '-lexpat_nacl',
        '-lmodp_b64_nacl',
        '-lopus_nacl',
        '-lboringssl_nacl',
        '-licui18n_nacl',
        '-licuuc_nacl',
        '-licudata_nacl',
        '-lprotobuf_lite_nacl',

        # Base NaCl libraries.
        '-lppapi_cpp',
        '-lpthread',
        '-lnacl_io',
      ],
      'sources': [
        'client/plugin/pepper_module.cc',
      ],
    },  # end of target 'remoting_client_plugin_nacl'
  ]
}
