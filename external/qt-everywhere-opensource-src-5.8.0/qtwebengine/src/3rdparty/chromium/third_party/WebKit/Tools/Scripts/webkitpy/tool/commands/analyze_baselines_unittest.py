# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.tool.commands.analyze_baselines import AnalyzeBaselines
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase
from webkitpy.tool.mock_tool import MockOptions


class _FakeOptimizer(BaselineOptimizer):

    def read_results_by_directory(self, baseline_name):
        if baseline_name.endswith('txt'):
            return {'LayoutTests/passes/text.html': '123456'}
        return {}


class TestAnalyzeBaselines(BaseTestCase):
    command_constructor = AnalyzeBaselines

    def setUp(self):
        super(TestAnalyzeBaselines, self).setUp()
        self.port = self.tool.port_factory.get('test')
        self.tool.port_factory.get = (lambda port_name=None, options=None: self.port)
        self.lines = []
        self.command._optimizer_class = _FakeOptimizer
        self.command._write = (lambda msg: self.lines.append(msg))

    def test_default(self):
        self.command.execute(MockOptions(suffixes='txt', missing=False, platform=None), ['passes/text.html'], self.tool)
        self.assertEqual(self.lines,
                         ['passes/text-expected.txt:',
                          '  (generic): 123456'])

    def test_missing_baselines(self):
        self.command.execute(MockOptions(suffixes='png,txt', missing=True, platform=None), ['passes/text.html'], self.tool)
        self.assertEqual(self.lines,
                         ['passes/text-expected.png: (no baselines found)',
                          'passes/text-expected.txt:',
                          '  (generic): 123456'])
