# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for a Rietveld issue.

This command interacts with the Rietveld API to get information about try jobs
with layout test results.
"""

import json
import logging
import optparse

from webkitpy.common.net.rietveld import Rietveld
from webkitpy.common.net.web import Web
from webkitpy.common.net.git_cl import GitCL
from webkitpy.layout_tests.models.test_expectations import BASELINE_SUFFIX_LIST
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand

_log = logging.getLogger(__name__)


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = "rebaseline-cl"
    help_text = "Fetches new baselines for a CL from test runs on try bots."
    long_help = ("By default, this command will check the latest try job results "
                 "for all platforms, and start try jobs for platforms with no "
                 "try jobs. Then, new baselines are downloaded for any tests "
                 "that are being rebaselined. After downloading, the baselines "
                 "for different platforms will be optimized (consolidated).")
    show_in_main_help = True

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            optparse.make_option(
                '--issue', type='int', default=None,
                help='Rietveld issue number; if none given, this will be obtained via `git cl issue`.'),
            optparse.make_option(
                '--dry-run', action='store_true', default=False,
                help='Dry run mode; list actions that would be performed but do not do anything.'),
            optparse.make_option(
                '--only-changed-tests', action='store_true', default=False,
                help='Only download new baselines for tests that are changed in the CL.'),
            optparse.make_option(
                '--no-trigger-jobs', dest='trigger_jobs', action='store_false', default=True,
                help='Do not trigger any try jobs.'),
            self.no_optimize_option,
            self.results_directory_option,
        ])
        self.rietveld = Rietveld(Web())

    def execute(self, options, args, tool):
        self._tool = tool
        issue_number = self._get_issue_number(options)
        if not issue_number:
            return

        builds = self.rietveld.latest_try_jobs(issue_number, self._try_bots())
        if options.trigger_jobs:
            if self.trigger_jobs_for_missing_builds(builds):
                _log.info('Please re-run webkit-patch rebaseline-cl once all pending try jobs have finished.')
                return
        if not builds:
            _log.info('No builds to download baselines from.')

        if args:
            test_prefix_list = {}
            for test in args:
                test_prefix_list[test] = {b: BASELINE_SUFFIX_LIST for b in builds}
        else:
            test_prefix_list = self._test_prefix_list(
                issue_number, only_changed_tests=options.only_changed_tests)

        self._log_test_prefix_list(test_prefix_list)

        if options.dry_run:
            return
        self.rebaseline(options, test_prefix_list)

    def _get_issue_number(self, options):
        """Gets the Rietveld CL number from either |options| or from the current local branch."""
        if options.issue:
            return options.issue
        issue_number = self.git_cl().get_issue_number()
        _log.debug('Issue number for current branch: %s', issue_number)
        if not issue_number.isdigit():
            _log.error('No issue number given and no issue for current branch. This tool requires a CL\n'
                       'to operate on; please run `git cl upload` on this branch first, or use the --issue\n'
                       'option to download baselines for another existing CL.')
            return None
        return int(issue_number)

    def git_cl(self):
        """Returns a GitCL instance; can be overridden for tests."""
        return GitCL(self._tool)

    def trigger_jobs_for_missing_builds(self, builds):
        """Triggers try jobs for any builders that have no builds started.

        Args:
          builds: A list of Build objects; if the build number of a Build is None,
              then that indicates that the job is pending.

        Returns:
            True if there are pending jobs to wait for, including jobs just started.
        """
        builders_with_builds = {b.builder_name for b in builds}
        builders_without_builds = set(self._try_bots()) - builders_with_builds
        builders_with_pending_builds = {b.builder_name for b in builds if b.build_number is None}

        if builders_with_pending_builds:
            _log.info('There are existing pending builds for:')
            for builder in sorted(builders_with_pending_builds):
                _log.info('  %s', builder)

        if builders_without_builds:
            _log.info('Triggering try jobs for:')
            command = ['try']
            for builder in sorted(builders_without_builds):
                _log.info('  %s', builder)
                command.extend(['-b', builder])
            self.git_cl().run(command)

        return bool(builders_with_pending_builds or builders_without_builds)

    def _test_prefix_list(self, issue_number, only_changed_tests):
        """Returns a collection of test, builder and file extensions to get new baselines for.

        Args:
            issue_number: The CL number of the change which needs new baselines.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A dict containing information about which new baselines to download.
        """
        builds_to_tests = self._builds_to_tests(issue_number)
        if only_changed_tests:
            files_in_cl = self.rietveld.changed_files(issue_number)
            # Note, in the changed files list from Rietveld, paths always
            # use / as the separator, and they're always relative to repo root.
            # TODO(qyearsley): Do this without using a hard-coded constant.
            test_base = 'third_party/WebKit/LayoutTests/'
            tests_in_cl = [f[len(test_base):] for f in files_in_cl if f.startswith(test_base)]
        result = {}
        for build, tests in builds_to_tests.iteritems():
            for test in tests:
                if only_changed_tests and test not in tests_in_cl:
                    continue
                if test not in result:
                    result[test] = {}
                result[test][build] = BASELINE_SUFFIX_LIST
        return result

    def _builds_to_tests(self, issue_number):
        """Fetches a list of try bots, and for each, fetches tests with new baselines."""
        _log.debug('Getting results for Rietveld issue %d.', issue_number)
        builds = self.rietveld.latest_try_jobs(issue_number, self._try_bots())
        if not builds:
            _log.debug('No try job results for builders in: %r.', self._try_bots())
        return {build: self._tests_to_rebaseline(build) for build in builds}

    def _try_bots(self):
        """Returns a collection of try bot builders to fetch results for."""
        return self._tool.builders.all_try_builder_names()

    def _tests_to_rebaseline(self, build):
        """Fetches a list of tests that should be rebaselined."""
        buildbot = self._tool.buildbot
        results_url = buildbot.results_url(build.builder_name, build.build_number)

        layout_test_results = buildbot.fetch_layout_test_results(results_url)
        if layout_test_results is None:
            _log.warning('Failed to request layout test results from "%s".', results_url)
            return []

        unexpected_results = layout_test_results.didnt_run_as_expected_results()
        tests = sorted(r.test_name() for r in unexpected_results
                       if r.is_missing_baseline() or r.has_mismatch_result())

        new_failures = self._fetch_tests_with_new_failures(build)
        if new_failures is None:
            _log.warning('No retry summary available for build %s.', build)
        else:
            tests = [t for t in tests if t in new_failures]
        return tests

    def _fetch_tests_with_new_failures(self, build):
        """Fetches a list of tests that failed with a patch in a given try job but not without."""
        buildbot = self._tool.buildbot
        content = buildbot.fetch_retry_summary_json(build)
        if content is None:
            return None
        try:
            retry_summary = json.loads(content)
            return retry_summary['failures']
        except (ValueError, KeyError):
            _log.warning('Unexepected retry summary content:\n%s', content)
            return None

    @staticmethod
    def _log_test_prefix_list(test_prefix_list):
        """Logs the tests to download new baselines for."""
        if not test_prefix_list:
            _log.info('No tests to rebaseline; exiting.')
            return
        _log.debug('Tests to rebaseline:')
        for test, builds in test_prefix_list.iteritems():
            _log.debug('  %s:', test)
            for build in sorted(builds):
                _log.debug('    %s', build)
