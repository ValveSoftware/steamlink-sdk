#!/usr/bin/env python
#############################################################################
##
## Copyright (c) 2012 The Chromium Authors. All rights reserved.
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtWebEngine module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

# This is esentially a trimmed down version of chrome's repack_locales script
"""Helper script to repack paks for a list of locales.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when repacking pak files, thus avoiding a
proliferation of mostly duplicate, cut-n-paste gyp actions.
"""

import optparse
import os
import sys
import qtwebengine_utils as utils

chrome_src = utils.getChromiumSrcDir()

sys.path.append(os.path.join(chrome_src, 'tools', 'grit'))
from grit.format import data_pack

# The gyp "branding" variable.
BRANDING = 'chromium'

# Some build paths defined by gyp.
SHARE_INT_DIR = None
INT_DIR = None

# The target platform. If it is not defined, sys.platform will be used.
OS = None

# Extra input files.
EXTRA_INPUT_FILES = []

class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(INTERMEDIATE_DIR)/repack/qtwebengine_locales/da.pak',
  return os.path.join(INT_DIR, 'repack/qtwebengine_locales', locale + '.pak')


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

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

  if OS != 'ios':
    #e.g. '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'content', 'app', 'strings',
                  'content_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'ui', 'strings',
                  'ui_strings_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_da.pak',
    inputs.append(os.path.join(SHARE_INT_DIR, 'ui', 'strings',
                  'app_locale_settings_%s.pak' % locale))

    # Used for localized error messages and error pages
    #e.g. '<(SHARED_INTERMEDIATE_DIR)/chrome/generated_resources_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'chrome',
                  'generated_resources_%s.pak' % locale))

    #e.g. '<(SHARED_INTERMEDIATE_DIR)/chrome/generated_resources_da.pak'
    inputs.append(os.path.join(SHARE_INT_DIR, 'chrome',
                  'chromium_strings_%s.pak' % locale))

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
    data_pack.DataPack.RePack(output, inputs)


def DoMain(argv):
  global SHARE_INT_DIR
  global INT_DIR
  global OS
  global EXTRA_INPUT_FILES

  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("-i", action="store_true", dest="inputs", default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("-o", action="store_true", dest="outputs", default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("-x", action="store", dest="int_dir",
                    help="Intermediate build files output directory.")
  parser.add_option("-g", action="store", dest="grit_dir",
                    help="Placeholder for unneeded/unused grit_dir option.")
  parser.add_option("-b", action="store", dest="branding",
                    help="Placeholder for unneeded/unused branding option.")
  parser.add_option("-s", action="store", dest="share_int_dir",
                    help="Shared intermediate build files output directory.")
  parser.add_option("-e", action="append", dest="extra_input", default=[],
                    help="Full path to an extra input pak file without the\
                         locale suffix and \".pak\" extension.")
  parser.add_option("-p", action="store", dest="os",
                    help="The target OS. (e.g. mac, linux, win, etc.)")
  options, locales = parser.parse_args(argv)

  if not locales:
    parser.error('Please specificy at least one locale to process.\n')

  print_inputs = options.inputs
  print_outputs = options.outputs
  INT_DIR = options.int_dir
  SHARE_INT_DIR = options.share_int_dir
  EXTRA_INPUT_FILES = options.extra_input
  OS = options.os

  if not OS:
    if sys.platform == 'darwin':
      OS = 'mac'
    elif sys.platform.startswith('linux'):
      OS = 'linux'
    elif sys.platform in ('cygwin', 'win32'):
      OS = 'win'
    else:
      OS = sys.platform

  if not (INT_DIR and SHARE_INT_DIR):
    parser.error('Please specify all of "-x" and "-s".\n')
  if print_inputs and print_outputs:
    parser.error('Please specify only one of "-i" or "-o".\n')

  if print_inputs:
    return list_inputs(locales)

  if print_outputs:
    return list_outputs(locales)

  return repack_locales(locales)

if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results
