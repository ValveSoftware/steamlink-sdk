# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'crnet',
      'type': 'static_library',
      'dependencies': [
        '../../components/prefs/prefs.gyp:prefs',
        '../../ios/net/ios_net.gyp:ios_net',
        '../../ios/web/ios_web.gyp:user_agent',
        '../../net/net.gyp:net',
        'crnet_resources',
      ],
      'mac_framework_headers': [
        'CrNet.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'sources': [
        # TODO(ellyjones): http://crbug.com/485144
        '../../net/url_request/sdch_dictionary_fetcher.cc',
        '../../net/url_request/sdch_dictionary_fetcher.h',
        'CrNet.h',
        'CrNet.mm',
        'crnet_environment.h',
        'crnet_environment.mm',
        'sdch_owner_pref_storage.cc',
        'sdch_owner_pref_storage.h',
      ],
      'defines': [
        # TODO(stuartmorgan): Revisit the way this is set, and the above is
        # built, once the web/ layer is complete. Note that this setting doesn't
        # propagate to any included targets.
        'CRNET=1',
      ],
      'xcode_settings': {
        'DEAD_CODE_STRIPPING': 'YES',
      },
    },
    {
      'target_name': 'crnet_framework',
      'product_name': 'CrNet',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [
        'CrNet.h',
        'CrNet.mm',
        'crnet_environment.h',
        'crnet_environment.mm',
        'sdch_owner_pref_storage.cc',
        'sdch_owner_pref_storage.h',
        'sdch_owner_pref_storage.cc',
      ],
      'mac_framework_headers': [
        'CrNet.h',
      ],
      'link_settings': {
        'libraries': [
          'Foundation.framework',
        ],
      },
      'xcode_settings': {
        'DEBUGGING_SYMBOLS': 'YES',
        'INFOPLIST_FILE': 'Info.plist',
        'LD_DYLIB_INSTALL_NAME': '@loader_path/Frameworks/CrNet.framework/CrNet',
      },
      'dependencies': [
        '../../base/base.gyp:base',
        '../../components/prefs/prefs.gyp:prefs',
        '../../ios/net/ios_net.gyp:ios_net',
        '../../ios/web/ios_web.gyp:user_agent',
        '../../net/net.gyp:net',
        'crnet_resources',
      ],
      'configurations': {
        'Debug_Base': {
          'xcode_settings': {
            'DEPLOYMENT_POSTPROCESSING': 'NO',
            'DEBUG_INFORMATION_FORMAT': 'dwarf',
            'STRIP_INSTALLED_PRODUCT': 'NO',
          }
        },
        'Release_Base': {
          'xcode_settings': {
            'DEPLOYMENT_POSTPROCESSING': 'YES',
            'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
            'STRIP_INSTALLED_PRODUCT': 'YES',
            'STRIP_STYLE': 'non-global',
          }
        },
      },
    },
    {
      # This bundle contains "Accept-Languages" header values for known locales.
      # TODO(huey): These strings should be auto-generated from chrome's .xtb
      # files, not hardcoded.
      'target_name': 'crnet_resources',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'Resources/Localization/am.lproj/Localizable.strings',
        'Resources/Localization/ar.lproj/Localizable.strings',
        'Resources/Localization/bg.lproj/Localizable.strings',
        'Resources/Localization/bn.lproj/Localizable.strings',
        'Resources/Localization/ca.lproj/Localizable.strings',
        'Resources/Localization/cs.lproj/Localizable.strings',
        'Resources/Localization/da.lproj/Localizable.strings',
        'Resources/Localization/de.lproj/Localizable.strings',
        'Resources/Localization/el.lproj/Localizable.strings',
        'Resources/Localization/en-GB.lproj/Localizable.strings',
        'Resources/Localization/en.lproj/Localizable.strings',
        'Resources/Localization/es-419.lproj/Localizable.strings',
        'Resources/Localization/es.lproj/Localizable.strings',
        'Resources/Localization/fa.lproj/Localizable.strings',
        'Resources/Localization/fi.lproj/Localizable.strings',
        'Resources/Localization/fil.lproj/Localizable.strings',
        'Resources/Localization/fr.lproj/Localizable.strings',
        'Resources/Localization/gu.lproj/Localizable.strings',
        'Resources/Localization/he.lproj/Localizable.strings',
        'Resources/Localization/hi.lproj/Localizable.strings',
        'Resources/Localization/hr.lproj/Localizable.strings',
        'Resources/Localization/hu.lproj/Localizable.strings',
        'Resources/Localization/id.lproj/Localizable.strings',
        'Resources/Localization/it.lproj/Localizable.strings',
        'Resources/Localization/ja.lproj/Localizable.strings',
        'Resources/Localization/kn.lproj/Localizable.strings',
        'Resources/Localization/ko.lproj/Localizable.strings',
        'Resources/Localization/lt.lproj/Localizable.strings',
        'Resources/Localization/lv.lproj/Localizable.strings',
        'Resources/Localization/ml.lproj/Localizable.strings',
        'Resources/Localization/mr.lproj/Localizable.strings',
        'Resources/Localization/ms.lproj/Localizable.strings',
        'Resources/Localization/nb.lproj/Localizable.strings',
        'Resources/Localization/nl.lproj/Localizable.strings',
        'Resources/Localization/pl.lproj/Localizable.strings',
        'Resources/Localization/pt-BR.lproj/Localizable.strings',
        'Resources/Localization/pt-PT.lproj/Localizable.strings',
        'Resources/Localization/pt.lproj/Localizable.strings',
        'Resources/Localization/ro.lproj/Localizable.strings',
        'Resources/Localization/ru.lproj/Localizable.strings',
        'Resources/Localization/sk.lproj/Localizable.strings',
        'Resources/Localization/sl.lproj/Localizable.strings',
        'Resources/Localization/sr.lproj/Localizable.strings',
        'Resources/Localization/sv.lproj/Localizable.strings',
        'Resources/Localization/sw.lproj/Localizable.strings',
        'Resources/Localization/ta.lproj/Localizable.strings',
        'Resources/Localization/te.lproj/Localizable.strings',
        'Resources/Localization/th.lproj/Localizable.strings',
        'Resources/Localization/tr.lproj/Localizable.strings',
        'Resources/Localization/uk.lproj/Localizable.strings',
        'Resources/Localization/vi.lproj/Localizable.strings',
        'Resources/Localization/zh-Hans.lproj/Localizable.strings',
        'Resources/Localization/zh-Hant.lproj/Localizable.strings',
        'Resources/Localization/zh.lproj/Localizable.strings',
      ],
      'all_dependent_settings': {
        'link_settings': {
          'mac_bundle_resources': [
            '>(PRODUCT_DIR)/crnet_resources.bundle',
          ],
        },
      },
    },
  ],
}
