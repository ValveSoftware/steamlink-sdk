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
import re
import sys
import time
import traceback
import urllib2

from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.controllers.test_result_writer import baseline_name
from webkitpy.layout_tests.models.test_expectations import TestExpectations, BASELINE_SUFFIX_LIST, SKIP
from webkitpy.layout_tests.port import factory
from webkitpy.tool.commands.command import Command


_log = logging.getLogger(__name__)


class AbstractRebaseliningCommand(Command):
    """Base class for rebaseline-related commands."""
    # Not overriding execute() - pylint: disable=abstract-method

    no_optimize_option = optparse.make_option('--no-optimize', dest='optimize', action='store_false', default=True,
                                              help=('Do not optimize/de-dup the expectations after rebaselining (default is to de-dup automatically). '
                                                    'You can use "webkit-patch optimize-baselines" to optimize separately.'))

    platform_options = factory.platform_options(use_globs=True)

    results_directory_option = optparse.make_option("--results-directory", help="Local results directory to use.")

    suffixes_option = optparse.make_option("--suffixes", default=','.join(BASELINE_SUFFIX_LIST), action="store",
                                           help="Comma-separated-list of file types to rebaseline.")

    def __init__(self, options=None):
        super(AbstractRebaseliningCommand, self).__init__(options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST
        self._scm_changes = {'add': [], 'delete': [], 'remove-lines': []}

    def _results_url(self, builder_name, master_name, build_number=None):
        builder = self._tool.buildbot.builder_with_name(builder_name, master_name)
        if build_number:
            build = builder.build(build_number)
            return build.results_url()
        return builder.latest_layout_test_results_url()

    def _add_to_scm_later(self, path):
        self._scm_changes['add'].append(path)

    def _delete_from_scm_later(self, path):
        self._scm_changes['delete'].append(path)

    def _print_scm_changes(self):
        print(json.dumps(self._scm_changes))


class BaseInternalRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline-related commands that are intended to be used by other commands."""
    # Not overriding execute() - pylint: disable=abstract-method

    def __init__(self):
        super(BaseInternalRebaselineCommand, self).__init__(options=[
            self.results_directory_option,
            self.suffixes_option,
            optparse.make_option("--builder", help="Builder to pull new baselines from."),
            optparse.make_option("--test", help="Test to rebaseline."),
            optparse.make_option("--build-number", default=None, type="int",
                                 help="Optional build number; if not given, the latest build is used."),
            optparse.make_option("--master-name", default='chromium.webkit', type="str",
                                 help="Optional master name; if not given, a default master will be used."),
        ])

    def _baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        return port.baseline_version_dir()

    def _test_root(self, test_name):
        return self._tool.filesystem.splitext(test_name)[0]

    def _file_name_for_actual_result(self, test_name, suffix):
        return "%s-actual.%s" % (self._test_root(test_name), suffix)

    def _file_name_for_expected_result(self, test_name, suffix):
        return "%s-expected.%s" % (self._test_root(test_name), suffix)


class CopyExistingBaselinesInternal(BaseInternalRebaselineCommand):
    name = "copy-existing-baselines-internal"
    help_text = "Copy existing baselines down one level in the baseline order to ensure new baselines don't break existing passing platforms."

    @memoized
    def _immediate_predecessors_in_fallback(self, path_to_rebaseline):
        port_names = self._tool.port_factory.all_port_names()
        immediate_predecessors_in_fallback = []
        for port_name in port_names:
            port = self._tool.port_factory.get(port_name)
            if not port.buildbot_archives_baselines():
                continue
            baseline_search_path = port.baseline_search_path()
            try:
                index = baseline_search_path.index(path_to_rebaseline)
                if index:
                    immediate_predecessors_in_fallback.append(self._tool.filesystem.basename(baseline_search_path[index - 1]))
            except ValueError:
                # baseline_search_path.index() throws a ValueError if the item isn't in the list.
                pass
        return immediate_predecessors_in_fallback

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
                _log.debug("No existing baseline for %s." % test_name)
                continue

            new_baseline = self._tool.filesystem.join(port.baseline_path(), self._file_name_for_expected_result(test_name, suffix))
            if self._tool.filesystem.exists(new_baseline):
                _log.debug("Existing baseline at %s, not copying over it." % new_baseline)
                continue

            expectations = TestExpectations(port, [test_name])
            if SKIP in expectations.get_expectations(test_name):
                _log.debug("%s is skipped on %s." % (test_name, port.name()))
                continue

            old_baselines.append(old_baseline)
            new_baselines.append(new_baseline)

        for i in range(len(old_baselines)):
            old_baseline = old_baselines[i]
            new_baseline = new_baselines[i]

            _log.debug("Copying baseline from %s to %s." % (old_baseline, new_baseline))
            self._tool.filesystem.maybe_make_directory(self._tool.filesystem.dirname(new_baseline))
            self._tool.filesystem.copyfile(old_baseline, new_baseline)
            if not self._tool.scm().exists(new_baseline):
                self._add_to_scm_later(new_baseline)

    def execute(self, options, args, tool):
        for suffix in options.suffixes.split(','):
            self._copy_existing_baseline(options.builder, options.test, suffix)
        self._print_scm_changes()


class RebaselineTest(BaseInternalRebaselineCommand):
    name = "rebaseline-test-internal"
    help_text = "Rebaseline a single test from a buildbot. Only intended for use by other webkit-patch commands."

    def _save_baseline(self, data, target_baseline):
        if not data:
            _log.debug("No baseline data to save.")
            return

        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)
        if not self._tool.scm().exists(target_baseline):
            self._add_to_scm_later(target_baseline)

    def _rebaseline_test(self, builder_name, test_name, suffix, results_url):
        baseline_directory = self._baseline_directory(builder_name)

        source_baseline = "%s/%s" % (results_url, self._file_name_for_actual_result(test_name, suffix))
        target_baseline = self._tool.filesystem.join(baseline_directory, self._file_name_for_expected_result(test_name, suffix))

        _log.debug("Retrieving source %s for target %s." % (source_baseline, target_baseline))
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
            results_url = self._results_url(options.builder, options.master_name, build_number=options.build_number)

        for suffix in self._baseline_suffix_list:
            self._rebaseline_test(options.builder, options.test, suffix, results_url)
        self._scm_changes['remove-lines'].append({'builder': options.builder, 'test': options.test})

    def execute(self, options, args, tool):
        self._rebaseline_test_and_update_expectations(options)
        self._print_scm_changes()


class OptimizeBaselines(AbstractRebaseliningCommand):
    name = "optimize-baselines"
    help_text = "Reshuffles the baselines for the given tests to use as litte space on disk as possible."
    show_in_main_help = True
    argument_names = "TEST_NAMES"

    def __init__(self):
        super(OptimizeBaselines, self).__init__(options=[
            self.suffixes_option,
            optparse.make_option('--no-modify-scm', action='store_true', default=False,
                                 help='Dump SCM commands as JSON instead of actually committing changes.'),
        ] + self.platform_options)

    def _optimize_baseline(self, optimizer, test_name):
        files_to_delete = []
        files_to_add = []
        for suffix in self._baseline_suffix_list:
            name = baseline_name(self._tool.filesystem, test_name, suffix)
            succeeded, more_files_to_delete, more_files_to_add = optimizer.optimize(name)
            if not succeeded:
                _log.error("Heuristics failed to optimize %s", name)
            files_to_delete.extend(more_files_to_delete)
            files_to_add.extend(more_files_to_add)
        return files_to_delete, files_to_add

    def execute(self, options, args, tool):
        self._baseline_suffix_list = options.suffixes.split(',')
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return
        port = tool.port_factory.get(port_names[0])
        optimizer = BaselineOptimizer(tool, port, port_names, skip_scm_commands=options.no_modify_scm)
        tests = port.tests(args)
        for test_name in tests:
            files_to_delete, files_to_add = self._optimize_baseline(optimizer, test_name)
            for path in files_to_delete:
                self._delete_from_scm_later(path)
            for path in files_to_add:
                self._add_to_scm_later(path)
        self._print_scm_changes()


class AbstractParallelRebaselineCommand(AbstractRebaseliningCommand):
    """Base class for rebaseline commands that do some tasks in parallel."""
    # Not overriding execute() - pylint: disable=abstract-method

    def __init__(self, options=None):
        super(AbstractParallelRebaselineCommand, self).__init__(options=options)
        self._builder_data = {}

    def builder_data(self):
        if not self._builder_data:
            for builder_name in self._release_builders():
                builder = self._tool.buildbot.builder_with_name(builder_name)
                builder_results = builder.latest_layout_test_results()
                if builder_results:
                    self._builder_data[builder_name] = builder_results
                else:
                    raise Exception("No result for builder %s." % builder_name)
        return self._builder_data

    # The release builders cycle much faster than the debug ones and cover all the platforms.
    def _release_builders(self):
        release_builders = []
        for builder_name in self._tool.builders.all_continuous_builder_names():
            if 'ASAN' in builder_name:
                continue
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
        """Returns the subset of builders that will cover all of the baseline search paths
        used in the input list.

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

    def _rebaseline_commands(self, test_prefix_list, options):
        path_to_webkit_patch = self._tool.path()
        cwd = self._tool.scm().checkout_root
        copy_baseline_commands = []
        rebaseline_commands = []
        lines_to_remove = {}
        port = self._tool.port_factory.get()

        for test_prefix in test_prefix_list:
            for test in port.tests([test_prefix]):
                for builder in self._builders_to_fetch_from(test_prefix_list[test_prefix]):
                    actual_failures_suffixes = self._suffixes_for_actual_failures(
                        test, builder, test_prefix_list[test_prefix][builder])
                    if not actual_failures_suffixes:
                        # If we're not going to rebaseline the test because it's passing on this
                        # builder, we still want to remove the line from TestExpectations.
                        if test not in lines_to_remove:
                            lines_to_remove[test] = []
                        lines_to_remove[test].append(builder)
                        continue

                    suffixes = ','.join(actual_failures_suffixes)
                    cmd_line = ['--suffixes', suffixes, '--builder', builder, '--test', test]
                    if options.results_directory:
                        cmd_line.extend(['--results-directory', options.results_directory])
                    if options.verbose:
                        cmd_line.append('--verbose')
                    copy_baseline_commands.append(
                        tuple([[self._tool.executable, path_to_webkit_patch, 'copy-existing-baselines-internal'] + cmd_line, cwd]))
                    rebaseline_commands.append(
                        tuple([[self._tool.executable, path_to_webkit_patch, 'rebaseline-test-internal'] + cmd_line, cwd]))
        return copy_baseline_commands, rebaseline_commands, lines_to_remove

    def _serial_commands(self, command_results):
        files_to_add = set()
        files_to_delete = set()
        lines_to_remove = {}
        for output in [result[1].split('\n') for result in command_results]:
            file_added = False
            for line in output:
                try:
                    if line:
                        parsed_line = json.loads(line)
                        if 'add' in parsed_line:
                            files_to_add.update(parsed_line['add'])
                        if 'delete' in parsed_line:
                            files_to_delete.update(parsed_line['delete'])
                        if 'remove-lines' in parsed_line:
                            for line_to_remove in parsed_line['remove-lines']:
                                test = line_to_remove['test']
                                builder = line_to_remove['builder']
                                if test not in lines_to_remove:
                                    lines_to_remove[test] = []
                                lines_to_remove[test].append(builder)
                        file_added = True
                except ValueError:
                    _log.debug('"%s" is not a JSON object, ignoring' % line)

            if not file_added:
                _log.debug('Could not add file based off output "%s"' % output)

        return list(files_to_add), list(files_to_delete), lines_to_remove

    def _optimize_baselines(self, test_prefix_list, verbose=False):
        optimize_commands = []
        for test in test_prefix_list:
            all_suffixes = set()
            for builder in self._builders_to_fetch_from(test_prefix_list[test]):
                all_suffixes.update(self._suffixes_for_actual_failures(test, builder, test_prefix_list[test][builder]))

            # No need to optimize baselines for a test with no failures.
            if not all_suffixes:
                continue

            # FIXME: We should propagate the platform options as well.
            cmd_line = ['--no-modify-scm', '--suffixes', ','.join(all_suffixes), test]
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
                if self._port_skips_test(port, test, generic_expectations, full_expectations):
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
        expectationsString = expectations.remove_configurations(to_remove)
        path = port.path_to_generic_test_expectations_file()
        self._tool.filesystem.write_text_file(path, expectationsString)

    def _port_skips_test(self, port, test, generic_expectations, full_expectations):
        fs = port.host.filesystem
        if port.default_smoke_test_only():
            smoke_test_filename = fs.join(port.layout_tests_dir(), 'SmokeTests')
            if fs.exists(smoke_test_filename) and test not in fs.read_text_file(smoke_test_filename):
                return True

        return (SKIP in full_expectations.get_expectations(test) and
                SKIP not in generic_expectations.get_expectations(test))

    def _run_in_parallel_and_update_scm(self, commands):
        if not commands:
            return {}

        command_results = self._tool.executive.run_in_parallel(commands)
        log_output = '\n'.join(result[2] for result in command_results).replace('\n\n', '\n')
        for line in log_output.split('\n'):
            if line:
                _log.error(line)

        files_to_add, files_to_delete, lines_to_remove = self._serial_commands(command_results)
        if files_to_delete:
            self._tool.scm().delete_list(files_to_delete)
        if files_to_add:
            self._tool.scm().add_list(files_to_add)
        return lines_to_remove

    def _rebaseline(self, options, test_prefix_list):
        for test, builders_to_check in sorted(test_prefix_list.items()):
            _log.info("Rebaselining %s" % test)
            for builder, suffixes in sorted(builders_to_check.items()):
                _log.debug("  %s: %s" % (builder, ",".join(suffixes)))

        copy_baseline_commands, rebaseline_commands, extra_lines_to_remove = self._rebaseline_commands(test_prefix_list, options)
        lines_to_remove = {}

        self._run_in_parallel_and_update_scm(copy_baseline_commands)
        lines_to_remove = self._run_in_parallel_and_update_scm(rebaseline_commands)

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
            self._run_in_parallel_and_update_scm(self._optimize_baselines(test_prefix_list, options.verbose))

    def _suffixes_for_actual_failures(self, test, builder_name, existing_suffixes):
        if builder_name not in self.builder_data():
            return set()
        test_result = self.builder_data()[builder_name].result_for_test(test)
        if not test_result:
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
        self._rebaseline(options, json.loads(sys.stdin.read()))


class RebaselineExpectations(AbstractParallelRebaselineCommand):
    name = "rebaseline-expectations"
    help_text = "Rebaselines the tests indicated in TestExpectations."
    show_in_main_help = True

    def __init__(self):
        super(RebaselineExpectations, self).__init__(options=[
            self.no_optimize_option,
        ] + self.platform_options)
        self._test_prefix_list = None

    def _tests_to_rebaseline(self, port):
        tests_to_rebaseline = {}
        for path, value in port.expectations_dict().items():
            expectations = TestExpectations(port, include_overrides=False, expectations_dict={path: value})
            for test in expectations.get_rebaselining_failures():
                suffixes = TestExpectations.suffixes_for_expectations(expectations.get_expectations(test))
                tests_to_rebaseline[test] = suffixes or BASELINE_SUFFIX_LIST
        return tests_to_rebaseline

    def _add_tests_to_rebaseline_for_port(self, port_name):
        builder_name = self._tool.builders.builder_name_for_port_name(port_name)
        if not builder_name:
            return
        tests = self._tests_to_rebaseline(self._tool.port_factory.get(port_name)).items()

        if tests:
            _log.info("Retrieving results for %s from %s." % (port_name, builder_name))

        for test_name, suffixes in tests:
            _log.info("    %s (%s)" % (test_name, ','.join(suffixes)))
            if test_name not in self._test_prefix_list:
                self._test_prefix_list[test_name] = {}
            self._test_prefix_list[test_name][builder_name] = suffixes

    def execute(self, options, args, tool):
        options.results_directory = None
        self._test_prefix_list = {}
        port_names = tool.port_factory.all_port_names(options.platform)
        for port_name in port_names:
            self._add_tests_to_rebaseline_for_port(port_name)
        if not self._test_prefix_list:
            _log.warning("Did not find any tests marked Rebaseline.")
            return

        self._rebaseline(options, self._test_prefix_list)


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
                                 help="Comma-separated-list of builders to pull new baselines from (can also be provided multiple times)."),
        ])

    def _builders_to_pull_from(self):
        chosen_names = self._tool.user.prompt_with_list(
            "Which builder to pull results from:", self._release_builders(), can_choose_multiple=True)
        return [self._builder_with_name(name) for name in chosen_names]

    def _builder_with_name(self, name):
        return self._tool.buildbot.builder_with_name(name)

    def execute(self, options, args, tool):
        if not args:
            _log.error("Must list tests to rebaseline.")
            return

        if options.builders:
            builders_to_check = []
            for builder_names in options.builders:
                builders_to_check += [self._builder_with_name(name) for name in builder_names.split(",")]
        else:
            builders_to_check = self._builders_to_pull_from()

        test_prefix_list = {}
        suffixes_to_update = options.suffixes.split(",")

        for builder in builders_to_check:
            for test in args:
                if test not in test_prefix_list:
                    test_prefix_list[test] = {}
                test_prefix_list[test][builder.name()] = suffixes_to_update

        if options.verbose:
            _log.debug("rebaseline-json: " + str(test_prefix_list))

        self._rebaseline(options, test_prefix_list)


class AutoRebaseline(AbstractParallelRebaselineCommand):
    name = "auto-rebaseline"
    help_text = "Rebaselines any NeedsRebaseline lines in TestExpectations that have cycled through all the bots."
    AUTO_REBASELINE_BRANCH_NAME = "auto-rebaseline-temporary-branch"
    AUTO_REBASELINE_ALT_BRANCH_NAME = "auto-rebaseline-alt-temporary-branch"

    # Rietveld uploader stinks. Limit the number of rebaselines in a given patch to keep upload from failing.
    # FIXME: http://crbug.com/263676 Obviously we should fix the uploader here.
    MAX_LINES_TO_REBASELINE = 200

    SECONDS_BEFORE_GIVING_UP = 300

    def __init__(self):
        super(AutoRebaseline, self).__init__(options=[
            # FIXME: Remove this option.
            self.no_optimize_option,
            # FIXME: Remove this option.
            self.results_directory_option,
            optparse.make_option("--auth-refresh-token-json", help="Rietveld auth refresh JSON token."),
            optparse.make_option("--dry-run", action='store_true', default=False,
                                 help='Run without creating a temporary branch, committing locally, or uploading/landing '
                                 'changes to the remote repository.')
        ])
        self._blame_regex = re.compile(r"""
                ^(\S*)      # Commit hash
                [^(]* \(    # Whitespace and open parenthesis
                <           # Email address is surrounded by <>
                (
                    [^@]+   # Username preceding @
                    @
                    [^@>]+  # Domain terminated by @ or >, some lines have an additional @ fragment after the email.
                )
                .*?([^ ]*)  # Test file name
                \ \[        # Single space followed by opening [ for expectation specifier
                [^[]*$      # Prevents matching previous [ for version specifiers instead of expectation specifiers
            """, re.VERBOSE)

    def bot_revision_data(self, scm):
        revisions = []
        for result in self.builder_data().values():
            if result.run_was_interrupted():
                _log.error("Can't rebaseline because the latest run on %s exited early." % result.builder_name())
                return []
            revisions.append({
                "builder": result.builder_name(),
                "revision": result.chromium_revision(scm),
            })
        return revisions

    def _strip_comments(self, line):
        comment_index = line.find("#")
        if comment_index == -1:
            comment_index = len(line)
        return re.sub(r"\s+", " ", line[:comment_index].strip())

    def tests_to_rebaseline(self, tool, min_revision, print_revisions):
        port = tool.port_factory.get()
        expectations_file_path = port.path_to_generic_test_expectations_file()

        tests = set()
        revision = None
        commit = None
        author = None
        bugs = set()
        has_any_needs_rebaseline_lines = False

        for line in tool.scm().blame(expectations_file_path).split("\n"):
            line = self._strip_comments(line)
            if "NeedsRebaseline" not in line:
                continue

            has_any_needs_rebaseline_lines = True

            parsed_line = self._blame_regex.match(line)
            if not parsed_line:
                # Deal gracefully with inability to parse blame info for a line in TestExpectations.
                # Parsing could fail if for example during local debugging the developer modifies
                # TestExpectations and does not commit.
                _log.info("Couldn't find blame info for expectations line, skipping [line=%s]." % line)
                continue

            commit_hash = parsed_line.group(1)
            commit_position = tool.scm().commit_position_from_git_commit(commit_hash)

            test = parsed_line.group(3)
            if print_revisions:
                _log.info("%s is waiting for r%s" % (test, commit_position))

            if not commit_position or commit_position > min_revision:
                continue

            if revision and commit_position != revision:
                continue

            if not revision:
                revision = commit_position
                commit = commit_hash
                author = parsed_line.group(2)

            bugs.update(re.findall(r"crbug\.com\/(\d+)", line))
            tests.add(test)

            if len(tests) >= self.MAX_LINES_TO_REBASELINE:
                _log.info("Too many tests to rebaseline in one patch. Doing the first %d." % self.MAX_LINES_TO_REBASELINE)
                break

        return tests, revision, commit, author, bugs, has_any_needs_rebaseline_lines

    def link_to_patch(self, commit):
        return "https://chromium.googlesource.com/chromium/src/+/" + commit

    def commit_message(self, author, revision, commit, bugs):
        bug_string = ""
        if bugs:
            bug_string = "BUG=%s\n" % ",".join(bugs)

        return """Auto-rebaseline for r%s

%s

%sTBR=%s
""" % (revision, self.link_to_patch(commit), bug_string, author)

    def get_test_prefix_list(self, tests):
        test_prefix_list = {}
        lines_to_remove = {}

        for builder_name in self._release_builders():
            port_name = self._tool.builders.port_name_for_builder_name(builder_name)
            port = self._tool.port_factory.get(port_name)
            expectations = TestExpectations(port, include_overrides=True)
            for test in expectations.get_needs_rebaseline_failures():
                if test not in tests:
                    continue

                if test not in test_prefix_list:
                    lines_to_remove[test] = []
                    test_prefix_list[test] = {}
                lines_to_remove[test].append(builder_name)
                test_prefix_list[test][builder_name] = BASELINE_SUFFIX_LIST

        return test_prefix_list, lines_to_remove

    def _run_git_cl_command(self, options, command):
        subprocess_command = ['git', 'cl'] + command
        if options.verbose:
            subprocess_command.append('--verbose')
        if options.auth_refresh_token_json:
            subprocess_command.append('--auth-refresh-token-json')
            subprocess_command.append(options.auth_refresh_token_json)

        process = self._tool.executive.popen(subprocess_command, stdout=self._tool.executive.PIPE,
                                             stderr=self._tool.executive.STDOUT)
        last_output_time = time.time()

        # git cl sometimes completely hangs. Bail if we haven't gotten any output to stdout/stderr in a while.
        while process.poll() is None and time.time() < last_output_time + self.SECONDS_BEFORE_GIVING_UP:
            # FIXME: This doesn't make any sense. readline blocks, so all this code to
            # try and bail is useless. Instead, we should do the readline calls on a
            # subthread. Then the rest of this code would make sense.
            out = process.stdout.readline().rstrip('\n')
            if out:
                last_output_time = time.time()
                _log.info(out)

        if process.poll() is None:
            _log.error('Command hung: %s' % subprocess_command)
            return False
        return True

    # FIXME: Move this somewhere more general.
    def tree_status(self):
        blink_tree_status_url = "http://chromium-status.appspot.com/status"
        status = urllib2.urlopen(blink_tree_status_url).read().lower()
        if 'closed' in status or status == "0":
            return 'closed'
        elif 'open' in status or status == "1":
            return 'open'
        return 'unknown'

    def execute(self, options, args, tool):
        if tool.scm().executable_name == "svn":
            _log.error("Auto rebaseline only works with a git checkout.")
            return

        if not options.dry_run and tool.scm().has_working_directory_changes():
            _log.error("Cannot proceed with working directory changes. Clean working directory first.")
            return

        revision_data = self.bot_revision_data(tool.scm())
        if not revision_data:
            return

        min_revision = int(min([item["revision"] for item in revision_data]))
        tests, revision, commit, author, bugs, _ = self.tests_to_rebaseline(
            tool, min_revision, print_revisions=options.verbose)

        if options.verbose:
            _log.info("Min revision across all bots is %s." % min_revision)
            for item in revision_data:
                _log.info("%s: r%s" % (item["builder"], item["revision"]))

        if not tests:
            _log.debug('No tests to rebaseline.')
            return

        if self.tree_status() == 'closed':
            _log.info('Cannot proceed. Tree is closed.')
            return

        _log.info('Rebaselining %s for r%s by %s.' % (list(tests), revision, author))

        test_prefix_list, _ = self.get_test_prefix_list(tests)

        did_switch_branches = False
        did_finish = False
        old_branch_name_or_ref = ''
        rebaseline_branch_name = self.AUTO_REBASELINE_BRANCH_NAME
        try:
            # Save the current branch name and check out a clean branch for the patch.
            old_branch_name_or_ref = tool.scm().current_branch_or_ref()
            if old_branch_name_or_ref == self.AUTO_REBASELINE_BRANCH_NAME:
                rebaseline_branch_name = self.AUTO_REBASELINE_ALT_BRANCH_NAME
            if not options.dry_run:
                tool.scm().delete_branch(rebaseline_branch_name)
                tool.scm().create_clean_branch(rebaseline_branch_name)
                did_switch_branches = True

            if test_prefix_list:
                self._rebaseline(options, test_prefix_list)

            if options.dry_run:
                return

            tool.scm().commit_locally_with_message(
                self.commit_message(author, revision, commit, bugs))

            # FIXME: It would be nice if we could dcommit the patch without uploading, but still
            # go through all the precommit hooks. For rebaselines with lots of files, uploading
            # takes a long time and sometimes fails, but we don't want to commit if, e.g. the
            # tree is closed.
            did_finish = self._run_git_cl_command(options, ['upload', '-f'])

            if did_finish:
                # Uploading can take a very long time. Do another pull to make sure TestExpectations is up to date,
                # so the dcommit can go through.
                # FIXME: Log the pull and dcommit stdout/stderr to the log-server.
                tool.executive.run_command(['git', 'pull'])

                self._run_git_cl_command(options, ['land', '-f', '-v'])
        except Exception:
            traceback.print_exc(file=sys.stderr)
        finally:
            if did_switch_branches:
                if did_finish:
                    # Close the issue if dcommit failed.
                    issue_already_closed = tool.executive.run_command(
                        ['git', 'config', 'branch.%s.rietveldissue' % rebaseline_branch_name],
                        return_exit_code=True)
                    if not issue_already_closed:
                        self._run_git_cl_command(options, ['set_close'])

                tool.scm().ensure_cleanly_tracking_remote_master()
                if old_branch_name_or_ref:
                    tool.scm().checkout_branch(old_branch_name_or_ref)
                tool.scm().delete_branch(rebaseline_branch_name)
