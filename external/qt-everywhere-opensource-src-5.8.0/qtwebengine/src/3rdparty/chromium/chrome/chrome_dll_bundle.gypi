# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains resources for the main Mac chromium bundle.
{
  # The main browser executable's name is <(mac_product_name).
  # Certain things will get confused if two modules in the
  # executable share the same name, so append " Framework" to the
  # product_name used for the framework.  This will result in
  # a name like "Chromium Framework.framework".
  'product_name': '<(mac_product_name) Framework',
  'mac_bundle': 1,
  'xcode_settings': {
    'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',

    # The dylib versions are of the form a[.b[.c]], where a is a
    # 16-bit unsigned integer, and b and c are 8-bit unsigned
    # integers.  Any missing component is taken to be 0.  The
    # best mapping from product version numbers into this scheme
    # is to just use a=BUILD, b=(PATCH/256), c=(PATCH%256). There
    # is no ambiguity in this scheme because the build and patch
    # numbers are guaranteed unique even across distinct major
    # and minor version numbers.  These settings correspond to
    # -compatibility_version and -current_version.
    'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
    'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',

    # The framework is placed within the .app's versioned
    # directory.  DYLIB_INSTALL_NAME_BASE and
    # LD_DYLIB_INSTALL_NAME affect -install_name.
    'DYLIB_INSTALL_NAME_BASE':
        '@executable_path/../Versions/<(version_full)',
    # See /build/mac/copy_framework_unversioned.sh for
    # information on LD_DYLIB_INSTALL_NAME.
    'LD_DYLIB_INSTALL_NAME':
        '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',

    'INFOPLIST_FILE': 'app/framework-Info.plist',
  },
  'includes': [
    'chrome_nibs.gypi',
    '../media/cdm_paths.gypi',
  ],
  # TODO(mark): Come up with a fancier way to do this.  It should
  # only be necessary to list framework-Info.plist once, not the
  # three times it is listed here.
  'mac_bundle_resources': [
    # This image is used to badge the lock icon in the
    # authentication dialogs, such as those used for installation
    # from disk image and Keystone promotion (if so enabled).  It
    # needs to exist as a file on disk and not just something in a
    # resource bundle because that's the interface that
    # Authorization Services uses.  Also, Authorization Services
    # can't deal with .icns files.
    'app/theme/default_100_percent/<(theme_dir_name)/product_logo_32.png',

    'app/framework-Info.plist',
    '<@(mac_all_xibs)',
    'browser/mac/install.sh',
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_material_100_percent.pak',
    '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
    '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))',
    # Note: pseudo_locales are generated via the packed_resources
    # dependency but not copied to the final target.  See
    # common.gypi for more info.
  ],
  'mac_bundle_resources!': [
    'app/framework-Info.plist',
  ],
  'dependencies': [
    'app_mode_app',
    # Bring in pdfsqueeze and run it on all pdfs
    '../crypto/crypto.gyp:crypto',
    # On Mac, Flash gets put into the framework, so we need this
    # dependency here. flash_player.gyp will copy the Flash bundle
    # into PRODUCT_DIR.
    '../third_party/adobe/flash/flash_player.gyp:flapper_binaries',
    '../third_party/crashpad/crashpad/handler/handler.gyp:crashpad_handler',
    '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
    'chrome_resources.gyp:packed_extra_resources',
    'chrome_resources.gyp:packed_resources',
  ],
  'variables': {
    'theme_dir_name': '<(branding_path_component)',
  },
  'postbuilds': [
    {
      # Modify the Info.plist as needed.  The script explains why
      # this is needed.  This is also done in the chrome target.
      # The framework needs the Breakpad keys if this feature is
      # enabled.  It does not need the Keystone keys; these always
      # come from the outer application bundle.  The framework
      # doesn't currently use the SCM keys for anything,
      # but this seems like a really good place to store them.
      'postbuild_name': 'Tweak Info.plist',
      'action': ['<(tweak_info_plist_path)',
                 '--plist=${TARGET_BUILD_DIR}/${INFOPLIST_PATH}',
                 '--breakpad=<(mac_breakpad_compiled_in)',
                 '--breakpad_uploads=<(mac_breakpad_uploads)',
                 '--keystone=0',
                 '--scm=1',
                 '--branding=<(branding)'],
    },
  ],
  'copies': [
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Helpers',
      'files': [
        '<(PRODUCT_DIR)/crashpad_handler',
      ],
    },
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins',
      'files': [],
      'conditions': [
        ['disable_nacl!=1', {
          'files': [
            '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
          ],
        }],
      ],
    },
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins/PepperFlash',
      'files': [],
      'conditions': [
        ['branding == "Chrome"', {
          'files': [
            '<(PRODUCT_DIR)/PepperFlash/PepperFlashPlayer.plugin',
            '<(PRODUCT_DIR)/PepperFlash/manifest.json',
          ],
        }],
      ],
    },
    {
      # The adapter is not a complete library on its own. It needs the Widevine
      # CDM to work.
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries/<(widevine_cdm_path)',
      'files': [],
      'conditions': [
        ['branding == "Chrome"', {
          'files': [
            '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdm.dylib',
            '<(PRODUCT_DIR)/<(widevine_cdm_path)/widevinecdmadapter.plugin',
          ],
        }],
      ],
    },
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries/WidevineCdm',
      'files': [],
      'conditions': [
        ['branding == "Chrome"', {
          'files': [
            '<(PRODUCT_DIR)/WidevineCdm/manifest.json',
          ],
        }],
      ],
    },
    {
      # Copy of resources used by tests.
      'destination': '<(PRODUCT_DIR)/pseudo_locales',
      'files': [
          '<(SHARED_INTERMEDIATE_DIR)/<(pseudo_locales).pak'
      ],
    },
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
      'files': [
        # Loader bundle for platform apps.
        '<(PRODUCT_DIR)/app_mode_loader.app',
      ],
    },
  ],
  'conditions': [
    ['branding=="Chrome"', {
      'copies': [
        {
          # This location is for the Mac build. Note that the
          # copying of these files for Windows and Linux is handled
          # in chrome.gyp, as Mac needs to be dropped inside the
          # framework.
          'destination':
              '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Default Apps',
          'files': ['<@(default_apps_list)'],
        },
      ],
    }],
    ['mac_breakpad==1', {
      'variables': {
        # A real .dSYM is needed for dump_syms to operate on.
        'mac_real_dsym': 1,
      },
    }],
    ['mac_keystone==1', {
      'mac_bundle_resources': [
        'browser/mac/keystone_promote_preflight.sh',
        'browser/mac/keystone_promote_postflight.sh',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Copy KeystoneRegistration.framework',
          'action': [
            '../build/mac/copy_framework_unversioned.sh',
            '-I',
            '../third_party/googlemac/Releases/Keystone/KeystoneRegistration.framework',
            '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
          ],
        },
        {
          'postbuild_name:': 'Lipo KeystoneRegistration.framework',
          'variables': {
            'KEYSTONE_FILE':
            '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks/KeystoneRegistration.framework/KeystoneRegistration',
          },
          'action': [
            'tools/mac_helpers/lipo_thin_x86_64.sh',
            '<(KEYSTONE_FILE)',
          ],
        },
        {
          'postbuild_name': 'Symlink Frameworks',
          'action': [
            'ln',
            '-fns',
            'Versions/Current/Frameworks',
            '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Frameworks'
          ],
        },
      ],
    }],  # mac_keystone
    ['debug_devtools==1', {
      'postbuilds': [{
        'postbuild_name': 'Copy inspector files',
        'action': [
          'ln',
          '-fs',
          '${BUILT_PRODUCTS_DIR}/resources/inspector',
          '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Resources',
        ],
      }],
    }],
    ['enable_hidpi==1', {
      'mac_bundle_resources': [
        '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
        '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_material_200_percent.pak',
      ],
    }],
    ['icu_use_data_file_flag==1', {
      'mac_bundle_resources': [
        '<(PRODUCT_DIR)/icudtl.dat',
      ],
    }],
    ['v8_use_external_startup_data==1', {
      'mac_bundle_resources': [
        '<(PRODUCT_DIR)/natives_blob.bin',
        '<(PRODUCT_DIR)/snapshot_blob.bin',
      ],
    }],
  ],  # conditions
}
