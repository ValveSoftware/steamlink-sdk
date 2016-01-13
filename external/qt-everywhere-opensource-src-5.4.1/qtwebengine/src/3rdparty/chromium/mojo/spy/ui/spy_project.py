# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import tvcm_stub

from trace_viewer import trace_viewer_project


class SpyProject(trace_viewer_project.TraceViewerProject):
  spy_path = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                            '..'))

  def __init__(self):
    super(SpyProject, self).__init__(
      [self.spy_path])
