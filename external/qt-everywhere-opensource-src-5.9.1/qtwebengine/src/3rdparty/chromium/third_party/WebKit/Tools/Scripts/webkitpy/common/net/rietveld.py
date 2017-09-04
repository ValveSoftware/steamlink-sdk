# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions to communicate with Rietveld."""

import json
import logging
import urllib2

from webkitpy.common.net.buildbot import Build

_log = logging.getLogger(__name__)

BASE_CODEREVIEW_URL = 'https://codereview.chromium.org/api'


class Rietveld(object):

    def __init__(self, web):
        self.web = web

    def latest_try_jobs(self, issue_number, builder_names=None, patchset_number=None):
        """Returns a list of Build objects for builds on the latest patchset.

        Args:
            issue_number: A Rietveld issue number.
            builder_names: A collection of builder names. If specified, only results
                from the given list of builders will be kept.
            patchset_number: If given, a specific patchset will be used instead of the latest one.

        Returns:
            A list of Build objects, where Build objects for completed jobs have a build number,
            and Build objects for pending jobs have no build number.
        """
        try:
            if patchset_number:
                url = self._patchset_url(issue_number, patchset_number)
            else:
                url = self._latest_patchset_url(issue_number)
            patchset_data = self._get_json(url)
        except (urllib2.URLError, ValueError):
            return []

        builds = []
        for result_dict in patchset_data['try_job_results']:
            build = Build(result_dict['builder'], result_dict['buildnumber'])
            # Normally, a value of -1 or 6 in the "result" field indicates the job is
            # started or pending, and the "buildnumber" field is null.
            if build.build_number and result_dict['result'] in (-1, 6):
                _log.warning('Build %s has result %d, but unexpectedly has a build number.', build, result_dict['result'])
                build.build_number = None
            builds.append(build)

        if builder_names is not None:
            builds = [b for b in builds if b.builder_name in builder_names]

        return self._filter_latest_builds(builds)

    def _filter_latest_builds(self, builds):
        """Filters out a collection of Build objects to include only the latest for each builder.

        Args:
            jobs: A list of Build objects.

        Returns:
            A list of Build objects; only one Build object per builder name. If there are only
            Builds with no build number, then one is kept; if there are Builds with build numbers,
            then the one with the highest build number is kept.
        """
        builder_to_latest_build = {}
        for build in builds:
            if build.builder_name not in builder_to_latest_build:
                builder_to_latest_build[build.builder_name] = build
            elif build.build_number > builder_to_latest_build[build.builder_name].build_number:
                builder_to_latest_build[build.builder_name] = build
        return sorted(builder_to_latest_build.values())

    def changed_files(self, issue_number):
        """Lists the files included in a CL that are changed but not deleted.

        File paths are sorted and relative to the repository root.
        """
        try:
            url = self._latest_patchset_url(issue_number)
            issue_data = self._get_json(url)
            return sorted(path for path, file_change in issue_data['files'].iteritems() if file_change['status'] != 'D')
        except (urllib2.URLError, ValueError, KeyError):
            _log.warning('Failed to list changed files for issue %s.', issue_number)
            return None

    def _latest_patchset_url(self, issue_number):
        issue_data = self._get_json(self._issue_url(issue_number))
        latest_patchset_number = issue_data["patchsets"][-1]
        return self._patchset_url(issue_number, latest_patchset_number)

    def _get_json(self, url):
        """Fetches JSON from a URL, and logs errors if the request was unsuccessful.

        Raises:
            urllib2.URLError: Something went wrong with the request.
            ValueError: The response wasn't valid JSON.
        """
        try:
            contents = self.web.get_binary(url)
        except urllib2.URLError:
            _log.error('Request failed to URL: %s', url)
            raise
        try:
            return json.loads(contents)
        except ValueError:
            _log.error('Invalid JSON: %s', contents)
            raise

    def _issue_url(self, issue_number):
        return '%s/%s' % (BASE_CODEREVIEW_URL, issue_number)

    def _patchset_url(self, issue_number, patchset_number):
        return '%s/%s' % (self._issue_url(issue_number), patchset_number)
