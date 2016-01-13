# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="linux" and branding=="Chrome" and enable_remoting_host==1', {
      'variables': {
        'build_deb_script': 'host/installer/linux/build-deb.sh',
        'deb_filename': 'host/installer/<!(["<(build_deb_script)", "-p", "-s", "<(DEPTH)"])',
        'packaging_outputs': [
          '<(deb_filename)',
          '<!(echo <(deb_filename) | sed -e "s/.deb$/.changes/")',
          '<(PRODUCT_DIR)/remoting_me2me_host.debug',
          '<(PRODUCT_DIR)/remoting_start_host.debug',
          '<(PRODUCT_DIR)/native_messaging_host.debug',
          '<(PRODUCT_DIR)/remote_assistance_host.debug',
        ]
      },
      'targets': [
        {
          # Store the installer package(s) into a zip file so there is a
          # consistent filename to reference for build archiving (i.e. in
          # FILES.cfg). This also avoids possible conflicts with "wildcard"
          # package handling in other build/signing scripts.
          'target_name': 'remoting_me2me_host_archive',
          'type': 'none',
          'dependencies': [
            'remoting_me2me_host_deb_installer',
          ],
          'actions': [
            {
              'action_name': 'build_linux_installer_zip',
              'inputs': [
                '<@(packaging_outputs)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
              ],
              'action': [ 'zip', '-j', '-0', '<@(_outputs)', '<@(_inputs)' ],
            },
          ],
        },
        {
          'target_name': 'remoting_me2me_host_deb_installer',
          'type': 'none',
          'dependencies': [
            '<(icu_gyp_path):icudata',
            'remoting_it2me_native_messaging_host',
            'remoting_me2me_host',
            'remoting_me2me_native_messaging_host',
            'remoting_native_messaging_manifests',
            'remoting_resources',
            'remoting_start_host',
          ],
          'actions': [
            {
              'action_name': 'build_debian_package',
              'inputs': [
                '<(build_deb_script)',
                'host/installer/linux/Makefile',
                'host/installer/linux/debian/chrome-remote-desktop.init',
                'host/installer/linux/debian/chrome-remote-desktop.pam',
                'host/installer/linux/debian/compat',
                'host/installer/linux/debian/control',
                'host/installer/linux/debian/copyright',
                'host/installer/linux/debian/postinst',
                'host/installer/linux/debian/preinst',
                'host/installer/linux/debian/rules',
              ],
              'outputs': [
                '<@(packaging_outputs)',
              ],
              'action': [ '<(build_deb_script)', '-s', '<(DEPTH)' ],
            },
          ],
        },
      ],
    }],  # OS=="linux" and branding=="Chrome"

    ['OS=="linux" and enable_remoting_host==1', {
      'targets': [
        # Linux breakpad processing
        {
          'target_name': 'remoting_linux_symbols',
          'type': 'none',
          'conditions': [
            ['linux_dump_symbols==1', {
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'inputs': [
                    '<(DEPTH)/build/linux/dump_app_syms',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/remoting_me2me_host',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/remoting_me2me_host.breakpad.<(target_arch)',
                  ],
                  'action': ['<(DEPTH)/build/linux/dump_app_syms',
                             '<(PRODUCT_DIR)/dump_syms',
                             '<(linux_strip_binary)',
                             '<(PRODUCT_DIR)/remoting_me2me_host',
                             '<@(_outputs)'],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                  'process_outputs_as_sources': 1,
                },
              ],
              'dependencies': [
                'remoting_me2me_host',
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],  # 'linux_dump_symbols==1'
          ],  # end of 'conditions'
        },  # end of target 'linux_symbols'
        {
          'target_name': 'remoting_start_host',
          'type': 'executable',
          'dependencies': [
            'remoting_host_setup_base',
          ],
          'sources': [
            'host/setup/host_starter.cc',
            'host/setup/host_starter.h',
            'host/setup/start_host.cc',
          ],
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },  # end of target 'remoting_start_host'
      ],  # end of 'targets'
    }],  # 'OS=="linux"'

  ],  # end of 'conditions'
}
