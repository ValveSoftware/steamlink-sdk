# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.layouttestresults_unittest import LayoutTestResultsTest


# TODO(qyearsley): Instead of canned results from another module, other unit
# tests may be a little easier to understand if this returned None by default
# when there are no canned results to return.
class MockBuildBot(BuildBot):

    def __init__(self):
        super(MockBuildBot, self).__init__()
        # Dict of Build to canned LayoutTestResults.
        self._canned_results = {}
        self._canned_retry_summary_json = {}

    def fetch_layout_test_results(self, _):
        return LayoutTestResults.results_from_string(LayoutTestResultsTest.example_full_results_json)

    def set_results(self, build, results):
        self._canned_results[build] = results

    def fetch_results(self, build):
        return self._canned_results.get(
            build,
            LayoutTestResults.results_from_string(LayoutTestResultsTest.example_full_results_json))

    def set_retry_sumary_json(self, build, content):
        self._canned_retry_summary_json[build] = content

    def fetch_retry_summary_json(self, build):
        return self._canned_retry_summary_json.get(build)
