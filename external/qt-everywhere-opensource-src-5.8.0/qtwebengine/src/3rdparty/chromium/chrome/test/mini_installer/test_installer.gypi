# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    # The source and config files for test_installer.py.
    'test_installer_sources': [
      '<(DEPTH)/chrome/test/mini_installer/chrome_helper.py',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_canary_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_canary_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_canary_no_pv.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_canary_not_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_canary_not_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_system_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_system_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_system_no_pv.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_system_not_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_system_not_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_user_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_user_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_user_no_pv.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_user_not_installed.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/chrome_user_not_inuse.prop',
      '<(DEPTH)/chrome/test/mini_installer/config/config.config',
      '<(DEPTH)/chrome/test/mini_installer/file_verifier.py',
      '<(DEPTH)/chrome/test/mini_installer/launch_chrome.py',
      '<(DEPTH)/chrome/test/mini_installer/process_verifier.py',
      '<(DEPTH)/chrome/test/mini_installer/quit_chrome.py',
      '<(DEPTH)/chrome/test/mini_installer/registry_verifier.py',
      '<(DEPTH)/chrome/test/mini_installer/test_installer.py',
      '<(DEPTH)/chrome/test/mini_installer/uninstall_chrome.py',
      '<(DEPTH)/chrome/test/mini_installer/variable_expander.py',
      '<(DEPTH)/chrome/test/mini_installer/verifier.py',
      '<(DEPTH)/chrome/test/mini_installer/verifier_runner.py',
    ],
  },
}
