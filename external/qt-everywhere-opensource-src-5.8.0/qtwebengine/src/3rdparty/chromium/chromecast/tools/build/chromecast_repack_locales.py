#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
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

# Some build paths defined by gyp.
GRIT_DIR = None
INT_DIR = None
CHROMECAST_BRANDING = None

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
  return os.path.join(INT_DIR, locale + '.pak')


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []
  if CHROMECAST_BRANDING != 'public':
    inputs.append(os.path.join(GRIT_DIR, 'app_strings_%s.pak' % locale))
  inputs.append(os.path.join(GRIT_DIR, 'chromecast_settings_%s.pak' % locale))
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
  global CHROMECAST_BRANDING
  global GRIT_DIR
  global INT_DIR

  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("-i", action="store_true", dest="inputs", default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("-o", action="store_true", dest="outputs", default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("-g", action="store", dest="grit_dir",
                    help="GRIT build files output directory.")
  parser.add_option("-x", action="store", dest="int_dir",
                    help="Intermediate build files output directory.")
  parser.add_option("-b", action="store", dest="chromecast_branding",
                    help="Chromecast branding " +
                         "('public', 'internal' or 'google').")
  options, locales = parser.parse_args(argv)

  if not locales:
    parser.error('Please specificy at least one locale to process.\n')

  print_inputs = options.inputs
  print_outputs = options.outputs
  GRIT_DIR = options.grit_dir
  INT_DIR = options.int_dir
  CHROMECAST_BRANDING = options.chromecast_branding

  if (CHROMECAST_BRANDING != 'public' and
      CHROMECAST_BRANDING != 'internal' and
      CHROMECAST_BRANDING != 'google'):
    parser.error('Chromecast branding (-b) must be ' +
                 '"public", "internal" or "google".\n')
  if not (GRIT_DIR and INT_DIR):
    parser.error('Please specify all of "-g" and "-x".\n')
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
