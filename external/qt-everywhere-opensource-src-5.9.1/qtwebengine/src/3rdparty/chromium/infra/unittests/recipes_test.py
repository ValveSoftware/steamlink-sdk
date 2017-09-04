#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess


THIS_DIR = os.path.abspath(os.path.dirname(__file__))
INFRA_DIR = os.path.dirname(THIS_DIR)


def recipes_py(*args):
  subprocess.check_call([
      os.path.join(INFRA_DIR, 'recipes.py'),
      '--deps-path=-',
      '--use-bootstrap',
  ] + list(args))


recipes_py('simulation_test')
recipes_py('lint')
