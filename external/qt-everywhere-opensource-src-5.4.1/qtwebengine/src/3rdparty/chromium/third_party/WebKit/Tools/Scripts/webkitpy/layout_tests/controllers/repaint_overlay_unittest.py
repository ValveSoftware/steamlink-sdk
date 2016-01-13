# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import webkitpy.thirdparty.unittest2 as unittest

from webkitpy.layout_tests.controllers import repaint_overlay


EXPECTED_TEXT = """
(GraphicsLayer
  (bounds 800.00 600.00)
  (children 1
    (GraphicsLayer
      (bounds 800.00 600.00)
      (contentsOpaque 1)
      (drawsContent 1)
      (repaint rects
        (rect 8.00 108.00 100.00 100.00)
        (rect 0.00 216.00 800.00 100.00)
      )
    )
  )
)
"""

ACTUAL_TEXT = """
(GraphicsLayer
  (bounds 800.00 600.00)
  (children 1
    (GraphicsLayer
      (bounds 800.00 600.00)
      (contentsOpaque 1)
      (drawsContent 1)
      (repaint rects
        (rect 0.00 216.00 800.00 100.00)
      )
    )
  )
)
"""


class TestRepaintOverlay(unittest.TestCase):
    def test_result_contains_repaint_rects(self):
        self.assertTrue(repaint_overlay.result_contains_repaint_rects(EXPECTED_TEXT))
        self.assertTrue(repaint_overlay.result_contains_repaint_rects(ACTUAL_TEXT))
        self.assertFalse(repaint_overlay.result_contains_repaint_rects('ABCD'))

    def test_generate_repaint_overlay_html(self):
        html = repaint_overlay.generate_repaint_overlay_html('test', ACTUAL_TEXT, EXPECTED_TEXT)
        self.assertNotEqual(-1, html.find('expected_rects = [[8.00,108.00,100.00,100.00],[0.00,216.00,800.00,100.00]];'))
        self.assertNotEqual(-1, html.find('actual_rects = [[0.00,216.00,800.00,100.00]];'))
