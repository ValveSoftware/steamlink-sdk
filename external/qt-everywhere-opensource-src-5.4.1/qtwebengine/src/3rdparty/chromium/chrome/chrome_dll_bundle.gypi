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
    'app/theme/find_next_Template.pdf',
    'app/theme/find_prev_Template.pdf',
    'app/theme/menu_overflow_down.pdf',
    'app/theme/menu_overflow_up.pdf',
    'browser/mac/install.sh',
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
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
    '../build/temp_gyp/pdfsqueeze.gyp:pdfsqueeze',
    '../crypto/crypto.gyp:crypto',
    '../pdf/pdf.gyp:pdf',
    # On Mac, Flash gets put into the framework, so we need this
    # dependency here. flash_player.gyp will copy the Flash bundle
    # into PRODUCT_DIR.
    '../third_party/adobe/flash/flash_player.gyp:flapper_binaries',
    '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
    'chrome_resources.gyp:packed_extra_resources',
    'chrome_resources.gyp:packed_resources',
  ],
  'rules': [
    {
      'rule_name': 'pdfsqueeze',
      'extension': 'pdf',
      'inputs': [
        '<(PRODUCT_DIR)/pdfsqueeze',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/pdfsqueeze/<(RULE_INPUT_ROOT).pdf',
      ],
      'action': ['<(PRODUCT_DIR)/pdfsqueeze',
                 '<(RULE_INPUT_PATH)', '<@(_outputs)'],
      'message': 'Running pdfsqueeze on <(RULE_INPUT_PATH)',
    },
  ],
  'variables': {
    'conditions': [
      ['branding=="Chrome"', {
        'theme_dir_name': 'google_chrome',
      }, {  # else: 'branding!="Chrome"
        'theme_dir_name': 'chromium',
      }],
    ],
    'libpeer_target_type%': 'static_library',
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
                 '--breakpad=<(mac_breakpad_compiled_in)',
                 '--breakpad_uploads=<(mac_breakpad_uploads)',
                 '--keystone=0',
                 '--scm=1',
                 '--branding=<(branding)'],
    },
    {
      'postbuild_name': 'Symlink Libraries',
      'action': [
        'ln',
        '-fns',
        'Versions/Current/Libraries',
        '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Libraries'
      ],
    },
  ],
  'copies': [
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
      'files': [
        '<(PRODUCT_DIR)/exif.so',
        '<(PRODUCT_DIR)/ffmpegsumo.so',
      ],
    },
    {
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins',
      'files': [
        '<(PRODUCT_DIR)/PDF.plugin',
      ],
      'conditions': [
        ['disable_nacl!=1', {
          'files': [
            '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.plugin',
          ],
          'conditions': [
            ['target_arch=="x64"', {
              'files': [
                '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
              ],
            }, {
              'files': [
                '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
              ],
            }],
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
          ],
        }],
      ],
    },
    {
      # This file is used by the component installer.
      # It is not a complete plug-in on its own.
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins/',
      'files': [],
      'conditions': [
        ['branding == "Chrome"', {
          'files': [
            '<(PRODUCT_DIR)/widevinecdmadapter.plugin',
          ],
        }],
      ],
    },
    {
      # Copy of resources used by tests.
      'destination': '<(PRODUCT_DIR)',
      'files': [
          '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak'
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
      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/resources',
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
    ['mac_breakpad_compiled_in==1', {
      'dependencies': [
        '../breakpad/breakpad.gyp:breakpad',
        '../components/components.gyp:policy',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
          'files': [
            '<(PRODUCT_DIR)/crash_inspector',
            '<(PRODUCT_DIR)/crash_report_sender.app'
          ],
        },
      ],
    }],  # mac_breakpad_compiled_in
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
      ],
    }],
    ['enable_webrtc==1 and libpeer_target_type!="static_library"', {
      'copies': [{
       'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
       'files': [
          '<(PRODUCT_DIR)/libpeerconnection.so',
        ],
      }],
    }],
    ['icu_use_data_file_flag==1', {
      'mac_bundle_resources': [
        '<(PRODUCT_DIR)/icudtl.dat',
      ],
    }],
  ],  # conditions
}
