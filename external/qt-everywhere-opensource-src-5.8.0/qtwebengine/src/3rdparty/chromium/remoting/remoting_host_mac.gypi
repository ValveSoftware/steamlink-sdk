# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_host_installer_mac_roots': [
      'host/installer/mac/',
      '<(DEPTH)/chrome/installer/mac/',
    ],
    'remoting_host_installer_mac_files': [
      'host/installer/mac/do_signing.sh',
      'host/installer/mac/do_signing.props',
      'host/installer/mac/ChromotingHost.pkgproj',
      'host/installer/mac/ChromotingHostService.pkgproj',
      'host/installer/mac/ChromotingHostUninstaller.pkgproj',
      'host/installer/mac/LaunchAgents/org.chromium.chromoting.plist',
      'host/installer/mac/PrivilegedHelperTools/org.chromium.chromoting.me2me.sh',
      'host/installer/mac/Scripts/keystone_install.sh',
      'host/installer/mac/Scripts/remoting_postflight.sh',
      'host/installer/mac/Scripts/remoting_preflight.sh',
      'host/installer/mac/Keystone/GoogleSoftwareUpdate.pkg',
      '<(DEPTH)/chrome/installer/mac/pkg-dmg',
    ],
  },

  'targets': [
    {
      'target_name': 'remoting_host_uninstaller',
      'type': 'executable',
      'mac_bundle': 1,
      'variables': {
        'bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_BUNDLE_ID@")',
        'host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
        'prefpane_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
      },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'remoting_infoplist_strings',
      ],
      'defines': [
        'HOST_BUNDLE_NAME="<(host_bundle_name)"',
        'PREFPANE_BUNDLE_NAME="<(prefpane_bundle_name)"',
      ],
      'sources': [
        'host/constants_mac.cc',
        'host/constants_mac.h',
        'host/installer/mac/uninstaller/remoting_uninstaller.h',
        'host/installer/mac/uninstaller/remoting_uninstaller.mm',
        'host/installer/mac/uninstaller/remoting_uninstaller_app.h',
        'host/installer/mac/uninstaller/remoting_uninstaller_app.mm',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
        'INFOPLIST_PREPROCESS': 'YES',
        'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(bundle_id)"',
      },
      'mac_bundle_resources': [
        'host/installer/mac/uninstaller/remoting_uninstaller.icns',
        'host/installer/mac/uninstaller/remoting_uninstaller.xib',
        'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',

        # Localized strings for 'Info.plist'
        '<!@pymod_do_main(remoting_localize --locale_output '
        '"<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_uninstaller-InfoPlist.strings/@{json_suffix}.lproj/InfoPlist.strings" '
        '--print_only <(remoting_locales))',
      ],
      'mac_bundle_resources!': [
        'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
      ],
    },  # end of target 'remoting_host_uninstaller'

    # This packages up the files needed for the remoting host installer so
    # they can be sent off to be signed.
    # We don't build an installer here because we don't have signed binaries.
    {
      'target_name': 'remoting_me2me_host_archive',
      'type': 'none',
      'dependencies': [
        'remoting_host_prefpane',
        'remoting_host_uninstaller',
        'remoting_me2me_host',
        'remoting_me2me_native_messaging_host',
        'remoting_it2me_native_messaging_host',
        'remoting_native_messaging_manifests',
      ],
      'variables': {
        'host_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_PACKAGE_NAME@")',
        'host_service_name': '<!(python <(version_py_path) -f <(branding_path) -t "@DAEMON_FILE_NAME@")',
        'host_uninstaller_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_NAME@")',
        'bundle_prefix': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_BUNDLE_PREFIX@")',
        'me2me_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
        'prefpane_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
        'native_messaging_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_NATIVE_MESSAGING_HOST_BUNDLE_NAME@")',
        'remote_assistance_host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_REMOTE_ASSISTANCE_HOST_BUNDLE_NAME@")',
      },
      'actions': [
        {
          'action_name': 'Zip installer files for signing',
          'temp_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting-me2me-host',
          'zip_path': '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
          'variables': {
            'host_name_nospace': '<!(echo <(host_name) | sed "s/ //g")',
            'host_service_name_nospace': '<!(echo <(host_service_name) | sed "s/ //g")',
            'host_uninstaller_name_nospace': '<!(echo <(host_uninstaller_name) | sed "s/ //g")',
          },
          'generated_files': [
            '<(PRODUCT_DIR)/remoting_host_prefpane.prefPane',
            '<(PRODUCT_DIR)/remoting_me2me_host.app',
            '<(PRODUCT_DIR)/native_messaging_host.app',
            '<(PRODUCT_DIR)/remote_assistance_host.app',
            '<(PRODUCT_DIR)/remoting_host_uninstaller.app',
            '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_desktop.json',
            '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_assistance.json',
          ],
          'generated_files_dst': [
            'PreferencePanes/<(prefpane_bundle_name)',
            'PrivilegedHelperTools/<(me2me_host_bundle_name)',
            'PrivilegedHelperTools/<(me2me_host_bundle_name)/Contents/MacOS/<(native_messaging_host_bundle_name)',
            'PrivilegedHelperTools/<(me2me_host_bundle_name)/Contents/MacOS/<(remote_assistance_host_bundle_name)',
            'Applications/<(host_uninstaller_name).app',
            'Config/com.google.chrome.remote_desktop.json',
            'Config/com.google.chrome.remote_assistance.json',
          ],
          'source_files': [
            '<@(remoting_host_installer_mac_files)',
          ],
          'defs': [
            'VERSION=<(version_full)',
            'VERSION_SHORT=<(version_short)',
            'VERSION_MAJOR=<(version_major)',
            'VERSION_MINOR=<(version_minor)',
            'HOST_NAME=<(host_name)',
            'HOST_BUNDLE_NAME=<(me2me_host_bundle_name)',
            'HOST_SERVICE_NAME=<(host_service_name)',
            'HOST_UNINSTALLER_NAME=<(host_uninstaller_name)',
            'HOST_PKG=<(host_name)',
            'HOST_SERVICE_PKG=<(host_service_name_nospace)',
            'HOST_UNINSTALLER_PKG=<(host_uninstaller_name_nospace)',
            'BUNDLE_ID_HOST=<(bundle_prefix).<(host_name_nospace)',
            'BUNDLE_ID_HOST_SERVICE=<(bundle_prefix).<(host_service_name_nospace)',
            'BUNDLE_ID_HOST_UNINSTALLER=<(bundle_prefix).<(host_uninstaller_name_nospace)',
            'DMG_VOLUME_NAME=<(host_name) <(version_full)',
            'DMG_FILE_NAME=<!(echo <(host_name) | sed "s/ //g")-<(version_full)',
            'NATIVE_MESSAGING_HOST_BUNDLE_NAME=<(native_messaging_host_bundle_name)',
            'REMOTE_ASSISTANCE_HOST_BUNDLE_NAME=<(remote_assistance_host_bundle_name)',
            'PREFPANE_BUNDLE_NAME=<(prefpane_bundle_name)',
          ],
          'inputs': [
            'host/installer/build-installer-archive.py',
            '<@(_source_files)',
          ],
          'outputs': [
            '<(_zip_path)',
          ],
          'action': [
            'python', 'host/installer/build-installer-archive.py',
            '<(_temp_dir)',
            '<(_zip_path)',
            '--source-file-roots', '<@(remoting_host_installer_mac_roots)',
            '--source-files', '<@(_source_files)',
            '--generated-files', '<@(_generated_files)',
            '--generated-files-dst', '<@(_generated_files_dst)',
            '--defs', '<@(_defs)',
          ],
        },
      ],  # actions
    }, # end of target 'remoting_me2me_host_archive'

    {
      'target_name': 'remoting_host_prefpane',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'product_extension': 'prefPane',
      'variables': {
        'bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_ID@")',
        'host_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_NAME@")',
        'prefpane_bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
      },
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_infoplist_strings',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
      ],
      'defines': [
        'HOST_BUNDLE_NAME="<(host_bundle_name)"',
        'PREFPANE_BUNDLE_NAME="<(prefpane_bundle_name)"',
        'JSON_USE_EXCEPTION=0',
      ],
      'include_dirs': [
        '../third_party/jsoncpp/overrides/include/',
        '../third_party/jsoncpp/source/include/',
        '../third_party/jsoncpp/source/src/lib_json/',
      ],
      'sources': [
        'host/mac/me2me_preference_pane.h',
        'host/mac/me2me_preference_pane.mm',
        'host/mac/me2me_preference_pane_confirm_pin.h',
        'host/mac/me2me_preference_pane_confirm_pin.mm',
        'host/mac/me2me_preference_pane_disable.h',
        'host/mac/me2me_preference_pane_disable.mm',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/PreferencePanes.framework',
          '$(SDKROOT)/System/Library/Frameworks/Security.framework',
        ],
      },
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'supported',
        'INFOPLIST_FILE': 'host/mac/me2me_preference_pane-Info.plist',
        'INFOPLIST_PREPROCESS': 'YES',
        'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(bundle_id)"',
      },
      'mac_bundle_resources': [
        'host/mac/me2me_preference_pane.xib',
        'host/mac/me2me_preference_pane_confirm_pin.xib',
        'host/mac/me2me_preference_pane_disable.xib',
        'host/mac/me2me_preference_pane-Info.plist',
        'resources/chromoting128.png',

        # Localized strings for 'Info.plist'
        '<!@pymod_do_main(remoting_localize --locale_output '
        '"<(SHARED_INTERMEDIATE_DIR)/remoting/me2me_preference_pane-InfoPlist.strings/@{json_suffix}.lproj/InfoPlist.strings" '
        '--print_only <(remoting_locales))',
      ],
      'mac_bundle_resources!': [
        'host/mac/me2me_preference_pane-Info.plist',
      ],
      'conditions': [
        ['mac_breakpad==1', {
          'variables': {
            # A real .dSYM is needed for dump_syms to operate on.
            'mac_real_dsym': 1,
          },
        }],  # 'mac_breakpad==1'
      ],  # conditions
    },  # end of target 'remoting_host_prefpane'
  ],  # end of 'targets'

}
