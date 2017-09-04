# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse

from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.rietveld import Rietveld
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.logtesting import LoggingTestCase
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline_cl import RebaselineCL
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase


class RebaselineCLTest(BaseTestCase, LoggingTestCase):
    command_constructor = RebaselineCL

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)
        web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1, 2],
            }),
            'https://codereview.chromium.org/api/11112222/2': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'MOCK Try Win',
                        'buildnumber': 5000,
                        'result': 0,
                    },
                    {
                        'builder': 'MOCK Try Mac',
                        'buildnumber': 4000,
                        'result': 0,
                    },
                ],
                'files': {
                    'third_party/WebKit/LayoutTests/fast/dom/prototype-inheritance.html': {'status': 'M'},
                    'third_party/WebKit/LayoutTests/fast/dom/prototype-taco.html': {'status': 'M'},
                },
            }),
        })
        self.tool.builders = BuilderList({
            "MOCK Try Win": {
                "port_name": "test-win-win7",
                "specifiers": ["Win7", "Release"],
                "is_try_builder": True,
            },
            "MOCK Try Linux": {
                "port_name": "test-mac-mac10.10",
                "specifiers": ["Mac10.10", "Release"],
                "is_try_builder": True,
            },
        })
        self.command.rietveld = Rietveld(web)

        self.tool.buildbot.set_retry_sumary_json(Build('MOCK Try Win', 5000), json.dumps({
            'failures': [
                'fast/dom/prototype-newtest.html',
                'fast/dom/prototype-taco.html',
                'fast/dom/prototype-inheritance.html',
                'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html',
            ],
            'ignored': [],
        }))

        # Write to the mock filesystem so that these tests are considered to exist.
        port = self.mac_port
        tests = [
            'fast/dom/prototype-taco.html',
            'fast/dom/prototype-inheritance.html',
            'fast/dom/prototype-newtest.html',
            'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html',
        ]
        for test in tests:
            self._write(port.host.filesystem.join(port.layout_tests_dir(), test), 'contents')

    def tearDown(self):
        BaseTestCase.tearDown(self)
        LoggingTestCase.tearDown(self)

    @staticmethod
    def command_options(**kwargs):
        options = {
            'only_changed_tests': False,
            'dry_run': False,
            'issue': None,
            'optimize': True,
            'results_directory': None,
            'verbose': False,
            'trigger_jobs': False,
        }
        options.update(kwargs)
        return optparse.Values(dict(**options))

    def test_execute_with_issue_number_given(self):
        self.command.execute(self.command_options(issue=11112222), [], self.tool)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_no_issue_number(self):
        self.command.execute(self.command_options(), [], self.tool)
        self.assertLog(['ERROR: No issue number given and no issue for current branch. This tool requires a CL\n'
                        'to operate on; please run `git cl upload` on this branch first, or use the --issue\n'
                        'option to download baselines for another existing CL.\n'])

    def test_execute_with_issue_number_from_branch(self):
        git_cl = GitCL(self.tool)
        git_cl.get_issue_number = lambda: '11112222'
        self.command.git_cl = lambda: git_cl
        self.command.execute(self.command_options(), [], self.tool)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_only_changed_tests_option(self):
        self.command.execute(self.command_options(issue=11112222, only_changed_tests=True), [], self.tool)
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # is in the list of failed tests, but not in the list of files modified
        # in the given CL; it should be included because all_tests is set to True.
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
        ])

    def test_execute_with_flaky_test_that_fails_on_retry(self):
        # In this example, the --only-changed-tests flag is not given, but
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # failed both with and without the patch in the try job, so it is not
        # rebaselined.
        self.tool.buildbot.set_retry_sumary_json(Build('MOCK Try Win', 5000), json.dumps({
            'failures': [
                'fast/dom/prototype-taco.html',
                'fast/dom/prototype-inheritance.html',
            ],
            'ignored': ['svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html'],
        }))
        self.command.execute(self.command_options(issue=11112222), [], self.tool)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
        ])

    def test_execute_with_no_retry_summary_downloaded(self):
        # In this example, the --only-changed-tests flag is not given, but
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # failed both with and without the patch in the try job, so it is not
        # rebaselined.
        self.tool.buildbot.set_retry_sumary_json(
            Build('MOCK Try Win', 5000), None)
        self.command.execute(self.command_options(issue=11112222), [], self.tool)
        self.assertLog([
            'WARNING: No retry summary available for build Build(builder_name=u\'MOCK Try Win\', build_number=5000).\n',
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_trigger_jobs_option(self):
        self.command.execute(self.command_options(issue=11112222, trigger_jobs=True), [], self.tool)
        self.assertLog([
            'INFO: Triggering try jobs for:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Please re-run webkit-patch rebaseline-cl once all pending try jobs have finished.\n',
        ])
        self.assertEqual(
            self.tool.executive.calls,
            [['git', 'cl', 'try', '-b', 'MOCK Try Linux']])

    def test_rebaseline_calls(self):
        """Tests the list of commands that are invoked when rebaseline is called."""
        # First write test contents to the mock filesystem so that
        # fast/dom/prototype-taco.html is considered a real test to rebaseline.
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            port.host.filesystem.join(port.layout_tests_dir(), 'fast/dom/prototype-taco.html'),
            'test contents')

        self.command.rebaseline(
            self.command_options(issue=11112222),
            {"fast/dom/prototype-taco.html": {Build("MOCK Try Win", 5000): ["txt", "png"]}})

        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html', '--build-number', '5000']],
                [['python', 'echo', 'optimize-baselines', '--suffixes', 'txt', 'fast/dom/prototype-taco.html']]
            ])

    def test_trigger_jobs_for_missing_builds_empty_list(self):
        # Passing in no builds implies that no try jobs were started.
        self.assertTrue(self.command.trigger_jobs_for_missing_builds([]))
        self.assertEqual(
            self.tool.executive.calls,
            [['git', 'cl', 'try', '-b', 'MOCK Try Linux', '-b', 'MOCK Try Win']])
        self.assertLog([
            'INFO: Triggering try jobs for:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Win\n',
        ])

    def test_trigger_jobs_for_missing_builds_started_and_successful(self):
        # A build number of None implies that a job has been started but not finished yet.
        self.assertTrue(self.command.trigger_jobs_for_missing_builds([
            Build('MOCK Try Linux', None),
            Build('MOCK Try Win', 123),
        ]))
        self.assertEqual(self.tool.executive.calls, [])
        self.assertLog([
            'INFO: There are existing pending builds for:\n',
            'INFO:   MOCK Try Linux\n',
        ])

    def test_trigger_jobs_for_missing_builds_one_started(self):
        self.assertTrue(self.command.trigger_jobs_for_missing_builds([
            Build('MOCK Try Linux', None),
        ]))
        self.assertEqual(self.tool.executive.calls, [['git', 'cl', 'try', '-b', 'MOCK Try Win']])
        self.assertLog([
            'INFO: There are existing pending builds for:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Triggering try jobs for:\n',
            'INFO:   MOCK Try Win\n',
        ])
