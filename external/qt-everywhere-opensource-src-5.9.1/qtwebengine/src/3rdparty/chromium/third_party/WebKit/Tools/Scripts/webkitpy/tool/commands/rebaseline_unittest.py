# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import unittest

from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline import (
    AbstractParallelRebaselineCommand, CopyExistingBaselinesInternal,
    Rebaseline, RebaselineExpectations, RebaselineJson, RebaselineTest
)
from webkitpy.tool.mock_tool import MockWebKitPatch


# pylint: disable=protected-access
class BaseTestCase(unittest.TestCase):
    WEB_PREFIX = 'https://storage.googleapis.com/chromium-layout-test-archives/MOCK_Mac10_11/results/layout-test-results'

    command_constructor = None

    def setUp(self):
        self.tool = MockWebKitPatch()
        # lint warns that command_constructor might not be set, but this is intentional; pylint: disable=E1102
        self.command = self.command_constructor()
        self.command._tool = self.tool
        self.tool.builders = BuilderList({
            "MOCK Mac10.10 (dbg)": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Debug"]},
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11 (dbg)": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Debug"]},
            "MOCK Mac10.11 ASAN": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
            "MOCK Precise": {"port_name": "test-linux-precise", "specifiers": ["Precise", "Release"]},
            "MOCK Trusty": {"port_name": "test-linux-trusty", "specifiers": ["Trusty", "Release"]},
            "MOCK Win10": {"port_name": "test-win-win10", "specifiers": ["Win10", "Release"]},
            "MOCK Win7 (dbg)": {"port_name": "test-win-win7", "specifiers": ["Win7", "Debug"]},
            "MOCK Win7 (dbg)(1)": {"port_name": "test-win-win7", "specifiers": ["Win7", "Debug"]},
            "MOCK Win7 (dbg)(2)": {"port_name": "test-win-win7", "specifiers": ["Win7", "Debug"]},
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
        })
        self.mac_port = self.tool.port_factory.get_from_builder_name("MOCK Mac10.11")

        self.mac_expectations_path = self.mac_port.path_to_generic_test_expectations_file()
        self.tool.filesystem.write_text_file(
            self.tool.filesystem.join(self.mac_port.layout_tests_dir(), "VirtualTestSuites"), '[]')

        # In AbstractParallelRebaselineCommand._rebaseline_commands, a default port
        # object is gotten using self.tool.port_factory.get(), which is used to get
        # test paths -- and the layout tests directory may be different for the "test"
        # ports and real ports. Since only "test" ports are used in this class,
        # we can make the default port also a "test" port.
        self.original_port_factory_get = self.tool.port_factory.get
        test_port = self.tool.port_factory.get('test')

        def get_test_port(port_name=None, options=None, **kwargs):
            if not port_name:
                return test_port
            return self.original_port_factory_get(port_name, options, **kwargs)

        self.tool.port_factory.get = get_test_port

    def tearDown(self):
        self.tool.port_factory.get = self.original_port_factory_get

    def _expand(self, path):
        if self.tool.filesystem.isabs(path):
            return path
        return self.tool.filesystem.join(self.mac_port.layout_tests_dir(), path)

    def _read(self, path):
        return self.tool.filesystem.read_text_file(self._expand(path))

    def _write(self, path, contents):
        self.tool.filesystem.write_text_file(self._expand(path), contents)

    def _zero_out_test_expectations(self):
        for port_name in self.tool.port_factory.all_port_names():
            port = self.tool.port_factory.get(port_name)
            for path in port.expectations_files():
                self._write(path, '')
        self.tool.filesystem.written_files = {}

    def _setup_mock_build_data(self):
        for builder in ['MOCK Win7', 'MOCK Win7 (dbg)', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "userscripts": {
                        "first-test.html": {
                            "expected": "PASS",
                            "actual": "IMAGE+TEXT"
                        },
                        "second-test.html": {
                            "expected": "FAIL",
                            "actual": "IMAGE+TEXT"
                        }
                    }
                }
            }))


class TestCopyExistingBaselinesInternal(BaseTestCase):
    command_constructor = CopyExistingBaselinesInternal

    def setUp(self):
        super(TestCopyExistingBaselinesInternal, self).setUp()

    def options(self, **kwargs):
        options_dict = {
            'results_directory': None,
            'suffixes': 'txt',
            'verbose': False,
        }
        options_dict.update(kwargs)
        return optparse.Values(options_dict)

    def test_copy_baseline_mac(self):
        port = self.tool.port_factory.get('test-mac-mac10.11')
        self._write(
            port.host.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-mac-mac10.11/failures/expected/image-expected.txt'),
            'original mac10.11 result')
        self.assertFalse(self.tool.filesystem.exists(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-mac-mac10.10/failures/expected/image-expected.txt')))

        self.command.execute(self.options(builder='MOCK Mac10.11', test='failures/expected/image.html'), [], self.tool)

        # The Mac 10.11 baseline is copied over to the Mac 10.10 directory,
        # because Mac10.10 is the "immediate predecessor" in the fallback tree.
        # That means that normally for Mac10.10 if there's no Mac10.10-specific
        # baseline, then we fall back to the Mac10.11 baseline.
        # The idea is, if in the next step we download new baselines for Mac10.11
        # but not Mac10.10, then mac10.10 will still have the correct baseline.
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-mac-mac10.11/failures/expected/image-expected.txt')),
            'original mac10.11 result')
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-mac-mac10.10/failures/expected/image-expected.txt')),
            'original mac10.11 result')

    def test_copying_overwritten_baseline_to_multiple_locations(self):
        self.tool.executive = MockExecutive2()

    def test_copy_baseline_win7_to_linux_trusty(self):
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt'),
            'original win7 result')
        self.assertFalse(self.tool.filesystem.exists(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-linux-trusty/failures/expected/image-expected.txt')))

        self.command.execute(self.options(builder='MOCK Win7', test='failures/expected/image.html'), [], self.tool)

        # The Mac Win7 baseline is copied over to the Linux Trusty directory,
        # because Linux Trusty is the baseline fallback "immediate predecessor" of Win7.
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt')),
            'original win7 result')
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-linux-trusty/failures/expected/image-expected.txt')),
            'original win7 result')

    def test_no_copy_existing_baseline(self):
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt'),
            'original win7 result')
        self._write(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-linux-trusty/failures/expected/image-expected.txt'),
            'original linux trusty result')

        self.command.execute(self.options(builder='MOCK Win7', test='failures/expected/image.html'), [], self.tool)

        # Since a baseline existed already for Linux Trusty, the Win7 baseline is not copied over.
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt')),
            'original win7 result')
        self.assertEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-linux-trusty/failures/expected/image-expected.txt')),
            'original linux trusty result')

    def test_no_copy_skipped_test(self):
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt'),
            'original win7 result')
        self._write(
            port.path_to_generic_test_expectations_file(),
            ("[ Win ] failures/expected/image.html [ Failure ]\n"
             "[ Linux ] failures/expected/image.html [ Skip ]\n"))

        self.command.execute(self.options(builder='MOCK Win7', test='failures/expected/image.html'), [], self.tool)

        # The Win7 baseline is not copied over to the Linux Trusty directory
        # because the test is skipped on linux.
        self.assertFalse(
            self.tool.filesystem.exists(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-linux-trusty/failures/expected/image-expected.txt')))


class TestRebaselineTest(BaseTestCase):
    command_constructor = RebaselineTest  # AKA webkit-patch rebaseline-test-internal

    def setUp(self):
        super(TestRebaselineTest, self).setUp()

    @staticmethod
    def options(**kwargs):
        return optparse.Values(dict({
            'builder': "MOCK Mac10.11",
            'test': "userscripts/another-test.html",
            'suffixes': "txt",
            'results_directory': None,
            'build_number': None
        }, **kwargs))

    def test_baseline_directory(self):
        command = self.command
        self.assertMultiLineEqual(command._baseline_directory("MOCK Mac10.11"),
                                  "/test.checkout/LayoutTests/platform/test-mac-mac10.11")
        self.assertMultiLineEqual(command._baseline_directory("MOCK Mac10.10"),
                                  "/test.checkout/LayoutTests/platform/test-mac-mac10.10")
        self.assertMultiLineEqual(command._baseline_directory("MOCK Trusty"),
                                  "/test.checkout/LayoutTests/platform/test-linux-trusty")
        self.assertMultiLineEqual(command._baseline_directory("MOCK Precise"),
                                  "/test.checkout/LayoutTests/platform/test-linux-precise")

    def test_rebaseline_updates_expectations_file_noop(self):
        self._zero_out_test_expectations()
        self._write(
            self.mac_expectations_path,
            ("Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]\n"
             "Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]\n"))
        self._write("fast/dom/Window/window-postmessage-clone-really-deep-array.html", "Dummy test contents")
        self._write("fast/css/large-list-of-rules-crash.html", "Dummy test contents")
        self._write("userscripts/another-test.html", "Dummy test contents")

        self.command._rebaseline_test_and_update_expectations(self.options(suffixes="png,wav,txt"))

        self.assertItemsEqual(self.tool.web.urls_fetched,
                              [self.WEB_PREFIX + '/userscripts/another-test-actual.png',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.wav',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])
        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ("Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]\n"
             "Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]\n"))

    def test_rebaseline_test(self):
        self.command._rebaseline_test("MOCK Trusty", "userscripts/another-test.html", "txt", self.WEB_PREFIX)
        self.assertItemsEqual(self.tool.web.urls_fetched, [self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])

    def test_rebaseline_test_with_results_directory(self):
        self._write("userscripts/another-test.html", "test data")
        self._write(
            self.mac_expectations_path,
            ("Bug(x) [ Mac ] userscripts/another-test.html [ Failure ]\n"
             "bug(z) [ Linux ] userscripts/another-test.html [ Failure ]\n"))
        self.command._rebaseline_test_and_update_expectations(self.options(results_directory='/tmp'))
        self.assertItemsEqual(self.tool.web.urls_fetched, ['file:///tmp/userscripts/another-test-actual.txt'])

    def test_rebaseline_reftest(self):
        self._write("userscripts/another-test.html", "test data")
        self._write("userscripts/another-test-expected.html", "generic result")
        OutputCapture().assert_outputs(
            self, self.command._rebaseline_test_and_update_expectations, args=[self.options(suffixes='png')],
            expected_logs="Cannot rebaseline image result for reftest: userscripts/another-test.html\n")
        self.assertDictEqual(self.command.expectation_line_changes.to_dict(), {'remove-lines': []})

    def test_rebaseline_test_internal_with_port_that_lacks_buildbot(self):
        self.tool.executive = MockExecutive2()

        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            port.host.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win10/failures/expected/image-expected.txt'),
            'original win10 result')

        oc = OutputCapture()
        try:
            options = optparse.Values({
                'optimize': True,
                'builder': "MOCK Win10",
                'suffixes': "txt",
                'verbose': True,
                'test': "failures/expected/image.html",
                'results_directory': None,
                'build_number': None
            })
            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win10/failures/expected/image-expected.txt')),
            'MOCK Web result, convert 404 to None=True')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')))
        self.assertMultiLineEqual(
            out, '{"remove-lines": [{"test": "failures/expected/image.html", "builder": "MOCK Win10"}]}\n')


class TestAbstractParallelRebaselineCommand(BaseTestCase):
    command_constructor = AbstractParallelRebaselineCommand

    def test_builders_to_fetch_from(self):
        builders_to_fetch = self.command._builders_to_fetch_from(
            ["MOCK Win10", "MOCK Win7 (dbg)(1)", "MOCK Win7 (dbg)(2)", "MOCK Win7"])
        self.assertEqual(builders_to_fetch, ["MOCK Win7", "MOCK Win10"])

    def test_all_baseline_paths(self):
        test_prefix_list = {
            'passes/text.html': {
                Build('MOCK Win7'): ('txt', 'png'),
                Build('MOCK Win10'): ('txt',),
            }
        }
        # pylint: disable=protected-access
        baseline_paths = self.command._all_baseline_paths(test_prefix_list)
        self.assertEqual(baseline_paths, [
            '/test.checkout/LayoutTests/passes/text-expected.png',
            '/test.checkout/LayoutTests/passes/text-expected.txt',
            '/test.checkout/LayoutTests/platform/test-win-win10/passes/text-expected.txt',
            '/test.checkout/LayoutTests/platform/test-win-win7/passes/text-expected.png',
            '/test.checkout/LayoutTests/platform/test-win-win7/passes/text-expected.txt',
        ])

    def test_remove_all_pass_testharness_baselines(self):
        self.tool.filesystem.write_text_file(
            '/test.checkout/LayoutTests/passes/text-expected.txt',
            ('This is a testharness.js-based test.\n'
             'PASS: foo\n'
             'Harness: the test ran to completion.\n'))
        test_prefix_list = {
            'passes/text.html': {
                Build('MOCK Win7'): ('txt', 'png'),
                Build('MOCK Win10'): ('txt',),
            }
        }
        self.command._remove_all_pass_testharness_baselines(test_prefix_list)
        self.assertFalse(self.tool.filesystem.exists(
            '/test.checkout/LayoutTests/passes/text-expected.txt'))


class TestRebaselineJson(BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJson, self).setUp()
        self.tool.executive = MockExecutive2()

    def tearDown(self):
        super(TestRebaselineJson, self).tearDown()

    @staticmethod
    def options(**kwargs):
        return optparse.Values(dict({
            'optimize': True,
            'verbose': True,
            'results_directory': None
        }, **kwargs))

    def test_rebaseline_test_passes_on_all_builders(self):
        self._setup_mock_build_data()

        self.tool.buildbot.set_results(Build('MOCK Win7'), LayoutTestResults({
            "tests": {
                "userscripts": {
                    "first-test.html": {
                        "expected": "NEEDSREBASELINE",
                        "actual": "PASS"
                    }
                }
            }
        }))

        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command.rebaseline(self.options(), {"userscripts/first-test.html": {Build("MOCK Win7"): ["txt", "png"]}})

        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_all(self):
        self._setup_mock_build_data()

        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command.rebaseline(self.options(), {"userscripts/first-test.html": {Build("MOCK Win7"): ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'optimize-baselines', '--suffixes', 'txt,png',
                  'userscripts/first-test.html', '--verbose']]
            ])

    def test_rebaseline_debug(self):
        self._setup_mock_build_data()

        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command.rebaseline(self.options(), {"userscripts/first-test.html": {Build("MOCK Win7 (dbg)"): ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder',
                  'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'optimize-baselines', '--suffixes', 'txt,png',
                  'userscripts/first-test.html', '--verbose']]
            ])

    def test_no_optimize(self):
        self._setup_mock_build_data()
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command.rebaseline(
            self.options(optimize=False),
            {"userscripts/first-test.html": {Build("MOCK Win7"): ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']]
            ])

    def test_results_directory(self):
        self._setup_mock_build_data()
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command.rebaseline(
            self.options(optimize=False, results_directory='/tmp'),
            {"userscripts/first-test.html": {Build("MOCK Win7"): ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose', '--results-directory', '/tmp']]
            ])


class TestRebaselineJsonUpdatesExpectationsFiles(BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJsonUpdatesExpectationsFiles, self).setUp()
        self.tool.executive = MockExecutive2()

        def mock_run_command(*args, **kwargs):  # pylint: disable=unused-argument
            return '{"add": [], "remove-lines": [{"test": "userscripts/first-test.html", "builder": "MOCK Mac10.11"}]}\n'
        self.tool.executive.run_command = mock_run_command

    @staticmethod
    def options():
        return optparse.Values({
            'optimize': False,
            'verbose': True,
            'results_directory': None
        })

    def test_rebaseline_updates_expectations_file(self):
        self._write(
            self.mac_expectations_path,
            ("Bug(x) [ Mac ] userscripts/first-test.html [ Failure ]\n"
             "bug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n"))
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_build_data()

        self.command.rebaseline(
            self.options(),
            {"userscripts/first-test.html": {Build("MOCK Mac10.11"): ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ("Bug(x) [ Mac10.10 ] userscripts/first-test.html [ Failure ]\n"
             "bug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n"))

    def test_rebaseline_updates_expectations_file_all_platforms(self):
        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_build_data()
        self.command.rebaseline(
            self.options(),
            {"userscripts/first-test.html": {Build("MOCK Mac10.11"): ["txt", "png"]}})
        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")

    def test_rebaseline_handles_platform_skips(self):
        # This test is just like test_rebaseline_updates_expectations_file_all_platforms(),
        # except that if a particular port happens to SKIP a test in an overrides file,
        # we count that as passing, and do not think that we still need to rebaseline it.
        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("NeverFixTests", "Bug(y) [ Android ] userscripts [ WontFix ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_build_data()

        self.command.rebaseline(
            self.options(),
            {"userscripts/first-test.html": {Build("MOCK Mac10.11"): ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")

    def test_rebaseline_handles_skips_in_file(self):
        # This test is like test_Rebaseline_handles_platform_skips, except that the
        # Skip is in the same (generic) file rather than a platform file. In this case,
        # the Skip line should be left unmodified. Note that the first line is now
        # qualified as "[Linux Mac Win]"; if it was unqualified, it would conflict with
        # the second line.
        self._write(self.mac_expectations_path,
                    ("Bug(x) [ Linux Mac Win ] userscripts/first-test.html [ Failure ]\n"
                     "Bug(y) [ Android ] userscripts/first-test.html [ Skip ]\n"))
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_build_data()

        self.command.rebaseline(
            self.options(),
            {"userscripts/first-test.html": {Build("MOCK Mac10.11"): ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ("Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n"
             "Bug(y) [ Android ] userscripts/first-test.html [ Skip ]\n"))

    def test_rebaseline_handles_smoke_tests(self):
        # This test is just like test_rebaseline_handles_platform_skips, except that we check for
        # a test not being in the SmokeTests file, instead of using overrides files.
        # If a test is not part of the smoke tests, we count that as passing on ports that only
        # run smoke tests, and do not think that we still need to rebaseline it.
        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("SmokeTests", "fast/html/article-element.html")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_build_data()

        self.command.rebaseline(
            self.options(),
            {"userscripts/first-test.html": {Build("MOCK Mac10.11"): ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")


class TestRebaseline(BaseTestCase):
    # This command shares most of its logic with RebaselineJson, so these tests just test what is different.

    command_constructor = Rebaseline  # AKA webkit-patch rebaseline

    def test_rebaseline(self):
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']

        self._write("userscripts/first-test.html", "test data")

        self._zero_out_test_expectations()
        self._setup_mock_build_data()
        options = optparse.Values({
            'results_directory': False,
            'optimize': False,
            'builders': None,
            'suffixes': "txt,png",
            'verbose': True
        })
        self.command.execute(options, ['userscripts/first-test.html'], self.tool)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                  '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']]
            ])

    def test_rebaseline_directory(self):
        self.command._builders_to_pull_from = lambda: ['MOCK Win7']

        self._write("userscripts/first-test.html", "test data")
        self._write("userscripts/second-test.html", "test data")

        self._setup_mock_build_data()
        options = optparse.Values({
            'results_directory': False,
            'optimize': False,
            'builders': None,
            'suffixes': "txt,png",
            'verbose': True
        })
        self.command.execute(options, ['userscripts'], self.tool)

        self.assertEqual(
            self.tool.executive.calls,
            [
                [
                    ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                     '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose'],
                    ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                     '--builder', 'MOCK Win7', '--test', 'userscripts/second-test.html', '--verbose']
                ],
                [
                    ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                     '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose'],
                    ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                     '--builder', 'MOCK Win7', '--test', 'userscripts/second-test.html', '--verbose']
                ]
            ])


class TestRebaselineExpectations(BaseTestCase):
    command_constructor = RebaselineExpectations

    def setUp(self):
        super(TestRebaselineExpectations, self).setUp()

    @staticmethod
    def options():
        return optparse.Values({
            'optimize': False,
            'builders': None,
            'suffixes': ['txt'],
            'verbose': False,
            'platform': None,
            'results_directory': None
        })

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def test_rebaseline_expectations(self):
        self._zero_out_test_expectations()

        self.tool.executive = MockExecutive2()

        for builder in ['MOCK Mac10.10', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "userscripts": {
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "PASS TEXT"
                        },
                        "images.svg": {
                            "expected": "FAIL",
                            "actual": "IMAGE+TEXT"
                        }
                    }
                }
            }))

        self._write("userscripts/another-test.html", "Dummy test contents")
        self._write("userscripts/images.svg", "Dummy test contents")
        self.command._tests_to_rebaseline = lambda port: {
            'userscripts/another-test.html': set(['txt']),
            'userscripts/images.svg': set(['png']),
            'userscripts/not-actually-failing.html': set(['txt', 'png', 'wav']),
        }

        self.command.execute(self.options(), [], self.tool)

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/another-test.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/another-test.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/images.svg'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/images.svg'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/another-test.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/another-test.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/images.svg'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/images.svg'],
            ],
        ])

    def test_rebaseline_expectations_reftests(self):
        self._zero_out_test_expectations()

        self.tool.executive = MockExecutive2()

        for builder in ['MOCK Mac10.10', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "userscripts": {
                        "reftest-text.html": {
                            "expected": "PASS",
                            "actual": "TEXT"
                        },
                        "reftest-image.html": {
                            "expected": "FAIL",
                            "actual": "IMAGE"
                        },
                        "reftest-image-text.html": {
                            "expected": "FAIL",
                            "actual": "IMAGE+TEXT"
                        }
                    }
                }
            }))

        self._write("userscripts/reftest-text.html", "Dummy test contents")
        self._write("userscripts/reftest-text-expected.html", "Dummy test contents")
        self._write("userscripts/reftest-text-expected.html", "Dummy test contents")
        self.command._tests_to_rebaseline = lambda port: {
            'userscripts/reftest-text.html': set(['txt']),
            'userscripts/reftest-image.html': set(['png']),
            'userscripts/reftest-image-text.html': set(['png', 'txt']),
        }

        self.command.execute(self.options(), [], self.tool)

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/reftest-text.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/reftest-text.html'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'userscripts/reftest-text.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'userscripts/reftest-text.html'],
            ],
        ])

    def test_rebaseline_expectations_noop(self):
        self._zero_out_test_expectations()

        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.options(), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
            self.assertEqual(self.tool.filesystem.written_files, {})
            self.assertEqual(logs, 'Did not find any tests marked Rebaseline.\n')

    def disabled_test_overrides_are_included_correctly(self):
        # TODO(qyearsley): Fix or remove this test method.
        # This tests that any tests marked as REBASELINE in the overrides are found, but
        # that the overrides do not get written into the main file.
        self._zero_out_test_expectations()

        self._write(self.mac_expectations_path, '')
        self.mac_port.expectations_dict = lambda: {
            self.mac_expectations_path: '',
            'overrides': ('Bug(x) userscripts/another-test.html [ Failure Rebaseline ]\n'
                          'Bug(y) userscripts/test.html [ Crash ]\n')}
        self._write('/userscripts/another-test.html', '')

        self.assertDictEqual(self.command._tests_to_rebaseline(self.mac_port),
                             {'userscripts/another-test.html': set(['png', 'txt', 'wav'])})
        self.assertEqual(self._read(self.mac_expectations_path), '')

    def test_rebaseline_without_other_expectations(self):
        self._write("userscripts/another-test.html", "Dummy test contents")
        self._write(self.mac_expectations_path, "Bug(x) userscripts/another-test.html [ Rebaseline ]\n")
        self.assertDictEqual(self.command._tests_to_rebaseline(self.mac_port),
                             {'userscripts/another-test.html': ('png', 'wav', 'txt')})

    def test_rebaseline_test_passes_everywhere(self):
        test_port = self.tool.port_factory.get('test')

        for builder in ['MOCK Mac10.10', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "fast": {
                        "dom": {
                            "prototype-taco.html": {
                                "expected": "FAIL",
                                "actual": "PASS",
                                "is_unexpected": True
                            }
                        }
                    }
                }
            }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ Rebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.execute(self.options(), [], self.tool)
        self.assertEqual(self.tool.executive.calls, [])

        # The mac ports should both be removed since they're the only ones in the builder list.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ Rebaseline ]
""")

    def test_rebaseline_missing(self):
        self.tool.buildbot.set_results(Build('MOCK Mac10.10'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "missing-text.html": {
                            "expected": "PASS",
                            "actual": "MISSING",
                            "is_unexpected": True,
                            "is_missing_text": True
                        },
                        "missing-text-and-image.html": {
                            "expected": "PASS",
                            "actual": "MISSING",
                            "is_unexpected": True,
                            "is_missing_text": True,
                            "is_missing_image": True
                        },
                        "missing-image.html": {
                            "expected": "PASS",
                            "actual": "MISSING",
                            "is_unexpected": True,
                            "is_missing_image": True
                        }
                    }
                }
            }
        }))

        self._write('fast/dom/missing-text.html', "Dummy test contents")
        self._write('fast/dom/missing-text-and-image.html', "Dummy test contents")
        self._write('fast/dom/missing-image.html', "Dummy test contents")

        self.command._tests_to_rebaseline = lambda port: {
            'fast/dom/missing-text.html': set(['txt', 'png']),
            'fast/dom/missing-text-and-image.html': set(['txt', 'png']),
            'fast/dom/missing-image.html': set(['txt', 'png']),
        }

        self.command.execute(self.options(), [], self.tool)

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-text.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-text-and-image.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-image.html'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-text.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-text-and-image.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/missing-image.html'],
            ]
        ])


class MockLineRemovingExecutive(MockExecutive):

    def run_in_parallel(self, commands):
        assert len(commands)

        num_previous_calls = len(self.calls)
        command_outputs = []
        for cmd_line, cwd in commands:
            out = self.run_command(cmd_line, cwd=cwd)
            if 'rebaseline-test-internal' in cmd_line:
                out = '{"remove-lines": [{"test": "%s", "builder": "%s"}]}\n' % (cmd_line[8], cmd_line[6])
            command_outputs.append([0, out, ''])

        new_calls = self.calls[num_previous_calls:]
        self.calls = self.calls[:num_previous_calls]
        self.calls.append(new_calls)
        return command_outputs
