# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import copy

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.buildbot_mock import MockBuildBot
from webkitpy.common.net.layouttestresults import LayoutTestResult, LayoutTestResults
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.logtesting import LoggingTestCase
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.w3c.update_w3c_test_expectations import W3CExpectationsLineAdder, MARKER_COMMENT


class UpdateW3CTestExpectationsTest(LoggingTestCase):

    def setUp(self):
        super(UpdateW3CTestExpectationsTest, self).setUp()
        self.host = MockHost()
        self.mock_dict_one = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_two = {
            'imported/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}
            }
        }
        self.mock_dict_three = {
            'imported/fake/test/path.html': {
                'four': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}}
        }
        self.mock_dict_four = {
            'imported/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'}
            }
        }
        self.host.builders = BuilderList({
            'mac': {'port_name': 'test-mac'},
        })

    def tearDown(self):
        super(UpdateW3CTestExpectationsTest, self).tearDown()
        self.host = None

    def test_get_failing_results_dict_only_passing_results(self):
        self.host.buildbot.set_results(Build('mac', 123), LayoutTestResults({
            'tests': {
                'fake': {
                    'test.html': {
                        'passing-test.html': {
                            'expected': 'PASS',
                            'actual': 'PASS',
                        },
                    },
                },
            },
        }))
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(line_adder.get_failing_results_dict(Build('mac', 123)), {})

    def test_get_failing_results_dict_no_results(self):
        self.host.buildbot = MockBuildBot()
        self.host.buildbot.set_results(Build('mac', 123), None)
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(line_adder.get_failing_results_dict(Build('mac', 123)), {})

    def test_get_failing_results_dict_some_failing_results(self):
        self.host.buildbot.set_results(Build('mac', 123), LayoutTestResults({
            'tests': {
                'fake': {
                    'test.html': {
                        'failing-test.html': {
                            'expected': 'PASS',
                            'actual': 'IMAGE',
                            'is_unexpected': True,
                        },
                    },
                },
            },
        }))
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(line_adder.get_failing_results_dict(Build('mac', 123)), {
            'fake/test.html/failing-test.html': {
                'Mac': {
                    'actual': 'IMAGE',
                    'expected': 'PASS',
                    'bug': 'crbug.com/626703',
                },
            },
        })

    def test_merge_same_valued_keys(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(
            line_adder.merge_same_valued_keys(self.mock_dict_one['fake/test/path.html']),
            {('two', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'}})
        self.assertEqual(
            line_adder.merge_same_valued_keys(self.mock_dict_two['imported/fake/test/path.html']),
            {
                ('three', 'one'): {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'}
            })

    def test_get_expectations(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(
            line_adder.get_expectations({'expected': 'FAIL', 'actual': 'PASS'}),
            set(['Pass']))
        self.assertEqual(
            line_adder.get_expectations({'expected': 'FAIL', 'actual': 'TIMEOUT'}),
            set(['Timeout']))
        self.assertEqual(
            line_adder.get_expectations({'expected': 'TIMEOUT', 'actual': 'PASS'}),
            set(['Pass']))
        self.assertEqual(
            line_adder.get_expectations({'expected': 'PASS', 'actual': 'TIMEOUT CRASH FAIL'}),
            set(['Crash', 'Failure', 'Timeout']))
        self.assertEqual(
            line_adder.get_expectations({'expected': 'SLOW CRASH FAIL TIMEOUT', 'actual': 'PASS'}),
            set(['Pass']))

    def test_create_line_list_old_tests(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(line_adder.create_line_list(self.mock_dict_one), [])

    def test_create_line_list_new_tests(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertEqual(
            line_adder.create_line_list(self.mock_dict_two),
            [
                'crbug.com/626703 [ three ] imported/fake/test/path.html [ Pass ]',
                'crbug.com/626703 [ two ] imported/fake/test/path.html [ Timeout ]',
                'crbug.com/626703 [ one ] imported/fake/test/path.html [ Pass ]',
            ])

    def test_merge_dicts_with_conflict_raise_exception(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertRaises(ValueError, line_adder.merge_dicts, self.mock_dict_two, self.mock_dict_four)

    def test_merge_dicts_merges_second_dict_into_first(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        output = line_adder.merge_dicts(self.mock_dict_one, self.mock_dict_three)
        self.assertEqual(output, self.mock_dict_one)
        output = line_adder.merge_dicts(self.mock_dict_two, self.mock_dict_three)
        self.assertEqual(output, self.mock_dict_two)

    def test_generate_results_dict(self):
        line_adder = W3CExpectationsLineAdder(MockHost())
        layout_test_list = [
            LayoutTestResult(
                'test/name.html', {
                    'expected': 'bar',
                    'actual': 'foo',
                    'is_unexpected': True,
                    'has_stderr': True
                }
            )]
        self.assertEqual(line_adder.generate_results_dict('dummy_platform', layout_test_list), {
            'test/name.html': {
                'dummy_platform': {
                    'expected': 'bar',
                    'actual': 'foo',
                    'bug': 'crbug.com/626703'
                }
            }
        })

    def test_write_to_test_expectations_with_marker_comment(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = MARKER_COMMENT + '\n'
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = ['crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]']
        line_adder.write_to_test_expectations(line_list)
        value = line_adder.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n'))

    def test_write_to_test_expectations_with_no_marker_comment(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = 'crbug.com/111 [ FakePlatform ]\n'
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = ['crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]']
        line_adder.write_to_test_expectations(line_list)
        value = self.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            ('crbug.com/111 [ FakePlatform ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]'))

    def test_write_to_test_expectations_skips_existing_lines(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = 'crbug.com/111 dont/copy/me.html [ Failure ]\n'
        line_adder = W3CExpectationsLineAdder(self.host)
        line_list = [
            'crbug.com/111 dont/copy/me.html [ Failure ]',
            'crbug.com/222 do/copy/me.html [ Failure ]'
        ]
        line_adder.write_to_test_expectations(line_list)
        value = self.host.filesystem.read_text_file(expectations_path)
        self.assertEqual(
            value,
            ('crbug.com/111 dont/copy/me.html [ Failure ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/222 do/copy/me.html [ Failure ]'))

    def test_write_to_test_expectations_with_marker_and_no_lines(self):
        expectations_path = '/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'
        self.host.filesystem.files[expectations_path] = (
            MARKER_COMMENT + '\n'
            'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n')
        line_adder = W3CExpectationsLineAdder(self.host)
        line_adder.write_to_test_expectations([])
        value = line_adder.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ FakePlatform ] fake/file/path.html [ Pass ]\n'))

    def test_is_js_test_true(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="/resources/testharness.js"></script>')
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertTrue(line_adder.is_js_test('foo/bar.html'))

    def test_is_js_test_false(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="ref-test.html"></script>')
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertFalse(line_adder.is_js_test('foo/bar.html'))

    def test_is_js_test_non_existent_file(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        self.assertFalse(line_adder.is_js_test('foo/bar.html'))

    def test_get_test_to_rebaseline_returns_only_tests_with_failures(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/imported/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/imported/other/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        line_adder = W3CExpectationsLineAdder(self.host)
        tests_to_rebaseline, _ = line_adder.get_tests_to_rebaseline(
            ['imported/fake/test/path.html', 'imported/other/test/path.html'], self.mock_dict_two)
        # The other test doesn't have an entry in the test results dict, so it is not listed as a test to rebaseline.
        self.assertEqual(tests_to_rebaseline, ['imported/fake/test/path.html'])

    def test_get_test_to_rebaseline_returns_only_js_tests(self):
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/imported/fake/test/path.html'] = (
            'this file does not look like a testharness JS test.')
        line_adder = W3CExpectationsLineAdder(self.host)
        tests_to_rebaseline, _ = line_adder.get_tests_to_rebaseline(
            ['imported/fake/test/path.html'], self.mock_dict_two)
        self.assertEqual(tests_to_rebaseline, [])

    def test_get_tests_to_rebaseline_returns_updated_dict(self):
        test_results_dict = {
            'imported/fake/test/path.html': {
                'one': {'expected': 'PASS', 'actual': 'TEXT', 'bug': 'crbug.com/626703'},
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'},
            },
        }
        test_results_dict_copy = copy.deepcopy(test_results_dict)
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/imported/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        line_adder = W3CExpectationsLineAdder(self.host)
        tests_to_rebaseline, modified_test_results = line_adder.get_tests_to_rebaseline(
            ['imported/fake/test/path.html'], test_results_dict)
        self.assertEqual(tests_to_rebaseline, ['imported/fake/test/path.html'])
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(modified_test_results, {
            'imported/fake/test/path.html': {
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT', 'bug': 'crbug.com/626703'},
            },
        })
        # The original dict isn't modified.
        self.assertEqual(test_results_dict, test_results_dict_copy)

    def test_run_no_issue_number(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        line_adder.get_issue_number = lambda: 'None'
        self.assertEqual(1, line_adder.run(args=[]))
        self.assertLog(['ERROR: No issue on current branch.\n'])

    def test_run_no_try_results(self):
        self.host.web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1],
            }),
            'https://codereview.chromium.org/api/11112222/1': json.dumps({
                'try_job_results': []
            })
        })
        line_adder = W3CExpectationsLineAdder(self.host)
        line_adder.get_issue_number = lambda: '11112222'
        line_adder.get_try_bots = lambda: ['test-builder-name']
        self.assertEqual(1, line_adder.run(args=[]))
        self.assertEqual(
            self.host.web.urls_fetched,
            [
                'https://codereview.chromium.org/api/11112222',
                'https://codereview.chromium.org/api/11112222/1'
            ])
        self.assertLog(['ERROR: No try job information was collected.\n'])

    def test_get_modified_existing_tests(self):
        line_adder = W3CExpectationsLineAdder(self.host)
        modified_files = [
            'third_party/WebKit/LayoutTests/a/b.html',
            'third_party/WebKit/LayoutTests/a/c.html',
            'x/y/z.cc',
        ]
        self.host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/a/b.html'] = ''
        self.host.filesystem.files['/mock-checkout/x/y/z.cc'] = ''
        self.host.executive = MockExecutive2(output='\n'.join(modified_files))
        tests = line_adder.get_modified_existing_tests()
        self.assertEqual(tests, ['a/b.html'])
        self.assertEqual(self.host.executive.calls, [['git', 'diff', 'origin/master', '--name-only', '--diff-filter=AMR']])
