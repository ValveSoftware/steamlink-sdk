# Copyright (C) 2009 Google Inc. All rights reserved.
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

import unittest

from webkitpy.common.net.buildbot import BuildBot


class BuilderTest(unittest.TestCase):

    def test_results_url_no_build_number(self):
        self.assertEqual(
            BuildBot().results_url('Test Builder'),
            'https://storage.googleapis.com/chromium-layout-test-archives/Test_Builder/results/layout-test-results')

    def test_results_url_with_build_number(self):
        self.assertEqual(
            BuildBot().results_url('Test Builder', 10),
            'https://storage.googleapis.com/chromium-layout-test-archives/Test_Builder/10/layout-test-results')

    def test_builder_results_url_base(self):
        self.assertEqual(
            BuildBot().builder_results_url_base('WebKit Mac10.8 (dbg)'),
            'https://storage.googleapis.com/chromium-layout-test-archives/WebKit_Mac10_8__dbg_')

    def test_accumulated_results_url(self):
        self.assertEqual(
            BuildBot().accumulated_results_url_base('WebKit Mac10.8 (dbg)'),
            'https://storage.googleapis.com/chromium-layout-test-archives/WebKit_Mac10_8__dbg_/results/layout-test-results')

    def fetch_layout_test_results_with_no_responses(self):
        buildbot = BuildBot()
        buildbot._fetch_file = lambda: None  # pylint: disable=protected-access
        self.assertIsNone(buildbot.fetch_layout_test_results(buildbot.results_url('Builder')))
