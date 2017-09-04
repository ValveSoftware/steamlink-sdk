# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.layout_tests.controllers import repaint_overlay


LAYER_TREE = """{
  "layers": [
    {
      "name": "layer1",
      "bounds": [800.00, 600.00],
    }, {
      "name": "layer2",
      "position": [8.00, 80.00],
      "bounds": [800.00, 600.00],
      "contentsOpaque": true,
      "drawsContent": true,
      "paintInvalidations": [
        {
          "object": "object1",
          "rect": [8, 108, 100, 100],
          "reason": "full"
        }, {
          "object": "object2",
          "rect": [0, 216, 800, 100],
          "reason": "full"
        }, {
          "object": "object1",
          "reason": "location change"
        }
      ]
    }
  ]
}
"""


class TestRepaintOverlay(unittest.TestCase):

    def test_result_contains_repaint_rects(self):
        self.assertTrue(repaint_overlay.result_contains_repaint_rects(LAYER_TREE))
        self.assertFalse(repaint_overlay.result_contains_repaint_rects('ABCD'))

    def test_extract_layer_tree(self):
        self.assertEquals(LAYER_TREE, repaint_overlay.extract_layer_tree(LAYER_TREE))
