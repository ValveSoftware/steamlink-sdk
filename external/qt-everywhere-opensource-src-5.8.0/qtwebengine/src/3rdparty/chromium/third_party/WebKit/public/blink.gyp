#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
{
    'includes': [
        '../Source/build/features.gypi',
    ],
    'variables': {
        # Duplicated in GN: //third_party/WebKit/public:mojo_bindings
        'blink_mojo_sources': [
            'platform/mime_registry.mojom',
            'platform/modules/background_sync/background_sync.mojom',
            'platform/modules/bluetooth/web_bluetooth.mojom',
            'platform/modules/geolocation/geolocation.mojom',
            'platform/modules/notifications/notification.mojom',
            'platform/modules/notifications/notification_service.mojom',
            'platform/modules/offscreencanvas/offscreen_canvas_surface.mojom',
            'platform/modules/permissions/permission.mojom',
            'platform/modules/permissions/permission_status.mojom',
            'platform/modules/presentation/presentation.mojom',
            'platform/modules/serviceworker/service_worker_event_status.mojom',
            'platform/modules/wake_lock/wake_lock_service.mojom',
        ],
        # Duplicated in GN: //third_party/WebKit/public:android_mojo_bindings
        'blink_android_mojo_sources': [
            'platform/modules/payments/payment_request.mojom',
        ],
    },
    'targets': [
        {
            # GN version: //third_party/WebKit/public:blink
            'target_name': 'blink',
            'type': 'none',
            'dependencies': [
                'mojo_bindings',
                '../Source/platform/blink_platform.gyp:blink_platform',
                '../Source/web/web.gyp:blink_web',
                '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
                'blink_headers.gyp:blink_headers',
                'blink_minimal',
            ],
            'export_dependent_settings': [
                '../Source/web/web.gyp:blink_web',
                '../Source/platform/blink_platform.gyp:blink_platform',
                'blink_minimal',

                # public/platform/Platform.h in 'blink_headers' depends on Mojo
                # APIs, and the dependent of this target needs to link Mojo.
                '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
            ],
        },
        {
            # This target provides a minimal set of Blink APIs such as WebString to use in
            # places that cannot link against the full Blink library.
            # FIXME: We really shouldn't have this at all and should instead remove all uses
            # of Blink's API types from places that can't link against Blink. crbug.com/248653
            #
            # GN version: //third_party/WebKit/public:blink_minimal
            'target_name': 'blink_minimal',
            'type': 'none',
            'dependencies': [
                '../Source/platform/blink_platform.gyp:blink_common',
            ],
            'export_dependent_settings': [
                '../Source/platform/blink_platform.gyp:blink_common',
            ],
        },
        {
            # GN version: //third_party/WebKit/public:test_support
            'target_name': 'blink_test_support',
            'type': 'none',
#            'dependencies': [
#                '../Source/platform/blink_platform_tests.gyp:blink_platform_test_support',
#                '../Source/web/web.gyp:blink_web_test_support',
#            ],
#            'export_dependent_settings': [
#                '../Source/platform/blink_platform_tests.gyp:blink_platform_test_support',
#                '../Source/web/web.gyp:blink_web_test_support',
#            ],
        },
        {
            # GN version: //third_party/WebKit/public:mojo_bindings_blink
            # (generated by //third_party/WebKit/public:mojo_bindings)
            'target_name': 'mojo_bindings_blink_mojom',
            'type': 'none',
            'variables': {
                'mojom_files': [
                    '<@(blink_mojo_sources)',
                    '<@(blink_android_mojo_sources)',
                ],
                'mojom_typemaps': [
                    '<(DEPTH)/cc/ipc/surface_id.typemap',
                    '<(DEPTH)/cc/ipc/surface_sequence.typemap',

                ],
                'for_blink': 'true',
            },
            'dependencies' : [
                '<(DEPTH)/cc/ipc/cc_ipc.gyp:interfaces_blink',
            ],
            'includes': [
                '../../../mojo/mojom_bindings_generator_explicit.gypi',
            ],
        },
        {
            # GN version: //third_party/WebKit/public:mojo_bindings
            'target_name': 'mojo_bindings_mojom',
            'type': 'none',
            'variables': {
                'mojom_files': [
                    '<@(blink_mojo_sources)',
                    '<@(blink_android_mojo_sources)',
                ],
                'mojom_typemaps': [
                  '../../../device/bluetooth/public/interfaces/bluetooth_uuid.typemap',
                    '<(DEPTH)/cc/ipc/surface_id.typemap',
                    '<(DEPTH)/cc/ipc/surface_sequence.typemap',
                ],
            },
            'dependencies' : [
                '<(DEPTH)/cc/ipc/cc_ipc.gyp:interfaces',
            ],
            'includes': [
                '../../../mojo/mojom_bindings_generator_explicit.gypi',
            ],
        },
        {
            # GN version: //third_party/WebKit/public:mojo_bindings
            'target_name': 'mojo_bindings',
            # Needed because of dependency on generated headers.
            'hard_dependency': '1',
            'type': 'static_library',
            'dependencies': [
                'mojo_bindings_blink_mojom',
                'mojo_bindings_mojom',
                '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
                '../../../device/bluetooth/bluetooth.gyp:bluetooth_mojom',
                '<(DEPTH)/cc/ipc/cc_ipc.gyp:interfaces',
                '<(DEPTH)/cc/ipc/cc_ipc.gyp:interfaces_blink',
            ],
        },
    ],
    'conditions': [
        ['OS == "android"', {
            'targets': [
                {
                    'target_name': 'android_mojo_bindings_mojom',
                    'type': 'none',
                    'variables': {
                        'mojom_files': ['<@(blink_android_mojo_sources)'],
                    },
                    'includes': [
                        '../../../mojo/mojom_bindings_generator_explicit.gypi',
                    ],
                },
                {
                    # GN version: //third_party/WebKit/public:android_mojo_bindings_java
                    'target_name': 'android_mojo_bindings_java',
                    'type': 'static_library',
                    'dependencies': [
                        'android_mojo_bindings_mojom',
                        '../../../mojo/mojo_public.gyp:mojo_bindings_java',
                    ],
                },
            ],
        }],
    ],
}
