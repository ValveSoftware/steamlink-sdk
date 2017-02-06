# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.commands.rebaseline_from_try_jobs import RebaselineFromTryJobs
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase
from webkitpy.tool.mock_tool import MockOptions


class RebaselineFromTryJobsTest(BaseTestCase):
    command_constructor = RebaselineFromTryJobs

    def setUp(self):
        super(RebaselineFromTryJobsTest, self).setUp()
        self.command.web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1, 2],
            }),
            'https://codereview.chromium.org/api/11112222/2': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'win_chromium_rel_ng',
                        'master': 'tryserver.chromium.win',
                        'buildnumber': 5000,
                    },
                ],
            }),
        })

    def test_execute_with_issue_number_given(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(MockOptions(issue=11112222), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(
            logs,
            ('Getting results for Rietveld issue 11112222.\n'
             '  Builder: win_chromium_rel_ng\n'
             '  Master: tryserver.chromium.win\n'
             '  Build: 5000\n'
             'fast/dom/prototype-inheritance.html (actual: TEXT, expected: PASS)\n'
             'fast/dom/prototype-taco.html (actual: PASS TEXT, expected: PASS)\n'
             'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html (actual: IMAGE, expected: PASS)\n'))

    def test_execute_with_no_issue_number(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(MockOptions(issue=None), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(logs, 'No issue number provided.\n')
