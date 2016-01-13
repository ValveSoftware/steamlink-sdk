#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Formats as a .json file that can be used to localize Google Chrome
extensions."""

from json import JSONEncoder
import re
import types

from grit import util
from grit.node import message

def Format(root, lang='en', output_dir='.'):
  """Format the messages as JSON."""
  yield '{\n'

  encoder = JSONEncoder();
  format = ('  "%s": {\n'
            '    "message": %s\n'
            '  }')
  first = True
  for child in root.ActiveDescendants():
    if isinstance(child, message.MessageNode):
      id = child.attrs['name']
      if id.startswith('IDR_') or id.startswith('IDS_'):
        id = id[4:]

      loc_message = encoder.encode(child.ws_at_start + child.Translate(lang) +
                                   child.ws_at_end)

      if not first:
        yield ',\n'
      first = False
      yield format % (id, loc_message)

  yield '\n}\n'
