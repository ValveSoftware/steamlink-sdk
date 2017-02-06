# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.net.buildbot_mock import MockBuilder
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline import *
from webkitpy.tool.mock_tool import MockTool, MockOptions


class BaseTestCase(unittest.TestCase):
    MOCK_WEB_RESULT = 'MOCK Web result, convert 404 to None=True'
    WEB_PREFIX = 'http://example.com/f/builders/MOCK Mac10.11/results/layout-test-results'

    command_constructor = None

    def setUp(self):
        self.tool = MockTool()
        # lint warns that command_constructor might not be set, but this is intentional; pylint: disable=E1102
        self.command = self.command_constructor()
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
        self.command.bind_to_tool(self.tool)
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

    def _setup_mock_builder_data(self):
        data = LayoutTestResults.results_from_string("""ADD_RESULTS({
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
});""")
        for builder in ['MOCK Win7', 'MOCK Win7 (dbg)', 'MOCK Mac10.11']:
            self.command._builder_data[builder] = data


class TestCopyExistingBaselinesInternal(BaseTestCase):
    command_constructor = CopyExistingBaselinesInternal

    def setUp(self):
        super(TestCopyExistingBaselinesInternal, self).setUp()

    def test_copying_overwritten_baseline(self):
        self.tool.executive = MockExecutive2()

        port = self.tool.port_factory.get('test-mac-mac10.10')
        self._write(port._filesystem.join(port.layout_tests_dir(),
                                          'platform/test-mac-mac10.10/failures/expected/image-expected.txt'), 'original mac10.11 result')

        oc = OutputCapture()
        try:
            options = MockOptions(builder="MOCK Mac10.11", suffixes="txt", verbose=True,
                                  test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(),
                                                                       'platform/test-mac-mac10.10/failures/expected/image-expected.txt')), 'original mac10.11 result')
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": [], "delete": []}\n')

    def test_copying_overwritten_baseline_to_multiple_locations(self):
        self.tool.executive = MockExecutive2()

        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(),
                                          'platform/test-win-win7/failures/expected/image-expected.txt'), 'original win7 result')

        oc = OutputCapture()
        try:
            options = MockOptions(builder="MOCK Win7", suffixes="txt", verbose=True,
                                  test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(),
                                                                       'platform/test-linux-trusty/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-linux-precise/userscripts/another-test-expected.txt')))
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-mac-mac10.10/userscripts/another-test-expected.txt')))
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": [], "delete": []}\n')

    def test_no_copy_existing_baseline(self):
        self.tool.executive = MockExecutive2()

        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(),
                                          'platform/test-win-win7/failures/expected/image-expected.txt'), 'original win7 result')

        oc = OutputCapture()
        try:
            options = MockOptions(builder="MOCK Win7", suffixes="txt", verbose=True,
                                  test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(),
                                                                       'platform/test-linux-trusty/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(),
                                                                       'platform/test-win-win7/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-mac-mac10.10/userscripts/another-test-expected.txt')))
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": [], "delete": []}\n')

    def test_no_copy_skipped_test(self):
        self.tool.executive = MockExecutive2()
        port = self.tool.port_factory.get('test-win-win7')
        fs = self.tool.filesystem
        self._write(
            fs.join(
                port.layout_tests_dir(),
                'platform/test-win-win7/failures/expected/image-expected.txt'),
            'original win7 result')
        expectations_path = fs.join(port.path_to_generic_test_expectations_file())
        self._write(expectations_path, (
            "[ Win ] failures/expected/image.html [ Failure ]\n"
            "[ Linux ] failures/expected/image.html [ Skip ]\n"))
        oc = OutputCapture()
        try:
            options = MockOptions(builder="MOCK Win7", suffixes="txt", verbose=True,
                                  test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            oc.restore_output()

        self.assertFalse(
            fs.exists(
                fs.join(
                    port.layout_tests_dir(),
                    'platform/test-mac-mac10.10/failures/expected/image-expected.txt')))
        self.assertFalse(
            fs.exists(
                fs.join(
                    port.layout_tests_dir(),
                    'platform/test-linux-trusty/failures/expected/image-expected.txt')))
        self.assertFalse(fs.exists(fs.join(port.layout_tests_dir(),
                                           'platform/test-linux-precise/failures/expected/image-expected.txt')))
        self.assertEqual(self._read(fs.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')),
                         'original win7 result')


class TestRebaselineTest(BaseTestCase):
    command_constructor = RebaselineTest  # AKA webkit-patch rebaseline-test-internal

    def setUp(self):
        super(TestRebaselineTest, self).setUp()
        self.options = MockOptions(
            builder="MOCK Mac10.11", test="userscripts/another-test.html", suffixes="txt", results_directory=None,
            master_name='chromium.mymaster', build_number=None)

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
        self._write(self.mac_expectations_path, """Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]
Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]
""")
        self._write("fast/dom/Window/window-postmessage-clone-really-deep-array.html", "Dummy test contents")
        self._write("fast/css/large-list-of-rules-crash.html", "Dummy test contents")
        self._write("userscripts/another-test.html", "Dummy test contents")

        self.options.suffixes = "png,wav,txt"
        self.command._rebaseline_test_and_update_expectations(self.options)

        self.assertItemsEqual(self.tool.web.urls_fetched,
                              [self.WEB_PREFIX + '/userscripts/another-test-actual.png',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.wav',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])
        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(new_expectations, """Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]
Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]
""")

    def test_rebaseline_test(self):
        self.command._rebaseline_test("MOCK Trusty", "userscripts/another-test.html", "txt", self.WEB_PREFIX)
        self.assertItemsEqual(self.tool.web.urls_fetched, [self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])

    def test_rebaseline_test_with_results_directory(self):
        self._write("userscripts/another-test.html", "test data")
        self._write(self.mac_expectations_path,
                    "Bug(x) [ Mac ] userscripts/another-test.html [ Failure ]\nbug(z) [ Linux ] userscripts/another-test.html [ Failure ]\n")
        self.options.results_directory = '/tmp'
        self.command._rebaseline_test_and_update_expectations(self.options)
        self.assertItemsEqual(self.tool.web.urls_fetched, ['file:///tmp/userscripts/another-test-actual.txt'])

    def test_rebaseline_reftest(self):
        self._write("userscripts/another-test.html", "test data")
        self._write("userscripts/another-test-expected.html", "generic result")
        self.options.suffixes = 'png'
        OutputCapture().assert_outputs(self, self.command._rebaseline_test_and_update_expectations, args=[self.options],
                                       expected_logs="Cannot rebaseline image result for reftest: userscripts/another-test.html\n")
        self.assertDictEqual(self.command._scm_changes, {'add': [], 'remove-lines': [], "delete": []})

    def test_rebaseline_test_and_print_scm_changes(self):
        self.command._print_scm_changes = True
        self.command._scm_changes = {'add': [], 'delete': []}
        self.tool._scm.exists = lambda x: False

        self.command._rebaseline_test("MOCK Trusty", "userscripts/another-test.html", "txt", None)

        self.assertDictEqual(self.command._scm_changes, {
            'add': ['/test.checkout/LayoutTests/platform/test-linux-trusty/userscripts/another-test-expected.txt'],
            'delete': []
        })

    def test_rebaseline_test_internal_with_port_that_lacks_buildbot(self):
        self.tool.executive = MockExecutive2()

        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(),
                                          'platform/test-win-win10/failures/expected/image-expected.txt'), 'original win10 result')

        oc = OutputCapture()
        try:
            options = MockOptions(
                optimize=True, builder="MOCK Win10", suffixes="txt", verbose=True, test="failures/expected/image.html",
                results_directory=None, master_name='chromium.mymaster', build_number=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(
        ), 'platform/test-win-win10/failures/expected/image-expected.txt')), 'MOCK Web result, convert 404 to None=True')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')))
        self.assertMultiLineEqual(
            out, '{"add": [], "remove-lines": [{"test": "failures/expected/image.html", "builder": "MOCK Win10"}], "delete": []}\n')


class TestAbstractParallelRebaselineCommand(BaseTestCase):
    command_constructor = AbstractParallelRebaselineCommand

    def test_builders_to_fetch_from(self):
        builders_to_fetch = self.command._builders_to_fetch_from(
            ["MOCK Win10", "MOCK Win7 (dbg)(1)", "MOCK Win7 (dbg)(2)", "MOCK Win7"])
        self.assertEqual(builders_to_fetch, ["MOCK Win7", "MOCK Win10"])


class TestRebaselineJson(BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJson, self).setUp()
        self.tool.executive = MockExecutive2()

    def tearDown(self):
        super(TestRebaselineJson, self).tearDown()

    def test_rebaseline_test_passes_on_all_builders(self):
        self._setup_mock_builder_data()

        def builder_data():
            self.command._builder_data['MOCK Win7'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "userscripts": {
            "first-test.html": {
                "expected": "NEEDSREBASELINE",
                "actual": "PASS"
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        options = MockOptions(optimize=True, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Win7": ["txt", "png"]}})

        self.assertEqual(self.tool.executive.calls, [])

    def test_rebaseline_all(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=True, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Win7": ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(self.tool.executive.calls,
                         [
                             [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                               '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                             [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                               '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                             [['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt,png',
                               'userscripts/first-test.html', '--verbose']]
                         ])

    def test_rebaseline_debug(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=True, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Win7 (dbg)": ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(self.tool.executive.calls,
                         [
                             [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                               '--builder', 'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']],
                             [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder',
                               'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']],
                             [['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt,png',
                               'userscripts/first-test.html', '--verbose']]
                         ])

    def test_no_optimize(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=False, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Win7 (dbg)": ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(self.tool.executive.calls,
                         [[['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']],
                          [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7 (dbg)', '--test', 'userscripts/first-test.html', '--verbose']]])

    def test_results_directory(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=False, verbose=True, results_directory='/tmp')
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Win7": ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(self.tool.executive.calls,
                         [[['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--results-directory', '/tmp', '--verbose']],
                          [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--results-directory', '/tmp', '--verbose']]])


class TestRebaselineJsonUpdatesExpectationsFiles(BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJsonUpdatesExpectationsFiles, self).setUp()
        self.tool.executive = MockExecutive2()

        def mock_run_command(*args, **kwargs):  # pylint: disable=unused-argument
            return '{"add": [], "remove-lines": [{"test": "userscripts/first-test.html", "builder": "MOCK Mac10.11"}]}\n'
        self.tool.executive.run_command = mock_run_command

    def test_rebaseline_updates_expectations_file(self):
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path,
                    "Bug(x) [ Mac ] userscripts/first-test.html [ Failure ]\nbug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Mac10.11": ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Mac10.10 ] userscripts/first-test.html [ Failure ]\nbug(z) [ Linux ] userscripts/first-test.html [ Failure ]\n")

    def test_rebaseline_updates_expectations_file_all_platforms(self):
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Mac10.11": ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")

    def test_rebaseline_handles_platform_skips(self):
        # This test is just like test_rebaseline_updates_expectations_file_all_platforms(),
        # except that if a particular port happens to SKIP a test in an overrides file,
        # we count that as passing, and do not think that we still need to rebaseline it.
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("NeverFixTests", "Bug(y) [ Android ] userscripts [ WontFix ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Mac10.11": ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")

    def test_rebaseline_handles_skips_in_file(self):
        # This test is like test_Rebaseline_handles_platform_skips, except that the
        # Skip is in the same (generic) file rather than a platform file. In this case,
        # the Skip line should be left unmodified. Note that the first line is now
        # qualified as "[Linux Mac Win]"; if it was unqualified, it would conflict with
        # the second line.
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path,
                    ("Bug(x) [ Linux Mac Win ] userscripts/first-test.html [ Failure ]\n"
                     "Bug(y) [ Android ] userscripts/first-test.html [ Skip ]\n"))
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Mac10.11": ["txt", "png"]}})

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
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.mac_expectations_path, "Bug(x) userscripts/first-test.html [ Failure ]\n")
        self._write("SmokeTests", "fast/html/article-element.html")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options, {"userscripts/first-test.html": {"MOCK Mac10.11": ["txt", "png"]}})

        new_expectations = self._read(self.mac_expectations_path)
        self.assertMultiLineEqual(
            new_expectations, "Bug(x) [ Linux Mac10.10 Win ] userscripts/first-test.html [ Failure ]\n")


class TestRebaseline(BaseTestCase):
    # This command shares most of its logic with RebaselineJson, so these tests just test what is different.

    command_constructor = Rebaseline  # AKA webkit-patch rebaseline

    def test_rebaseline(self):
        self.command._builders_to_pull_from = lambda: [MockBuilder('MOCK Win7')]

        self._write("userscripts/first-test.html", "test data")

        self._zero_out_test_expectations()
        self._setup_mock_builder_data()

        self.command.execute(MockOptions(results_directory=False, optimize=False, builders=None,
                                         suffixes="txt,png", verbose=True), ['userscripts/first-test.html'], self.tool)

        self.assertEqual(self.tool.executive.calls,
                         [[['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']],
                          [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Win7', '--test', 'userscripts/first-test.html', '--verbose']]])

    def test_rebaseline_directory(self):
        self.command._builders_to_pull_from = lambda: [MockBuilder('MOCK Win7')]

        self._write("userscripts/first-test.html", "test data")
        self._write("userscripts/second-test.html", "test data")

        self._setup_mock_builder_data()

        self.command.execute(MockOptions(results_directory=False, optimize=False, builders=None,
                                         suffixes="txt,png", verbose=True), ['userscripts'], self.tool)

        self.assertEqual(self.tool.executive.calls,
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


class MockLineRemovingExecutive(MockExecutive):

    def run_in_parallel(self, commands):
        assert len(commands)

        num_previous_calls = len(self.calls)
        command_outputs = []
        for cmd_line, cwd in commands:
            out = self.run_command(cmd_line, cwd=cwd)
            if 'rebaseline-test-internal' in cmd_line:
                out = '{"add": [], "remove-lines": [{"test": "%s", "builder": "%s"}], "delete": []}\n' % (cmd_line[8], cmd_line[6])
            command_outputs.append([0, out, ''])

        new_calls = self.calls[num_previous_calls:]
        self.calls = self.calls[:num_previous_calls]
        self.calls.append(new_calls)
        return command_outputs


class TestRebaselineExpectations(BaseTestCase):
    command_constructor = RebaselineExpectations

    def setUp(self):
        super(TestRebaselineExpectations, self).setUp()
        self.options = MockOptions(optimize=False, builders=None, suffixes=['txt'],
                                   verbose=False, platform=None, results_directory=None)

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def test_rebaseline_expectations(self):
        self._zero_out_test_expectations()

        self.tool.executive = MockExecutive2()

        def builder_data():
            self.command._builder_data['MOCK Mac10.11'] = self.command._builder_data['MOCK Mac10.10'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
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
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self._write("userscripts/another-test.html", "Dummy test contents")
        self._write("userscripts/images.svg", "Dummy test contents")
        self.command._tests_to_rebaseline = lambda port: {
            'userscripts/another-test.html': set(['txt']),
            'userscripts/images.svg': set(['png']),
            'userscripts/not-actually-failing.html': set(['txt', 'png', 'wav']),
        }

        self.command.execute(self.options, [], self.tool)

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

        def builder_data():
            self.command._builder_data['MOCK Mac10.10'] = self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
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
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self._write("userscripts/reftest-text.html", "Dummy test contents")
        self._write("userscripts/reftest-text-expected.html", "Dummy test contents")
        self._write("userscripts/reftest-text-expected.html", "Dummy test contents")
        self.command._tests_to_rebaseline = lambda port: {
            'userscripts/reftest-text.html': set(['txt']),
            'userscripts/reftest-image.html': set(['png']),
            'userscripts/reftest-image-text.html': set(['png', 'txt']),
        }

        self.command.execute(self.options, [], self.tool)

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
            self.command.execute(self.options, [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
            self.assertEqual(self.tool.filesystem.written_files, {})
            self.assertEqual(logs, 'Did not find any tests marked Rebaseline.\n')

    def disabled_test_overrides_are_included_correctly(self):
        # This tests that the any tests marked as REBASELINE in the overrides are found, but
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

        def builder_data():
            self.command._builder_data['MOCK Mac10.10'] = self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ Rebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.execute(self.options, [], self.tool)
        self.assertEqual(self.tool.executive.calls, [])

        # The mac ports should both be removed since they're the only ones in the builder list.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ Rebaseline ]
""")

    def test_rebaseline_missing(self):
        def builder_data():
            self.command._builder_data['MOCK Mac10.10'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "missing-text.html": {
                    "expected": "PASS",
                    "actual": "MISSING",
                    "is_unexpected": true,
                    "is_missing_text": true
                },
                "missing-text-and-image.html": {
                    "expected": "PASS",
                    "actual": "MISSING",
                    "is_unexpected": true,
                    "is_missing_text": true,
                    "is_missing_image": true
                },
                "missing-image.html": {
                    "expected": "PASS",
                    "actual": "MISSING",
                    "is_unexpected": true,
                    "is_missing_image": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self._write('fast/dom/missing-text.html', "Dummy test contents")
        self._write('fast/dom/missing-text-and-image.html', "Dummy test contents")
        self._write('fast/dom/missing-image.html', "Dummy test contents")

        self.command._tests_to_rebaseline = lambda port: {
            'fast/dom/missing-text.html': set(['txt', 'png']),
            'fast/dom/missing-text-and-image.html': set(['txt', 'png']),
            'fast/dom/missing-image.html': set(['txt', 'png']),
        }

        self.command.execute(self.options, [], self.tool)

        print self.tool.executive.calls
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


class TestOptimizeBaselines(BaseTestCase):
    command_constructor = OptimizeBaselines

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def setUp(self):
        super(TestOptimizeBaselines, self).setUp()

    def test_modify_scm(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")

        OutputCapture().assert_outputs(self, self.command.execute, args=[
            MockOptions(suffixes='txt', no_modify_scm=False, platform='test-mac-mac10.10'),
            ['another/test.html'],
            self.tool,
        ], expected_stdout='{"add": [], "remove-lines": [], "delete": []}\n')

        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/test-mac-mac10.10/another/test-expected.txt')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.txt')))

    def test_no_modify_scm(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")

        OutputCapture().assert_outputs(self, self.command.execute, args=[
            MockOptions(suffixes='txt', no_modify_scm=True, platform='test-mac-mac10.10'),
            ['another/test.html'],
            self.tool,
        ], expected_stdout='{"add": [], "remove-lines": [], "delete": ["/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.txt"]}\n')

        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/mac/another/test-expected.txt')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.txt')))

    def test_optimize_all_suffixes_by_default(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.png', "result A png")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.png', "result A png")

        try:
            oc = OutputCapture()
            oc.capture_output()
            self.command.execute(MockOptions(suffixes='txt,wav,png', no_modify_scm=True, platform='test-mac-mac10.10'),
                                 ['another/test.html'],
                                 self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertEquals(out, '{"add": [], "remove-lines": [], "delete": ["/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.txt", "/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.png"]}\n')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/mac/another/test-expected.txt')))
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/mac/another/test-expected.png')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.txt')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.png')))


class TestAutoRebaseline(BaseTestCase):
    command_constructor = AutoRebaseline

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def _execute_command_with_mock_options(self, auth_refresh_token_json=None, commit_author=None, dry_run=False):
        self.command.execute(
            MockOptions(optimize=True, verbose=False, results_directory=False,
                        auth_refresh_token_json=auth_refresh_token_json,
                        commit_author=commit_author, dry_run=dry_run),
            [], self.tool)

    def setUp(self):
        super(TestAutoRebaseline, self).setUp()
        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
            "MOCK Precise": {"port_name": "test-linux-precise", "specifiers": ["Precise", "Release"]},
            "MOCK Trusty": {"port_name": "test-linux-trusty", "specifiers": ["Trusty", "Release"]},
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
            "MOCK Win7 (dbg)": {"port_name": "test-win-win7", "specifiers": ["Win7", "Debug"]},
        })

        self.command.latest_revision_processed_on_all_bots = lambda: 9000
        self.command.bot_revision_data = lambda scm: [{"builder": "MOCK Win7", "revision": "9000"}]

    def test_release_builders(self):
        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11 (dbg)": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Debug"]},
            "MOCK Mac10.11 ASAN": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })
        self.assertEqual(self.command._release_builders(), ['MOCK Mac10.10'])

    def test_tests_to_rebaseline(self):
        def blame(_):
            return """
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org@bbb929c8-8fbe-4397-9dbb-9b2b20218538> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline-email-with-hash.html [ Failure ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) path/to/rebaseline-without-bug-number.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/rebaseline-with-modifiers.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 crbug.com/234 path/to/rebaseline-without-modifiers.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org@bbb929c8-8fbe-4397-9dbb-9b2b20218538> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/rebaseline-new-revision.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (<foo@chromium.org@@bbb929c8-8fbe-4397-9dbb-9b2b20218538>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
            set(['path/to/rebaseline-without-bug-number.html',
                 'path/to/rebaseline-with-modifiers.html', 'path/to/rebaseline-without-modifiers.html']),
            5678,
            '624c3081c0',
            'foobarbaz1@chromium.org',
            set(['24182', '234']),
            True))

    def test_tests_to_rebaseline_over_limit(self):
        def blame(_):
            result = ""
            for i in range(0, self.command.MAX_LINES_TO_REBASELINE + 1):
                result += "624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) crbug.com/24182 path/to/rebaseline-%s.html [ NeedsRebaseline ]\n" % i
            return result
        self.tool.scm().blame = blame

        expected_list_of_tests = []
        for i in range(0, self.command.MAX_LINES_TO_REBASELINE):
            expected_list_of_tests.append("path/to/rebaseline-%s.html" % i)

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
            set(expected_list_of_tests),
            5678,
            '624c3081c0',
            'foobarbaz1@chromium.org',
            set(['24182']),
            True))

    def test_commit_message(self):
        author = "foo@chromium.org"
        revision = 1234
        commit = "abcd567"
        bugs = set()
        self.assertEqual(self.command.commit_message(author, revision, commit, bugs),
                         """Auto-rebaseline for r1234

https://chromium.googlesource.com/chromium/src/+/abcd567

TBR=foo@chromium.org
""")

        bugs = set(["234", "345"])
        self.assertEqual(self.command.commit_message(author, revision, commit, bugs),
                         """Auto-rebaseline for r1234

https://chromium.googlesource.com/chromium/src/+/abcd567

BUG=234,345
TBR=foo@chromium.org
""")

    def test_no_needs_rebaseline_lines(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
"""
        self.tool.scm().blame = blame

        self._execute_command_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [])

    def test_execute(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) # Test NeedsRebaseline being in a comment doesn't bork parsing.
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Mac10.11 ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        original_builder_data = self.command.builder_data
        def builder_data():
            original_builder_data()
            # have prototype-chocolate only fail on "MOCK Mac10.10".
            self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "PASS",
                    "actual": "PASS TEXT",
                    "is_unexpected": true
                },
                "prototype-chocolate.html": {
                    "expected": "FAIL",
                    "actual": "PASS"
                },
                "prototype-strawberry.html": {
                    "expected": "PASS",
                    "actual": "IMAGE PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ Mac10.11 ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-strawberry.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-chocolate.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'closed'
        self._execute_command_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [])

        self.command.tree_status = lambda: 'open'
        self.tool.executive.calls = []
        self._execute_command_with_mock_options()

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/prototype-chocolate.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/prototype-taco.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt,png',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/prototype-chocolate.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder',
                 'MOCK Mac10.11', '--test', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.10', '--test', 'fast/dom/prototype-taco.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [
                ['python', 'echo', 'optimize-baselines', '--no-modify-scm',
                 '--suffixes', 'txt,png', 'fast/dom/prototype-chocolate.html'],
                ['python', 'echo', 'optimize-baselines', '--no-modify-scm',
                 '--suffixes', 'png', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt', 'fast/dom/prototype-taco.html'],
            ],
            ['git', 'cl', 'upload', '-f'],
            ['git', 'pull'],
            ['git', 'cl', 'land', '-f', '-v'],
            ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
        ])

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ Linux Win ] fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")

    def test_execute_git_cl_hangs(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        original_builder_data = self.command.builder_data
        def builder_data():
            original_builder_data()
            # have prototype-chocolate only fail on "MOCK Mac10.10".
            self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "PASS",
                    "actual": "PASS TEXT",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.builders = BuilderList({
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.SECONDS_BEFORE_GIVING_UP = 0
        self.command.tree_status = lambda: 'open'
        self.tool.executive = MockExecutive()
        self.tool.executive.calls = []
        self._execute_command_with_mock_options()

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt', 'fast/dom/prototype-taco.html']],
            ['git', 'cl', 'upload', '-f'],
        ])

    def test_execute_test_passes_everywhere(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        def builder_data():
            self.command._builder_data['MOCK Mac10.10'] = self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'open'
        self._execute_command_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [
            ['git', 'cl', 'upload', '-f'],
            ['git', 'pull'],
            ['git', 'cl', 'land', '-f', '-v'],
            ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
        ])

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

    def test_execute_use_alternate_rebaseline_branch(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        def builder_data():
            self.command._builder_data['MOCK Win'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
        })
        old_branch_name = self.tool.scm().current_branch_or_ref
        try:
            self.command.tree_status = lambda: 'open'
            self.tool.scm().current_branch_or_ref = lambda: 'auto-rebaseline-temporary-branch'
            self._execute_command_with_mock_options()
            self.assertEqual(self.tool.executive.calls, [
                ['git', 'cl', 'upload', '-f'],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v'],
                ['git', 'config', 'branch.auto-rebaseline-alt-temporary-branch.rietveldissue'],
            ])

            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Mac Win10 ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")
        finally:
            self.tool.scm().current_branch_or_ref = old_branch_name

    def test_execute_stuck_on_alternate_rebaseline_branch(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        def builder_data():
            self.command._builder_data['MOCK Win'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
        })

        old_branch_name = self.tool.scm().current_branch_or_ref
        try:
            self.command.tree_status = lambda: 'open'
            self.tool.scm().current_branch_or_ref = lambda: 'auto-rebaseline-alt-temporary-branch'
            self._execute_command_with_mock_options()
            self.assertEqual(self.tool.executive.calls, [
                ['git', 'cl', 'upload', '-f'],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v'],
                ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
            ])

            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Mac Win10 ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")
        finally:
            self.tool.scm().current_branch_or_ref = old_branch_name

    def _basic_execute_test(self, expected_executive_calls, auth_refresh_token_json=None, commit_author=None, dry_run=False):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        def builder_data():
            self.command._builder_data['MOCK Mac10.10'] = self.command._builder_data['MOCK Mac10.11'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "FAIL",
                    "actual": "PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'open'
        self._execute_command_with_mock_options(auth_refresh_token_json=auth_refresh_token_json,
                                                commit_author=commit_author, dry_run=dry_run)
        self.assertEqual(self.tool.executive.calls, expected_executive_calls)

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

    def test_execute_with_rietveld_auth_refresh_token(self):
        RIETVELD_REFRESH_TOKEN = '/creds/refresh_tokens/test_rietveld_token'
        self._basic_execute_test(
            [
                ['git', 'cl', 'upload', '-f', '--auth-refresh-token-json', RIETVELD_REFRESH_TOKEN],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v', '--auth-refresh-token-json', RIETVELD_REFRESH_TOKEN],
                ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
            ],
            auth_refresh_token_json=RIETVELD_REFRESH_TOKEN)

    def test_execute_with_dry_run(self):
        self._basic_execute_test([], dry_run=True)
        self.assertEqual(self.tool.scm().local_commits(), [])
