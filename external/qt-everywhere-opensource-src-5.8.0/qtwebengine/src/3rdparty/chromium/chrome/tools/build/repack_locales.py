#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to repack paks for a list of locales.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when repacking pak files, thus avoiding a
proliferation of mostly duplicate, cut-n-paste gyp actions.
"""

import optparse
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..',
                             'tools', 'grit'))
from grit.format import data_pack

# The gyp "branding" variable.
BRANDING = None

# Some build paths defined by gyp.
GRIT_DIR = None
SHARE_INT_DIR = None
INT_DIR = None

# The target platform. If it is not defined, sys.platform will be used.
OS = None

# Note that OS is normally set to 'linux' when building for chromeos.
CHROMEOS = False

USE_ASH = False
ENABLE_EXTENSIONS = False

WHITELIST = None

# Extra input files.
EXTRA_INPUT_FILES = []

class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(INTERMEDIATE_DIR)/repack/da.pak',
  # For Fake Bidi, generate it at a fixed path so that tests can safely
  # reference it.
  if locale == 'fake-bidi':
    return '%s/%s.pak' % (INT_DIR, locale)
  if OS == 'mac' or OS == 'ios':
    # For Cocoa to find the locale at runtime, it needs to use '_' instead
    # of '-' (http://crbug.com/20441).  Also, 'en-US' should be represented
    # simply as 'en' (http://crbug.com/19165, http://crbug.com/25578).
    if locale == 'en-US':
      locale = 'en'
    return '%s/repack/%s.lproj/locale.pak' % (INT_DIR, locale.replace('-', '_'))
  else:
    return os.path.join(INT_DIR, 'repack', locale + '.pak')


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  #e.g. '<(grit_out_dir)/generated_resources_da.pak'
  inputs.append(os.path.join(GRIT_DIR, 'generated_resources_%s.pak' % locale))

  #e.g. '<(grit_out_dir)/locale_settings_da.pak'
  inputs.append(os.path.join(GRIT_DIR, 'locale_settings_%s.pak' % locale))

  #e.g. '<(grit_out_dir)/platform_locale_settings_da.pak'
  inputs.append(os.path.join(GRIT_DIR,
                'platform_locale_settings_%s.pak' % locale))

  #e.g. '<(SHARED_INTERMEDIATE_DIR)/components/strings/
  # components_locale_settings_da.pak',
  inputs.append(os.path.join(SHARE_INT_DIR, 'components', 'strings',
                'components_locale_settings_%s.pak' % locale))

  #e.g. '<(SHARED_INTERMEDIATE_DIR)/components/strings/
  # components_strings_da.pak',
  inputs.append(os.path.join(SHARE_INT_DIR, 'components', 'strings',
                'components_strings_%s.pak' % locale))

  #e.g. '<(SHARED_INTERMEDIATE_DIR)/components/strings/
  # components_chromium_strings_da.pak'
  #     or
  #     '<(SHARED_INTERMEDIATE_DIR)/components/strings/
  # components_google_chrome_strings_da.pak',
  inputs.append(os.path.join(SHARE_INT_DIR, 'components', 'strings',
                'components_%s_strings_%s.pak' % (BRANDING, locale)))

  if USE_ASH:
    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ash/strings/ash_strings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'ash', 'strings',
                  'ash_strings_%s.pak' % locale))

  if CHROMEOS:
    inputs.append(os.path.join(SHARE_INT_DIR, 'ui', 'chromeos', 'strings',
                  'ui_chromeos_strings_%s.pak' % locale))
    inputs.append(os.path.join(SHARE_INT_DIR, 'remoting', 'resources',
                  '%s.pak' % locale))

  if OS != 'ios':
    #e.g.
    # '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'content', 'app', 'strings',
                  'content_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth/strings/
    # bluetooth_strings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'device', 'bluetooth', 'strings',
                  'bluetooth_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'ui', 'strings',
                  'ui_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'ui', 'strings',
                  'app_locale_settings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/third_party/libaddressinput/
    # address_input_strings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'third_party', 'libaddressinput',
                               'address_input_strings_%s.pak' % locale))

  else:
    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_locale_settings_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'ios', 'chrome',
                  'ios_locale_settings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_strings_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'ios', 'chrome',
                  'ios_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_chromium_strings_da.pak'
    # or  '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/
    # ios_google_chrome_strings_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'ios', 'chrome',
                  'ios_%s_strings_%s.pak' % (BRANDING, locale)))

  if ENABLE_EXTENSIONS:
    # For example:
    # '<(SHARED_INTERMEDIATE_DIR)/extensions/strings/extensions_strings_da.pak
    # TODO(jamescook): When Android stops building extensions code move this
    # to the OS != 'ios' and OS != 'android' section below.
    inputs.append(os.path.join(SHARE_INT_DIR, 'extensions', 'strings',
                  'extensions_strings_%s.pak' % locale))

  #e.g. '<(grit_out_dir)/google_chrome_strings_da.pak'
  #     or
  #     '<(grit_out_dir)/chromium_strings_da.pak'
  inputs.append(os.path.join(
      GRIT_DIR, '%s_strings_%s.pak' % (BRANDING, locale)))

  # Add any extra input files.
  for extra_file in EXTRA_INPUT_FILES:
    inputs.append('%s_%s.pak' % (extra_file, locale))

  return inputs


def list_outputs(locales):
  """Returns the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(locale))
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in outputs])


def list_inputs(locales):
  """Returns the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs += calc_inputs(locale)
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in inputs])


def repack_locales(locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = []
    inputs += calc_inputs(locale)
    output = calc_output(locale)
    data_pack.DataPack.RePack(output, inputs, whitelist_file=WHITELIST)


def DoMain(argv):
  global BRANDING
  global GRIT_DIR
  global SHARE_INT_DIR
  global INT_DIR
  global OS
  global CHROMEOS
  global USE_ASH
  global WHITELIST
  global ENABLE_AUTOFILL_DIALOG
  global ENABLE_EXTENSIONS
  global EXTRA_INPUT_FILES

  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("-i", action="store_true", dest="inputs", default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("-o", action="store_true", dest="outputs", default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("-g", action="store", dest="grit_dir",
                    help="GRIT build files output directory.")
  parser.add_option("-x", action="store", dest="int_dir",
                    help="Intermediate build files output directory.")
  parser.add_option("-s", action="store", dest="share_int_dir",
                    help="Shared intermediate build files output directory.")
  parser.add_option("-b", action="store", dest="branding",
                    help="Branding type of this build.")
  parser.add_option("-e", action="append", dest="extra_input", default=[],
                    help="Full path to an extra input pak file without the\
                         locale suffix and \".pak\" extension.")
  parser.add_option("-p", action="store", dest="os",
                    help="The target OS. (e.g. mac, linux, win, etc.)")
  parser.add_option("--use-ash", action="store", dest="use_ash",
                    help="Whether to include ash strings")
  parser.add_option("--chromeos", action="store",
                    help="Whether building for Chrome OS")
  parser.add_option("--whitelist", action="store", help="Full path to the "
                    "whitelist used to filter output pak file resource IDs")
  parser.add_option("--enable-autofill-dialog", action="store",
                    dest="enable_autofill_dialog",
                    help="Whether to include strings for autofill dialog")
  parser.add_option("--enable-extensions", action="store",
                    dest="enable_extensions",
                    help="Whether to include strings for extensions")
  options, locales = parser.parse_args(argv)

  if not locales:
    parser.error('Please specificy at least one locale to process.\n')

  print_inputs = options.inputs
  print_outputs = options.outputs
  GRIT_DIR = options.grit_dir
  INT_DIR = options.int_dir
  SHARE_INT_DIR = options.share_int_dir
  BRANDING = options.branding
  EXTRA_INPUT_FILES = options.extra_input
  OS = options.os
  CHROMEOS = options.chromeos == '1'
  USE_ASH = options.use_ash == '1'
  WHITELIST = options.whitelist
  ENABLE_AUTOFILL_DIALOG = options.enable_autofill_dialog == '1'
  ENABLE_EXTENSIONS = options.enable_extensions == '1'

  if not OS:
    if sys.platform == 'darwin':
      OS = 'mac'
    elif sys.platform.startswith('linux'):
      OS = 'linux'
    elif sys.platform in ('cygwin', 'win32'):
      OS = 'win'
    else:
      OS = sys.platform

  if not (GRIT_DIR and INT_DIR and SHARE_INT_DIR):
    parser.error('Please specify all of "-g" and "-x" and "-s".\n')
  if print_inputs and print_outputs:
    parser.error('Please specify only one of "-i" or "-o".\n')
  # Need to know the branding, unless we're just listing the outputs.
  if not print_outputs and not BRANDING:
    parser.error('Please specify "-b" to determine the input files.\n')

  if print_inputs:
    return list_inputs(locales)

  if print_outputs:
    return list_outputs(locales)

  return repack_locales(locales)

if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results
