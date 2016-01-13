#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Uses the closure compiler to check the braille ime.'''

import os
import sys


_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.join(_SCRIPT_DIR, '..', 'chromevox', 'tools'))
from jscompilerwrapper import RunCompiler


def CheckBrailleIme():
  print 'Compiling braille IME.'
  js_files = [
      os.path.join(_SCRIPT_DIR, 'main.js'),
      os.path.join(_SCRIPT_DIR, 'braille_ime.js')]
  externs = [os.path.join(_SCRIPT_DIR, 'externs.js')]
  return RunCompiler(js_files, externs)


def main():
  success, output = CheckBrailleIme()
  if len(output) > 0:
    print output
  return int(not success)


if __name__ == '__main__':
  sys.exit(main())
