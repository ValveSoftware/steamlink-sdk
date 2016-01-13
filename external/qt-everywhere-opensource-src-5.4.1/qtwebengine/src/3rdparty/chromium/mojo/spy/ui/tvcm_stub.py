# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_CHROME_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                            '..', '..', '..'))

# Bring in tvcm module for basic JS components capabilities.
sys.path.append(os.path.join(_CHROME_PATH,
    'third_party', 'trace-viewer', 'third_party', 'tvcm'))

# Bring in trace_viewer module for the UI features that are part of the trace
# viewer.
sys.path.append(os.path.join(_CHROME_PATH,
    'third_party', 'trace-viewer'))
