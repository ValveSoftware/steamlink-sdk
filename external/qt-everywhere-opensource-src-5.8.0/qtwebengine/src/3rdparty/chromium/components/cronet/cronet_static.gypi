# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # Included in 'cronet_static'.
  'dependencies': [
    '../base/base.gyp:base',
    '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
    '../url/url.gyp:url_url_features',
    'chromium_url_request_java',
    'cronet_android_cert_proto',
    'cronet_jni_headers',
    'cronet_version',
    'cronet_version_header',
    'metrics',
    'url_request_error_java',
  ],
  'sources': [
    'android/cert/cert_verifier_cache_serializer.cc',
    'android/cert/cert_verifier_cache_serializer.h',
    'android/chromium_url_request.cc',
    'android/chromium_url_request.h',
    'android/chromium_url_request_context.cc',
    'android/chromium_url_request_context.h',
    'android/cronet_bidirectional_stream_adapter.cc',
    'android/cronet_bidirectional_stream_adapter.h',
    'android/cronet_in_memory_pref_store.cc',
    'android/cronet_in_memory_pref_store.h',
    'android/cronet_library_loader.cc',
    'android/cronet_library_loader.h',
    'android/cronet_upload_data_stream.cc',
    'android/cronet_upload_data_stream.h',
    'android/cronet_upload_data_stream_adapter.cc',
    'android/cronet_upload_data_stream_adapter.h',
    'android/cronet_url_request_adapter.cc',
    'android/cronet_url_request_adapter.h',
    'android/cronet_url_request_context_adapter.cc',
    'android/cronet_url_request_context_adapter.h',
    'android/io_buffer_with_byte_buffer.cc',
    'android/io_buffer_with_byte_buffer.h',
    'android/url_request_adapter.cc',
    'android/url_request_adapter.h',
    'android/url_request_context_adapter.cc',
    'android/url_request_context_adapter.h',
    'android/url_request_error.cc',
    'android/url_request_error.h',
    'android/wrapped_channel_upload_element_reader.cc',
    'android/wrapped_channel_upload_element_reader.h',
    'histogram_manager.cc',
    'histogram_manager.h',
    'url_request_context_config.cc',
    'url_request_context_config.h',
  ],
  'cflags': [
    '-DLOGGING=1',
    '-fdata-sections',
    '-ffunction-sections',
    '-fno-rtti',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-Wno-sign-promo',
    '-Wno-missing-field-initializers',
  ],
  'ldflags': [
    '-llog',
    '-landroid',
    '-Wl,--gc-sections',
    '-Wl,--exclude-libs,ALL'
  ],
  'conditions': [
    # If Data Reduction Proxy support is enabled, add the following
    # defines and sources. Dependencies are target-specific and are
    # not included here.
    ['enable_data_reduction_proxy_support==1',
      {
        'defines' : [
          'DATA_REDUCTION_PROXY_SUPPORT'
        ],
        'sources': [
          'android/cronet_data_reduction_proxy.cc',
          'android/cronet_data_reduction_proxy.h',
        ],
      }
    ],
  ],
}
