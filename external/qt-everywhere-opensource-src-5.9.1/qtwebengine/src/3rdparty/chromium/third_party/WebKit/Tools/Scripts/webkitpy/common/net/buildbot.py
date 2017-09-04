# Copyright (c) 2009, Google Inc. All rights reserved.
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
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import collections
import re
import urllib2

from webkitpy.common.memoized import memoized
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.networktransaction import NetworkTransaction


RESULTS_URL_BASE = 'https://storage.googleapis.com/chromium-layout-test-archives'


class Build(collections.namedtuple('Build', ('builder_name', 'build_number'))):
    """Represents a combination of builder and build number.

    If build number is None, this represents the latest build
    for a given builder.
    """
    def __new__(cls, builder_name, build_number=None):
        return super(Build, cls).__new__(cls, builder_name, build_number)


class BuildBot(object):
    """This class represents an interface to BuildBot-related functionality.

    This includes fetching layout test results from Google Storage;
    for more information about the layout test result format, see:
        https://www.chromium.org/developers/the-json-test-results-format
    """
    def results_url(self, builder_name, build_number=None):
        """Returns a URL for one set of archived layout test results.

        If a build number is given, this will be results for a particular run;
        otherwise it will be the accumulated results URL, which should have
        the latest results.
        """
        if build_number:
            url_base = self.builder_results_url_base(builder_name)
            return "%s/%s/layout-test-results" % (url_base, build_number)
        return self.accumulated_results_url_base(builder_name)

    def builder_results_url_base(self, builder_name):
        """Returns the URL for the given builder's directory in Google Storage.

        Each builder has a directory in the GS bucket, and the directory
        name is the builder name transformed to be more URL-friendly by
        replacing all spaces, periods and parentheses with underscores.
        """
        return '%s/%s' % (RESULTS_URL_BASE, re.sub('[ .()]', '_', builder_name))

    @memoized
    def fetch_retry_summary_json(self, build):
        """Fetches and returns the text of the archived retry_summary file.

        This file is expected to contain the results of retrying layout tests
        with and without a patch in a try job. It includes lists of tests
        that failed only with the patch ("failures"), and tests that failed
        both with and without ("ignored").
        """
        url_base = "%s/%s" % (self.builder_results_url_base(build.builder_name), build.build_number)
        return NetworkTransaction(convert_404_to_None=True).run(
            lambda: self._fetch_file(url_base, "retry_summary.json"))

    def accumulated_results_url_base(self, builder_name):
        return self.builder_results_url_base(builder_name) + "/results/layout-test-results"

    @memoized
    def latest_layout_test_results(self, builder_name):
        return self.fetch_layout_test_results(self.accumulated_results_url_base(builder_name))

    @memoized
    def fetch_results(self, build):
        return self.fetch_layout_test_results(self.results_url(build.builder_name, build.build_number))

    @memoized
    def fetch_layout_test_results(self, results_url):
        """Returns a LayoutTestResults object for results fetched from a given URL."""
        results_file = NetworkTransaction(convert_404_to_None=True).run(
            lambda: self._fetch_file(results_url, "failing_results.json"))
        revision = NetworkTransaction(convert_404_to_None=True).run(
            lambda: self._fetch_file(results_url, "LAST_CHANGE"))
        if not revision:
            results_file = None
        return LayoutTestResults.results_from_string(results_file, revision)

    def _fetch_file(self, url_base, file_name):
        # It seems this can return None if the url redirects and then returns 404.
        # FIXME: This could use Web instead of using urllib2 directly.
        result = urllib2.urlopen("%s/%s" % (url_base, file_name))
        if not result:
            return None
        # urlopen returns a file-like object which sometimes works fine with str()
        # but sometimes is a addinfourl object.  In either case calling read() is correct.
        return result.read()
