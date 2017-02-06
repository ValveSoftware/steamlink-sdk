# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/autofill/core/common
      'target_name': 'autofill_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'autofill/core/common/autofill_constants.cc',
        'autofill/core/common/autofill_constants.h',
        'autofill/core/common/autofill_data_validation.cc',
        'autofill/core/common/autofill_data_validation.h',
        'autofill/core/common/autofill_l10n_util.cc',
        'autofill/core/common/autofill_l10n_util.h',
        'autofill/core/common/autofill_pref_names.cc',
        'autofill/core/common/autofill_pref_names.h',
        'autofill/core/common/autofill_regexes.cc',
        'autofill/core/common/autofill_regexes.h',
        'autofill/core/common/autofill_switches.cc',
        'autofill/core/common/autofill_switches.h',
        'autofill/core/common/autofill_util.cc',
        'autofill/core/common/autofill_util.h',
        'autofill/core/common/form_data.cc',
        'autofill/core/common/form_data.h',
        'autofill/core/common/form_data_predictions.cc',
        'autofill/core/common/form_data_predictions.h',
        'autofill/core/common/form_field_data.cc',
        'autofill/core/common/form_field_data.h',
        'autofill/core/common/form_field_data_predictions.cc',
        'autofill/core/common/form_field_data_predictions.h',
        'autofill/core/common/password_form.cc',
        'autofill/core/common/password_form.h',
        'autofill/core/common/password_form_field_prediction_map.h',
        'autofill/core/common/password_form_fill_data.cc',
        'autofill/core/common/password_form_fill_data.h',
        'autofill/core/common/password_form_generation_data.h',
        'autofill/core/common/password_generation_util.cc',
        'autofill/core/common/password_generation_util.h',
        'autofill/core/common/save_password_progress_logger.cc',
        'autofill/core/common/save_password_progress_logger.h',
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },

    {
      # GN version: //components/autofill/core/browser
      'target_name': 'autofill_core_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sql/sql.gyp:sql',
        '../sync/sync.gyp:sync',
        '../third_party/fips181/fips181.gyp:fips181',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_util',
        '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber',
        '../third_party/re2/re2.gyp:re2',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_range',
        '../ui/gfx/gfx.gyp:gfx_vector_icons',
        '../url/url.gyp:url_lib',
        'autofill_core_common',
        'autofill_server_proto',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        'data_use_measurement_core',
        'infobars_core',
        'keyed_service_core',
        'os_crypt',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'rappor',
        'signin_core_browser',
        'signin_core_common',
        'sync_driver',
        'variations_net',
        'webdata_common',
      ],
      'sources': [
        'autofill/core/browser/address.cc',
        'autofill/core/browser/address.h',
        'autofill/core/browser/address_field.cc',
        'autofill/core/browser/address_field.h',
        'autofill/core/browser/address_i18n.cc',
        'autofill/core/browser/address_i18n.h',
        'autofill/core/browser/address_rewriter.cc',
        'autofill/core/browser/address_rewriter.h',
        'autofill/core/browser/address_rewriter_rules.cc',
        'autofill/core/browser/autocomplete_history_manager.cc',
        'autofill/core/browser/autocomplete_history_manager.h',
        'autofill/core/browser/autofill-inl.h',
        'autofill/core/browser/autofill_client.h',
        'autofill/core/browser/autofill_country.cc',
        'autofill/core/browser/autofill_country.h',
        'autofill/core/browser/autofill_data_model.cc',
        'autofill/core/browser/autofill_data_model.h',
        'autofill/core/browser/autofill_data_util.cc',
        'autofill/core/browser/autofill_data_util.h',
        'autofill/core/browser/autofill_download_manager.cc',
        'autofill/core/browser/autofill_download_manager.h',
        'autofill/core/browser/autofill_driver.h',
        'autofill/core/browser/autofill_experiments.cc',
        'autofill/core/browser/autofill_experiments.h',
        'autofill/core/browser/autofill_external_delegate.cc',
        'autofill/core/browser/autofill_external_delegate.h',
        'autofill/core/browser/autofill_field.cc',
        'autofill/core/browser/autofill_field.h',
        'autofill/core/browser/autofill_ie_toolbar_import_win.cc',
        'autofill/core/browser/autofill_ie_toolbar_import_win.h',
        'autofill/core/browser/autofill_manager.cc',
        'autofill/core/browser/autofill_manager.h',
        'autofill/core/browser/autofill_manager_test_delegate.h',
        'autofill/core/browser/autofill_metrics.cc',
        'autofill/core/browser/autofill_metrics.h',
        'autofill/core/browser/autofill_popup_delegate.h',
        'autofill/core/browser/autofill_profile.cc',
        'autofill/core/browser/autofill_profile.h',
        'autofill/core/browser/autofill_profile_comparator.cc',
        'autofill/core/browser/autofill_profile_comparator.h',
        'autofill/core/browser/autofill_regex_constants.cc',
        'autofill/core/browser/autofill_regex_constants.h',
        'autofill/core/browser/autofill_scanner.cc',
        'autofill/core/browser/autofill_scanner.h',
        'autofill/core/browser/autofill_sync_constants.cc',
        'autofill/core/browser/autofill_sync_constants.h',
        'autofill/core/browser/autofill_type.cc',
        'autofill/core/browser/autofill_type.h',
        'autofill/core/browser/autofill_wallet_data_type_controller.cc',
        'autofill/core/browser/autofill_wallet_data_type_controller.h',
        'autofill/core/browser/card_unmask_delegate.cc',
        'autofill/core/browser/card_unmask_delegate.h',
        'autofill/core/browser/contact_info.cc',
        'autofill/core/browser/contact_info.h',
        'autofill/core/browser/country_data.cc',
        'autofill/core/browser/country_data.h',
        'autofill/core/browser/country_names.cc',
        'autofill/core/browser/country_names.h',
        'autofill/core/browser/credit_card.cc',
        'autofill/core/browser/credit_card.h',
        'autofill/core/browser/credit_card_field.cc',
        'autofill/core/browser/credit_card_field.h',
        'autofill/core/browser/detail_input.cc',
        'autofill/core/browser/detail_input.h',
        'autofill/core/browser/dialog_section.h',
        'autofill/core/browser/email_field.cc',
        'autofill/core/browser/email_field.h',
        'autofill/core/browser/field_candidates.h',
        'autofill/core/browser/field_candidates.cc',
        'autofill/core/browser/field_types.h',
        'autofill/core/browser/form_field.cc',
        'autofill/core/browser/form_field.h',
        'autofill/core/browser/form_group.cc',
        'autofill/core/browser/form_group.h',
        'autofill/core/browser/form_structure.cc',
        'autofill/core/browser/form_structure.h',
        'autofill/core/browser/legal_message_line.cc',
        'autofill/core/browser/legal_message_line.h',
        'autofill/core/browser/name_field.cc',
        'autofill/core/browser/name_field.h',
        'autofill/core/browser/password_generator.cc',
        'autofill/core/browser/password_generator.h',
        'autofill/core/browser/payments/full_card_request.cc',
        'autofill/core/browser/payments/full_card_request.h',
        'autofill/core/browser/payments/payments_client.cc',
        'autofill/core/browser/payments/payments_client.h',
        'autofill/core/browser/payments/payments_request.h',
        'autofill/core/browser/payments/payments_service_url.cc',
        'autofill/core/browser/payments/payments_service_url.h',
        'autofill/core/browser/personal_data_manager.cc',
        'autofill/core/browser/personal_data_manager.h',
        'autofill/core/browser/personal_data_manager_observer.h',
        'autofill/core/browser/phone_field.cc',
        'autofill/core/browser/phone_field.h',
        'autofill/core/browser/phone_number.cc',
        'autofill/core/browser/phone_number.h',
        'autofill/core/browser/phone_number_i18n.cc',
        'autofill/core/browser/phone_number_i18n.h',
        'autofill/core/browser/popup_item_ids.h',
        'autofill/core/browser/server_field_types_util.cc',
        'autofill/core/browser/server_field_types_util.h',
        'autofill/core/browser/state_names.cc',
        'autofill/core/browser/state_names.h',
        'autofill/core/browser/suggestion.cc',
        'autofill/core/browser/suggestion.h',
        'autofill/core/browser/ui/card_unmask_prompt_controller.h',
        'autofill/core/browser/ui/card_unmask_prompt_controller_impl.cc',
        'autofill/core/browser/ui/card_unmask_prompt_controller_impl.h',
        'autofill/core/browser/ui/card_unmask_prompt_view.h',
        'autofill/core/browser/validation.cc',
        'autofill/core/browser/validation.h',
        'autofill/core/browser/webdata/autocomplete_syncable_service.cc',
        'autofill/core/browser/webdata/autocomplete_syncable_service.h',
        'autofill/core/browser/webdata/autofill_change.cc',
        'autofill/core/browser/webdata/autofill_change.h',
        'autofill/core/browser/webdata/autofill_data_type_controller.cc',
        'autofill/core/browser/webdata/autofill_data_type_controller.h',
        'autofill/core/browser/webdata/autofill_entry.cc',
        'autofill/core/browser/webdata/autofill_entry.h',
        'autofill/core/browser/webdata/autofill_profile_data_type_controller.cc',
        'autofill/core/browser/webdata/autofill_profile_data_type_controller.h',
        'autofill/core/browser/webdata/autofill_profile_syncable_service.cc',
        'autofill/core/browser/webdata/autofill_profile_syncable_service.h',
        'autofill/core/browser/webdata/autofill_table.cc',
        'autofill/core/browser/webdata/autofill_table.h',
        'autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.cc',
        'autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h',
        'autofill/core/browser/webdata/autofill_wallet_syncable_service.cc',
        'autofill/core/browser/webdata/autofill_wallet_syncable_service.h',
        'autofill/core/browser/webdata/autofill_webdata.h',
        'autofill/core/browser/webdata/autofill_webdata_backend.h',
        'autofill/core/browser/webdata/autofill_webdata_backend_impl.cc',
        'autofill/core/browser/webdata/autofill_webdata_backend_impl.h',
        'autofill/core/browser/webdata/autofill_webdata_service.cc',
        'autofill/core/browser/webdata/autofill_webdata_service.h',
        'autofill/core/browser/webdata/autofill_webdata_service_observer.h',
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      # This is needed because GYP's handling of transitive dependencies is
      # not great. See https://goo.gl/QGtlae for details.
      'export_dependent_settings': [
        'autofill_server_proto',
      ],

      'conditions': [
        ['OS=="ios"', {
          'sources': [
            'autofill/core/browser/autofill_field_trial_ios.cc',
            'autofill/core/browser/autofill_field_trial_ios.h',
            'autofill/core/browser/keyboard_accessory_metrics_logger.h',
            'autofill/core/browser/keyboard_accessory_metrics_logger.mm',
          ],
        }],
        ['OS=="ios" or OS=="android"', {
          'sources': [
            'autofill/core/browser/autofill_save_card_infobar_delegate_mobile.cc',
            'autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h',
            'autofill/core/browser/autofill_save_card_infobar_mobile.h',
          ],
        }]
      ],
    },

    {
      # Protobuf compiler / generate rule for Autofill's server proto.
      # GN version: //components/autofill/core/browser/proto
      'target_name': 'autofill_server_proto',
      'type': 'static_library',
      'sources': [
        'autofill/core/browser/proto/server.proto',
      ],
      'variables': {
        'proto_in_dir': 'autofill/core/browser/proto',
        'proto_out_dir': 'components/autofill/core/browser/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },

    {
      # GN version: //components/autofill/core/browser:test_support
      'target_name': 'autofill_core_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        'autofill_core_common',
        'autofill_core_browser',
        'os_crypt',
        'pref_registry',
        'prefs/prefs.gyp:prefs',
        'rappor',
        'signin_core_browser_test_support',
      ],
      'sources': [
        'autofill/core/browser/autofill_test_utils.cc',
        'autofill/core/browser/autofill_test_utils.h',
        'autofill/core/browser/data_driven_test.cc',
        'autofill/core/browser/data_driven_test.h',
        'autofill/core/browser/suggestion_test_helpers.h',
        'autofill/core/browser/test_autofill_client.cc',
        'autofill/core/browser/test_autofill_client.h',
        'autofill/core/browser/test_autofill_driver.cc',
        'autofill/core/browser/test_autofill_driver.h',
        'autofill/core/browser/test_autofill_external_delegate.cc',
        'autofill/core/browser/test_autofill_external_delegate.h',
        'autofill/core/browser/test_personal_data_manager.cc',
        'autofill/core/browser/test_personal_data_manager.h',
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/autofill/content/public/interfaces:types
          'target_name': 'autofill_content_types_mojo_bindings_mojom',
          'type': 'none',
          'variables': {
            'mojom_files': [
              'autofill/content/public/interfaces/autofill_types.mojom',
            ],
            'mojom_typemaps': [
              'autofill/content/public/cpp/autofill_types.typemap',
              '<(DEPTH)/url/mojo/gurl.typemap',
            ],
          },
          'includes': [ '../mojo/mojom_bindings_generator_explicit.gypi' ],
          'dependencies': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
          ],
        },
        {
          # GN version: //components/autofill/content/public/interfaces:types
          'target_name': 'autofill_content_types_mojo_bindings',
          'type': 'static_library',
          'sources': [
            'autofill/content/public/cpp/autofill_types_struct_traits.cc'
          ],
          'export_dependent_settings': [
            '../url/url.gyp:url_mojom',
           ],
          'dependencies': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
            '../url/url.gyp:url_mojom',
            'autofill_content_types_mojo_bindings_mojom',
          ],
        },
        {
          # GN version: //components/autofill/content/public/interfaces:test_types
          'target_name': 'autofill_content_test_types_mojo_bindings',
          'type': 'static_library',
          'variables': {
            'mojom_typemaps': [
              'autofill/content/public/cpp/autofill_types.typemap',
              '<(DEPTH)/url/mojo/gurl.typemap',
            ],
          },
          'sources': [
            'autofill/content/public/interfaces/test_autofill_types.mojom',
          ],
          'export_dependent_settings': [
            '../url/url.gyp:url_mojom',
            'autofill_content_types_mojo_bindings',
           ],
          'dependencies': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
            '../url/url.gyp:url_mojom',
            'autofill_content_types_mojo_bindings',
          ],
          'includes': [ '../mojo/mojom_bindings_generator.gypi' ],
        },
        {
          # GN version: //components/autofill/content/public/interfaces
          'target_name': 'autofill_content_mojo_bindings_mojom',
          'type': 'none',
          'variables': {
            'mojom_files': [
              'autofill/content/public/interfaces/autofill_agent.mojom',
              'autofill/content/public/interfaces/autofill_driver.mojom',
            ],
            'mojom_typemaps': [
              'autofill/content/public/cpp/autofill_types.typemap',
            ],
          },
          'include_dirs': [
            '..',
          ],
          'includes': [
            '../mojo/mojom_bindings_generator_explicit.gypi',
          ],
        },
        {
          # GN version: //components/autofill/content/public/interfaces
          'target_name': 'autofill_content_mojo_bindings',
          'type': 'static_library',
          'export_dependent_settings': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
           ],
          'dependencies': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
            'autofill_content_mojo_bindings_mojom',
            'autofill_content_types_mojo_bindings',
          ],
        },
        {
          # GN version: //content/autofill/content/common
          'target_name': 'autofill_content_common',
          'type': 'static_library',
          'dependencies': [
            'autofill_core_common',
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
            '../third_party/WebKit/public/blink.gyp:blink_minimal',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
            '../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
            '../ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
            '../url/url.gyp:url_lib',
            '../url/ipc/url_ipc.gyp:url_ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'autofill/content/common/autofill_message_generator.cc',
            'autofill/content/common/autofill_message_generator.h',
            'autofill/content/common/autofill_messages.h',
            'autofill/content/common/autofill_param_traits_macros.h',
          ],
        },

        {
          # Protobuf compiler / generate rule for Autofill's risk integration.
          # GN version: //components/autofill/content/browser:risk_proto
          'target_name': 'autofill_content_risk_proto',
          'type': 'static_library',
          'sources': [
            'autofill/content/browser/risk/proto/fingerprint.proto',
          ],
          'variables': {
            'proto_in_dir': 'autofill/content/browser/risk/proto',
            'proto_out_dir': 'components/autofill/content/browser/risk/proto',
          },
          'includes': [ '../build/protoc.gypi' ]
        },
       {
         # GN version: //components/autofill/content/renderer:test_support
         'target_name': 'autofill_content_test_support',
         'type': 'static_library',
         'dependencies': [
           'autofill_content_browser',
           'autofill_content_renderer',
           '../base/base.gyp:base',
           '../ipc/ipc.gyp:ipc',
           '../skia/skia.gyp:skia',
           '../testing/gmock.gyp:gmock',
         ],
         'sources': [
           'autofill/content/renderer/test_password_autofill_agent.cc',
           'autofill/content/renderer/test_password_autofill_agent.h',
           'autofill/content/renderer/test_password_generation_agent.cc',
           'autofill/content/renderer/test_password_generation_agent.h',
         ],
         'include_dirs': [ '..' ],
       },
       {
          # GN version: //components/autofill/content/browser
          'target_name': 'autofill_content_browser',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            '../google_apis/google_apis.gyp:google_apis',
            '../ipc/ipc.gyp:ipc',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../sql/sql.gyp:sql',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/display/display.gyp:display',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../url/url.gyp:url_lib',
            'autofill_content_common',
            'autofill_content_mojo_bindings',
            'autofill_content_risk_proto',
            'autofill_core_browser',
            'autofill_core_common',
            'autofill_server_proto',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'os_crypt',
            'prefs/prefs.gyp:prefs',
            'user_prefs',
            'webdata_common',
          ],
          'sources': [
            'autofill/content/browser/content_autofill_driver.cc',
            'autofill/content/browser/content_autofill_driver.h',
            'autofill/content/browser/content_autofill_driver_factory.cc',
            'autofill/content/browser/content_autofill_driver_factory.h',
            'autofill/content/browser/risk/fingerprint.cc',
            'autofill/content/browser/risk/fingerprint.h',
          ],

          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
          # This is needed because GYP's handling of transitive dependencies is
          # not great. See https://goo.gl/QGtlae for details.
          'export_dependent_settings': [
            'autofill_server_proto',
          ],
        },

        {
          # GN version: //components/autofill/content/renderer
          'target_name': 'autofill_content_renderer',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_renderer',
            '../google_apis/google_apis.gyp:google_apis',
            '../ipc/ipc.gyp:ipc',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../third_party/re2/re2.gyp:re2',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../ui/base/ui_base.gyp:ui_base',
            'autofill_content_common',
            'autofill_content_mojo_bindings',
            'autofill_core_common',
            'components_strings.gyp:components_strings',
          ],
          'sources': [
            'autofill/content/renderer/autofill_agent.cc',
            'autofill/content/renderer/autofill_agent.h',
            'autofill/content/renderer/form_autofill_util.cc',
            'autofill/content/renderer/form_autofill_util.h',
            'autofill/content/renderer/form_cache.cc',
            'autofill/content/renderer/form_cache.h',
            'autofill/content/renderer/form_classifier.cc',
            'autofill/content/renderer/form_classifier.h',
            'autofill/content/renderer/page_click_listener.h',
            'autofill/content/renderer/page_click_tracker.cc',
            'autofill/content/renderer/page_click_tracker.h',
            'autofill/content/renderer/password_autofill_agent.cc',
            'autofill/content/renderer/password_autofill_agent.h',
            'autofill/content/renderer/password_form_conversion_utils.cc',
            'autofill/content/renderer/password_form_conversion_utils.h',
            'autofill/content/renderer/password_generation_agent.cc',
            'autofill/content/renderer/password_generation_agent.h',
            'autofill/content/renderer/renderer_save_password_progress_logger.cc',
            'autofill/content/renderer/renderer_save_password_progress_logger.h',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS == "ios"', {
      'targets': [
        {
          # GN version: //components/autofill/ios/browser
          'target_name': 'autofill_ios_browser',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'autofill_core_browser',
            'autofill_core_common',
            'autofill_ios_injected_js',
            'autofill_server_proto',
            '../ios/provider/ios_provider_web.gyp:ios_provider_web',
            '../ios/web/ios_web.gyp:ios_web',
          ],
          'sources': [
            'autofill/ios/browser/autofill_client_ios_bridge.h',
            'autofill/ios/browser/autofill_driver_ios.h',
            'autofill/ios/browser/autofill_driver_ios.mm',
            'autofill/ios/browser/autofill_driver_ios_bridge.h',
            'autofill/ios/browser/credit_card_util.h',
            'autofill/ios/browser/credit_card_util.mm',
            'autofill/ios/browser/form_suggestion.h',
            'autofill/ios/browser/form_suggestion.mm',
            'autofill/ios/browser/js_autofill_manager.h',
            'autofill/ios/browser/js_autofill_manager.mm',
            'autofill/ios/browser/js_suggestion_manager.h',
            'autofill/ios/browser/js_suggestion_manager.mm',
            'autofill/ios/browser/personal_data_manager_observer_bridge.h',
            'autofill/ios/browser/personal_data_manager_observer_bridge.mm',
          ],
          # This is needed because GYP's handling of transitive dependencies is
          # not great. See https://goo.gl/QGtlae for details.
          'export_dependent_settings': [
            'autofill_server_proto',
          ],
        },
        {
          # GN version: //components/autofill/ios/browser:injected_js
          'target_name': 'autofill_ios_injected_js',
          'type': 'none',
          'sources': [
            'autofill/ios/browser/resources/autofill_controller.js',
            'autofill/ios/browser/resources/suggestion_controller.js',
          ],
          'link_settings': {
            'mac_bundle_resources': [
              '<(SHARED_INTERMEDIATE_DIR)/autofill_controller.js',
              '<(SHARED_INTERMEDIATE_DIR)/suggestion_controller.js',
            ],
          },
          'includes': [
            '../ios/web/js_compile_checked.gypi',
          ],
        },
      ],
    }],
  ],
}
