# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for a Rietveld issue.

This command interacts with the Rietveld API to get information about try jobs
with layout test results.

TODO(qyearsley): Finish this module. After getting a list of try bots:
  - For each bot get the list of tests to rebaseline.
  - Invoke webkit-patch rebaseline-test-internal to fetch the baselines.
"""

import logging
import optparse

from webkitpy.common.net.rietveld import latest_try_jobs
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from webkitpy.common.net.web import Web


TRY_BOTS = (
    'linux_chromium_rel_ng',
    'mac_chromium_rel_ng',
    'win_chromium_rel_ng',
)

_log = logging.getLogger(__name__)


class RebaselineFromTryJobs(AbstractParallelRebaselineCommand):
    name = "rebaseline-from-try-jobs"
    help_text = "Fetches new baselines from layout test runs on try bots."
    show_in_main_help = True

    def __init__(self):
        super(RebaselineFromTryJobs, self).__init__(options=[
            optparse.make_option(
                '--issue', type='int', default=None,
                help="Rietveld issue number."),
        ])
        self.web = Web()

    def _unexpected_mismatch_results(self, try_job):
        results_url = self._results_url(try_job.builder_name, try_job.master_name, try_job.build_number)
        builder = self._tool.buildbot.builder_with_name(try_job.builder_name, try_job.master_name)
        layout_test_results = builder.fetch_layout_test_results(results_url)
        if layout_test_results is None:
            _log.warning('Failed to request layout test results from "%s".', results_url)
            return []
        return layout_test_results.unexpected_mismatch_results()

    def execute(self, options, args, tool):
        if not options.issue:
            # TODO(qyearsley): Later, the default behavior will be to run
            # git cl issue to get the issue number, then get lists of tests
            # to rebaseline and download new baselines.
            _log.info('No issue number provided.')
            return
        _log.info('Getting results for Rietveld issue %d.' % options.issue)

        jobs = latest_try_jobs(options.issue, TRY_BOTS, self.web)

        for job in jobs:
            _log.info('  Builder: %s', job.builder_name)
            _log.info('  Master: %s', job.master_name)
            _log.info('  Build: %s', job.build_number)
            test_results = self._unexpected_mismatch_results(job)
            if test_results:
                for result in test_results:
                    _log.info(
                        '%s (actual: %s, expected: %s)', result.test_name(),
                        result.actual_results(), result.expected_results())
            else:
                _log.info('No unexpected test results.')
