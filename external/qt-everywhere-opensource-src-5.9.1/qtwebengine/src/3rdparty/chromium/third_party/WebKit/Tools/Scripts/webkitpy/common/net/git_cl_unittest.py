# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.host_mock import MockHost


class GitCLTest(unittest.TestCase):

    def test_run(self):
        host = MockHost()
        host.executive = MockExecutive2(output='mock-output')
        git_cl = GitCL(host)
        output = git_cl.run(['command'])
        self.assertEqual(output, 'mock-output')
        self.assertEqual(host.executive.calls, [['git', 'cl', 'command']])

    def test_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive2(output='mock-output')
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['upload'])
        self.assertEqual(
            host.executive.calls,
            [['git', 'cl', 'upload', '--auth-refresh-token-json', 'token.json']])

    def test_some_commands_not_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive2(output='mock-output')
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['issue'])
        self.assertEqual(host.executive.calls, [['git', 'cl', 'issue']])

    def test_get_issue_number(self):
        host = MockHost()
        host.executive = MockExecutive2(output='Issue number: 12345 (http://crrev.com/12345)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), '12345')

    def test_get_issue_number_none(self):
        host = MockHost()
        host.executive = MockExecutive2(output='Issue number: None (None)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), 'None')

    def test_all_jobs_finished_empty(self):
        self.assertTrue(GitCL.all_jobs_finished([]))

    def test_all_jobs_finished_with_started_jobs(self):
        self.assertFalse(GitCL.all_jobs_finished([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
            },
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
            },
        ]))

    def test_all_jobs_finished_only_completed_jobs(self):
        self.assertTrue(GitCL.all_jobs_finished([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
            },
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
            },
        ]))

    def test_has_failing_try_results_empty(self):
        self.assertFalse(GitCL.has_failing_try_results([]))

    def test_has_failing_try_results_only_success_and_started(self):
        self.assertFalse(GitCL.has_failing_try_results([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
            },
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
            },
        ]))

    def test_has_failing_try_results_with_failing_results(self):
        self.assertTrue(GitCL.has_failing_try_results([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
            },
        ]))
