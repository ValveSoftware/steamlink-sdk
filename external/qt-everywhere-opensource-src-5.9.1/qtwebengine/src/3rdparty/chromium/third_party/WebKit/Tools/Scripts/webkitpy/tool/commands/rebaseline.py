# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR/ OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function
import json
import logging
import optparse
import sys
import traceback

from webkitpy.common.memoized import memoized
from webkitpy.common.net.buildbot import Build
from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.models.testharness_results import is_all_pass_testharness_result
from webkitpy.layout_tests.models.test_expectations import TestExpectations, BASELINE_SUFFIX_LIST, SKIP
from webkitpy.layout_tests.port import factory
from webkitpy.tool.commands.command import Command


_log = logging.getLogger(__name__)


class AbstractRebaseliningCommand(Command):
    """Base class for rebaseline-related commands."""
    # Not overriding execute() - pylint: disable=abstract-method

    no_optimize_option = optparse.make_option(
        '--no-optimize', dest='optimize', action='store_false', default=True,
        help=('Do not optimize (de-duplicate) the expectations after rebaselining '
              '(default is to de-dupe automatically). You can use "webkit-patch '
              'optimize-baselines" to optimize separately.'))
    platform_options = factory.platform_options(use_globs=True)
    results_directory_option = optparse.make_option(
        '--results-directory', help='Local results directory to use.')
    suffixes_option = optparse.make_option(
        '--suffixes', default=','.join(BASELINE_SUFFIX_LIST), action='store',
        help='Comma-separated-list of file types to rebaseline.')
    builder_option = optparse.make_option(
        '--builder', help='Builder to pull new baselines from.')
    test_option = optparse.make_option('--test', help='Test to rebaseline.')
    build_number_option = optparse.make_option(
        '--build-number', default=None, type='int',
        help='Optional build number; if not given, the latest build is used.')

    def __init__(self, options=None):
        super(AbstractRebaseliningCommand, self).__init__(options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST
        self.expectation_line_changes = ChangeSet()
        self._tool = None

    def _print_expectation_line_changes(self):
        print(json.dumps(self.expectation_line_changes.to_dict()))

    def _baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        return port.baseline_version_dir()

    def _test_root(self, test_name):
        return self._tool.filesystem.splitext(test_name)[0]

    def _file_name_for_actual_result(self, test_name, suffix):
        return "%s-actual.%s" % (self._test_root(test_name), suffix)

    def _file_name_for_expected_result(self, test_name, suffix):
        return "%s-expected.%s" % (self._test_root(test_name), suffix)


class ChangeSet(object):
    """A record of TestExpectation lines to remove.

    TODO(qyearsley): Remove this class, track list of lines to remove directly
    in an attribute of AbstractRebaseliningCommand.
    """
    def __init__(self, lines_to_remove=None):
        self.lines_to_remove = lines_to_remove or {}

    def remove_line(self, test, builder):
        if test not in self.lines_to_remove:
            self.lines_to_remove[test] = []
        self.lines_to_remove[test].append(builder)

    def to_dict(self):
        remove_lines = []
        for test in self.lines_to_remove:
            for builder in self.lines_to_remove[test]:
                remove_lines.append({'test': test, 'builder': builder})
        return {'remove-lines': remove_lines}

    @staticmethod
    def from_dict(change_dict):
        lines_to_remove = {}
        if 'remove-lines' in change_dict:
            for line_to_remove in change_dict['remove-lines']:
                test = line_to_remove['test']
                builder = line_to_remove['builder']
                if test not in lines_to_remove:
                    lines_to_remove[test] = []
                lines_to_remove[test].append(builder)
        return ChangeSet(lines_to_remove=lines_to_remove)

    def update(self, other):
        assert isinstance(other, ChangeSet)
        assert type(other.lines_to_remove) is dict
        for test in other.lines_to_remove:
            if test not in self.lines_to_remove:
                self.lines_to_remove[test] = []
            self.lines_to_remove[test].extend(other.lines_to_remove[test])


class CopyExistingBaselinesInternal(AbstractRebaseliningCommand):
    name = "copy-existing-baselines-internal"
    help_text = ("Copy existing baselines down one level in the baseline order to ensure "
                 "new baselines don't break existing passing platforms.")

    def __init__(self):
        super(CopyExistingBaselinesInternal, self).__init__(options=[
            self.results_directory_option,
            self.suffixes_option,
            self.builder_option,
            self.test_option,
        ])

    @memoized
    def _immediate_predecessors_in_fallback(self, path_to_rebaseline):
        port_names = self._tool.port_factory.all_port_names()
        immediate_predecessors = []
        for port_name in port_names:
            port = self._tool.port_factory.get(port_name)
            if not port.buildbot_archives_baselines():
                continue
            baseline_search_path = port.baseline_search_path()
            try:
                index = baseline_search_path.index(path_to_rebaseline)
                if index:
                    immediate_predecessors.append(self._tool.filesystem.basename(baseline_search_path[index - 1]))
            except ValueError:
                # baseline_search_path.index() throws a ValueError if the item isn't in the list.
                pass
        return immediate_predecessors

    def _port_for_primary_baseline(self, baseline):
        for port in [self._tool.port_factory.get(port_name) for port_name in self._tool.port_factory.all_port_names()]:
            if self._tool.filesystem.basename(port.baseline_version_dir()) == baseline:
                return port
        raise Exception("Failed to find port for primary baseline %s." % baseline)

    def _copy_existing_baseline(self, builder_name, test_name, suffix):
        baseline_directory = self._baseline_directory(builder_name)
        ports = [self._port_for_primary_baseline(baseline)
                 for baseline in self._immediate_predecessors_in_fallback(baseline_directory)]

        old_baselines = []
        new_baselines = []

        # Need to gather all the baseline paths before modifying the filesystem since
        # the modifications can affect the results of port.expected_filename.
        for port in ports:
            old_baseline = port.expected_filename(test_name, "." + suffix)
            if not self._tool.filesystem.exists(old_baseline):
                _log.debug("No existing baseline for %s.", test_name)
                continue

            new_baseline = self._tool.filesystem.join(
                port.baseline_version_dir(),
                self._file_name_for_expected_result(test_name, suffix))
            if self._tool.filesystem.exists(new_baseline):
                _log.debug("Existing baseline at %s, not copying over it.", new_baseline)
                continue

            generic_expectations = TestExpectations(port, tests=[test_name], include_overrides=False)
            full_expectations = TestExpectations(port, tests=[test_name], include_overrides=True)
            # TODO(qyearsley): Change Port.skips_test so that this can be simplified.
            if SKIP in full_expectations.get_expectations(test_name):
                _log.debug("%s is skipped (perhaps temporarily) on %s.", test_name, port.name())
                continue
            if port.skips_test(test_name, generic_expectations, full_expectations):
                _log.debug("%s is skipped on %s.", test_name, port.name())
                continue

            old_baselines.append(old_baseline)
            new_baselines.append(new_baseline)

        for i in range(len(old_baselines)):
            old_baseline = old_baselines[i]
            new_baseline = new_baselines[i]

            _log.debug("Copying baseline from %s to %s.", old_baseline, new_baseline)
            self._tool.filesystem.maybe_make_directory(self._tool.filesystem.dirname(new_baseline))
            self._tool.filesystem.copyfile(old_baseline, new_baseline)

    def execute(self, options, args, tool):
        self._tool = tool
        for suffix in options.suffixes.split(','):
            self._copy_existing_baseline(options.builder, options.test, suffix)


class RebaselineTest(AbstractRebaseliningCommand):
    name = "rebaseline-test-internal"
    help_text = "Rebaseline a single test from a buildbot. Only intended for use by other webkit-patch commands."

    def __init__(self):
        super(RebaselineTest, self).__init__(options=[
            self.results_directory_option,
            self.suffixes_option,
            self.builder_option,
            self.test_option,
            self.build_number_option,
        ])

    def _save_baseline(self, data, target_baseline):
        if not data:
            _log.debug("No baseline data to save.")
            return

        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)

    def _rebaseline_test(self, builder_name, test_name, suffix, results_url):
        baseline_directory = self._baseline_directory(builder_name)

        source_baseline = "%s/%s" % (results_url, self._file_name_for_actual_result(test_name, suffix))
        target_baseline = self._tool.filesystem.join(baseline_directory, self._file_name_for_expected_result(test_name, suffix))

        _log.debug("Retrieving source %s for target %s.", source_baseline, target_baseline)
        self._save_baseline(self._tool.web.get_binary(source_baseline, convert_404_to_None=True),
                            target_baseline)

    def _rebaseline_test_and_update_expectations(self, options):
        self._baseline_suffix_list = options.suffixes.split(',')

        port = self._tool.port_factory.get_from_builder_name(options.builder)
        if port.reference_files(options.test):
            if 'png' in self._baseline_suffix_list:
                _log.warning("Cannot rebaseline image result for reftest: %s", options.test)
                return
            assert self._baseline_suffix_list == ['txt']

        if options.results_directory:
            results_url = 'file://' + options.results_directory
        else:
            results_url = self._tool.buildbot.results_url(options.builder, build_number=options.build_number)

        for suffix in self._baseline_suffix_list:
            self._rebaseline_test(options.builder, options.test, suffix, results_url)
        self.expectation_line_changes.remove_line(test=options.test, builder=options.builder)

    def execute(self, options, args, tool):
        self._tool = tool
        self._rebaseline_test_and_update_expectations(options)
        self._print_expectation_line_changes()


class AbstractParallelRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline commands that do some tasks in parallel."""
    # Not overriding execute() - pylint: disable=abstract-method

    def __init__(self, options=None):
        super(AbstractParallelRebaselineCommand, self).__init__(options=options)

    def _release_builders(self):
        """Returns a list of builder names for continuous release builders.

        The release builders cycle much faster than the debug ones and cover all the platforms.
        """
        release_builders = []
        for builder_name in self._tool.builders.all_continuous_builder_names():
            port = self._tool.port_factory.get_from_builder_name(builder_name)
            if port.test_configuration().build_type == 'release':
                release_builders.append(builder_name)
        return release_builders

    def _run_webkit_patch(self, args, verbose):
        try:
            verbose_args = ['--verbose'] if verbose else []
            stderr = self._tool.executive.run_command([self._tool.path()] + verbose_args +
                                                      args, cwd=self._tool.scm().checkout_root, return_stderr=True)
            for line in stderr.splitlines():
                _log.warning(line)
        except ScriptError:
            traceback.print_exc(file=sys.stderr)

    def _builders_to_fetch_from(self, builders_to_check):
        """Returns the subset of builders that will cover all of the baseline
        search paths used in the input list.

        In particular, if the input list contains both Release and Debug
        versions of a configuration, we *only* return the Release version
        (since we don't save debug versions of baselines).

        Args:
            builders_to_check: List of builder names.
        """
        release_builders = set()
        debug_builders = set()
        builders_to_fallback_paths = {}
        for builder in builders_to_check:
            port = self._tool.port_factory.get_from_builder_name(builder)
            if port.test_configuration().build_type == 'release':
                release_builders.add(builder)
            else:
                debug_builders.add(builder)
        for builder in list(release_builders) + list(debug_builders):
            port = self._tool.port_factory.get_from_builder_name(builder)
            fallback_path = port.baseline_search_path()
            if fallback_path not in builders_to_fallback_paths.values():
                builders_to_fallback_paths[builder] = fallback_path
        return builders_to_fallback_paths.keys()

    @staticmethod
    def _builder_names(builds):
        # TODO(qyearsley): If test_prefix_list dicts are converted to instances
        # of some class, then this could be replaced with  a method on that class.
        return [b.builder_name for b in builds]

    def _rebaseline_commands(self, test_prefix_list, options):
        path_to_webkit_patch = self._tool.path()
        cwd = self._tool.scm().checkout_root
        copy_baseline_commands = []
        rebaseline_commands = []
        lines_to_remove = {}
        port = self._tool.port_factory.get()

        for test_prefix in test_prefix_list:
            for test in port.tests([test_prefix]):
                builders_to_fetch_from = self._builders_to_fetch_from(self._builder_names(test_prefix_list[test_prefix]))
                for build in sorted(test_prefix_list[test_prefix]):
                    builder, build_number = build.builder_name, build.build_number
                    if builder not in builders_to_fetch_from:
                        break
                    else:
                        actual_failures_suffixes = self._suffixes_for_actual_failures(
                            test, build, test_prefix_list[test_prefix][build])
                    if not actual_failures_suffixes:
                        # If we're not going to rebaseline the test because it's passing on this
                        # builder, we still want to remove the line from TestExpectations.
                        if test not in lines_to_remove:
                            lines_to_remove[test] = []
                        lines_to_remove[test].append(builder)
                        continue

                    suffixes = ','.join(actual_failures_suffixes)
                    args = ['--suffixes', suffixes, '--builder', builder, '--test', test]

                    if options.verbose:
                        args.append('--verbose')

                    copy_baseline_commands.append(
                        tuple([[self._tool.executable, path_to_webkit_patch, 'copy-existing-baselines-internal'] + args, cwd]))

                    if build_number:
                        args.extend(['--build-number', str(build_number)])
                    if options.results_directory:
                        args.extend(['--results-directory', options.results_directory])

                    rebaseline_commands.append(
                        tuple([[self._tool.executable, path_to_webkit_patch, 'rebaseline-test-internal'] + args, cwd]))
        return copy_baseline_commands, rebaseline_commands, lines_to_remove

    @staticmethod
    def _extract_expectation_line_changes(command_results):
        """Parses the JSON lines from sub-command output and returns the result as a ChangeSet."""
        change_set = ChangeSet()
        for _, stdout, _ in command_results:
            updated = False
            for line in filter(None, stdout.splitlines()):
                try:
                    parsed_line = json.loads(line)
                    change_set.update(ChangeSet.from_dict(parsed_line))
                    updated = True
                except ValueError:
                    _log.debug('"%s" is not a JSON object, ignoring', line)
            if not updated:
                # TODO(qyearsley): This probably should be an error. See http://crbug.com/649412.
                _log.debug('Could not add file based off output "%s"', stdout)
        return change_set

    def _optimize_baselines(self, test_prefix_list, verbose=False):
        optimize_commands = []
        for test in test_prefix_list:
            all_suffixes = set()
            builders_to_fetch_from = self._builders_to_fetch_from(self._builder_names(test_prefix_list[test]))
            for build in sorted(test_prefix_list[test]):
                if build.builder_name not in builders_to_fetch_from:
                    break
                all_suffixes.update(self._suffixes_for_actual_failures(test, build, test_prefix_list[test][build]))

            # No need to optimize baselines for a test with no failures.
            if not all_suffixes:
                continue

            # FIXME: We should propagate the platform options as well.
            cmd_line = ['--suffixes', ','.join(all_suffixes), test]
            if verbose:
                cmd_line.append('--verbose')

            path_to_webkit_patch = self._tool.path()
            cwd = self._tool.scm().checkout_root
            optimize_commands.append(tuple([[self._tool.executable, path_to_webkit_patch, 'optimize-baselines'] + cmd_line, cwd]))
        return optimize_commands

    def _update_expectations_files(self, lines_to_remove):
        # FIXME: This routine is way too expensive. We're creating O(n ports) TestExpectations objects.
        # This is slow and uses a lot of memory.
        tests = lines_to_remove.keys()
        to_remove = []

        # This is so we remove lines for builders that skip this test, e.g. Android skips most
        # tests and we don't want to leave stray [ Android ] lines in TestExpectations..
        # This is only necessary for "webkit-patch rebaseline" and for rebaselining expected
        # failures from garden-o-matic. rebaseline-expectations and auto-rebaseline will always
        # pass the exact set of ports to rebaseline.
        for port_name in self._tool.port_factory.all_port_names():
            port = self._tool.port_factory.get(port_name)
            generic_expectations = TestExpectations(port, tests=tests, include_overrides=False)
            full_expectations = TestExpectations(port, tests=tests, include_overrides=True)
            for test in tests:
                if port.skips_test(test, generic_expectations, full_expectations):
                    for test_configuration in port.all_test_configurations():
                        if test_configuration.version == port.test_configuration().version:
                            to_remove.append((test, test_configuration))

        for test in lines_to_remove:
            for builder in lines_to_remove[test]:
                port = self._tool.port_factory.get_from_builder_name(builder)
                for test_configuration in port.all_test_configurations():
                    if test_configuration.version == port.test_configuration().version:
                        to_remove.append((test, test_configuration))

        port = self._tool.port_factory.get()
        expectations = TestExpectations(port, include_overrides=False)
        expectations_string = expectations.remove_configurations(to_remove)
        path = port.path_to_generic_test_expectations_file()
        self._tool.filesystem.write_text_file(path, expectations_string)

    def _run_in_parallel(self, commands):
        if not commands:
            return {}

        command_results = self._tool.executive.run_in_parallel(commands)
        for _, _, stderr in command_results:
            if stderr:
                _log.error(stderr)

        change_set = self._extract_expectation_line_changes(command_results)

        return change_set.lines_to_remove

    def rebaseline(self, options, test_prefix_list):
        """Downloads new baselines in parallel, then updates expectations files
        and optimizes baselines.

        Args:
            options: An object with the options passed to the current command.
            test_prefix_list: A map of test names to Build objects to file suffixes
                for new baselines. For example:
                {
                    "some/test.html": {Build("builder-1", 412): ["txt"], Build("builder-2", 100): ["txt"]},
                    "some/other.html": {Build("builder-1", 412): ["txt"]}
                }
                This would mean that new text baselines should be downloaded for
                "some/test.html" on both builder-1 (build 412) and builder-2
                (build 100), and new text baselines should be downloaded for
                "some/other.html" but only from builder-1.
                TODO(qyearsley): Replace test_prefix_list everywhere with some
                sort of class that contains the same data.
        """
        if self._tool.scm().has_working_directory_changes(pathspec=self._layout_tests_dir()):
            _log.error('There are uncommitted changes in the layout tests directory; aborting.')
            return

        for test, builds_to_check in sorted(test_prefix_list.items()):
            _log.info("Rebaselining %s", test)
            for build, suffixes in sorted(builds_to_check.items()):
                _log.debug("  %s: %s", build, ",".join(suffixes))

        copy_baseline_commands, rebaseline_commands, extra_lines_to_remove = self._rebaseline_commands(
            test_prefix_list, options)
        lines_to_remove = {}

        self._run_in_parallel(copy_baseline_commands)
        lines_to_remove = self._run_in_parallel(rebaseline_commands)

        for test in extra_lines_to_remove:
            if test in lines_to_remove:
                lines_to_remove[test] = lines_to_remove[test] + extra_lines_to_remove[test]
            else:
                lines_to_remove[test] = extra_lines_to_remove[test]

        if lines_to_remove:
            self._update_expectations_files(lines_to_remove)

        if options.optimize:
            # TODO(wkorman): Consider changing temporary branch to base off of HEAD rather than
            # origin/master to ensure we run baseline optimization processes with the same code as
            # auto-rebaseline itself.
            self._run_in_parallel(self._optimize_baselines(test_prefix_list, options.verbose))

        self._remove_all_pass_testharness_baselines(test_prefix_list)

        self._tool.scm().add_all(pathspec=self._layout_tests_dir())

    def _remove_all_pass_testharness_baselines(self, test_prefix_list):
        """Removes all of the all-PASS baselines for the given builders and tests.

        In general, for testharness.js tests, the absence of a baseline
        indicates that the test is expected to pass. When rebaselining,
        new all-PASS baselines may be downloaded, but they should not be kept.
        """
        filesystem = self._tool.filesystem
        baseline_paths = self._all_baseline_paths(test_prefix_list)
        for path in baseline_paths:
            if not (filesystem.exists(path) and
                    filesystem.splitext(path)[1] == '.txt'):
                continue
            contents = filesystem.read_text_file(path)
            if is_all_pass_testharness_result(contents):
                _log.info('Removing all-PASS testharness baseline: %s', path)
                filesystem.remove(path)

    def _all_baseline_paths(self, test_prefix_list):
        """Return file paths for all baselines for the given tests and builders.

        Args:
            test_prefix_list: A dict mapping test prefixes, which could be
                directories or full test paths, to builds to baseline suffixes.
                TODO(qyearsley): If a class is added to replace test_prefix_list,
                then this can be made a method on that class.

        Returns:
            A list of absolute paths to possible baseline files,
            which may or may not exist on the local filesystem.
        """
        filesystem = self._tool.filesystem
        baseline_paths = []
        port = self._tool.port_factory.get()

        for test_prefix in test_prefix_list:
            tests = port.tests([test_prefix])
            all_suffixes = set()

            for build, suffixes in test_prefix_list[test_prefix].iteritems():
                all_suffixes.update(suffixes)
                port_baseline_dir = self._baseline_directory(build.builder_name)
                baseline_paths.extend([
                    filesystem.join(port_baseline_dir, self._file_name_for_expected_result(test, suffix))
                    for test in tests for suffix in suffixes
                ])

            baseline_paths.extend([
                filesystem.join(self._layout_tests_dir(), self._file_name_for_expected_result(test, suffix))
                for test in tests for suffix in all_suffixes
            ])

        return sorted(baseline_paths)

    def _layout_tests_dir(self):
        return self._tool.port_factory.get().layout_tests_dir()

    def _suffixes_for_actual_failures(self, test, build, existing_suffixes):
        """Gets the baseline suffixes for actual mismatch failures in some results.

        Args:
            test: A full test path string.
            build: A Build object.
            existing_suffixes: A collection of all suffixes to consider.

        Returns:
            A set of file suffix strings.
        """
        results = self._tool.buildbot.fetch_results(build)
        if not results:
            _log.debug('No results found for build %s', build)
            return set()
        test_result = results.result_for_test(test)
        if not test_result:
            _log.debug('No test result for test %s in build %s', test, build)
            return set()
        return set(existing_suffixes) & TestExpectations.suffixes_for_test_result(test_result)


class RebaselineJson(AbstractParallelRebaselineCommand):
    name = "rebaseline-json"
    help_text = "Rebaseline based off JSON passed to stdin. Intended to only be called from other scripts."

    def __init__(self,):
        super(RebaselineJson, self).__init__(options=[
            self.no_optimize_option,
            self.results_directory_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool
        self.rebaseline(options, json.loads(sys.stdin.read()))


class RebaselineExpectations(AbstractParallelRebaselineCommand):
    name = "rebaseline-expectations"
    help_text = "Rebaselines the tests indicated in TestExpectations."
    show_in_main_help = True

    def __init__(self):
        super(RebaselineExpectations, self).__init__(options=[
            self.no_optimize_option,
        ] + self.platform_options)
        self._test_prefix_list = None

    @staticmethod
    def _tests_to_rebaseline(port):
        tests_to_rebaseline = {}
        for path, value in port.expectations_dict().items():
            expectations = TestExpectations(port, include_overrides=False, expectations_dict={path: value})
            for test in expectations.get_rebaselining_failures():
                suffixes = TestExpectations.suffixes_for_expectations(expectations.get_expectations(test))
                tests_to_rebaseline[test] = suffixes or BASELINE_SUFFIX_LIST
        return tests_to_rebaseline

    def _add_tests_to_rebaseline(self, port_name):
        builder_name = self._tool.builders.builder_name_for_port_name(port_name)
        if not builder_name:
            return
        tests = self._tests_to_rebaseline(self._tool.port_factory.get(port_name)).items()

        if tests:
            _log.info("Retrieving results for %s from %s.", port_name, builder_name)

        for test_name, suffixes in tests:
            _log.info("    %s (%s)", test_name, ','.join(suffixes))
            if test_name not in self._test_prefix_list:
                self._test_prefix_list[test_name] = {}
            self._test_prefix_list[test_name][Build(builder_name)] = suffixes

    def execute(self, options, args, tool):
        self._tool = tool
        options.results_directory = None
        self._test_prefix_list = {}
        port_names = tool.port_factory.all_port_names(options.platform)
        for port_name in port_names:
            self._add_tests_to_rebaseline(port_name)
        if not self._test_prefix_list:
            _log.warning("Did not find any tests marked Rebaseline.")
            return

        self.rebaseline(options, self._test_prefix_list)


class Rebaseline(AbstractParallelRebaselineCommand):
    name = "rebaseline"
    help_text = "Rebaseline tests with results from the build bots."
    show_in_main_help = True
    argument_names = "[TEST_NAMES]"

    def __init__(self):
        super(Rebaseline, self).__init__(options=[
            self.no_optimize_option,
            # FIXME: should we support the platform options in addition to (or instead of) --builders?
            self.suffixes_option,
            self.results_directory_option,
            optparse.make_option("--builders", default=None, action="append",
                                 help=("Comma-separated-list of builders to pull new baselines from "
                                       "(can also be provided multiple times).")),
        ])

    def _builders_to_pull_from(self):
        return self._tool.user.prompt_with_list(
            "Which builder to pull results from:", self._release_builders(), can_choose_multiple=True)

    def execute(self, options, args, tool):
        self._tool = tool
        if not args:
            _log.error("Must list tests to rebaseline.")
            return

        if options.builders:
            builders_to_check = []
            for builder_names in options.builders:
                builders_to_check += builder_names.split(",")
        else:
            builders_to_check = self._builders_to_pull_from()

        test_prefix_list = {}
        suffixes_to_update = options.suffixes.split(",")

        for builder in builders_to_check:
            for test in args:
                if test not in test_prefix_list:
                    test_prefix_list[test] = {}
                build = Build(builder)
                test_prefix_list[test][build] = suffixes_to_update

        if options.verbose:
            _log.debug("rebaseline-json: " + str(test_prefix_list))

        self.rebaseline(options, test_prefix_list)
