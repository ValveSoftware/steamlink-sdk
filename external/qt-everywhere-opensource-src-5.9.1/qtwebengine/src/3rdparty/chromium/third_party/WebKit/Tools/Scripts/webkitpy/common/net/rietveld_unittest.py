# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import urllib2

from webkitpy.common.net.rietveld import Rietveld
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.logtesting import LoggingTestCase


class RietveldTest(LoggingTestCase):

    def mock_web(self):
        return MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1, 2, 3],
            }),
            'https://codereview.chromium.org/api/11112222/2': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'foo-builder',
                        'buildnumber': None,
                        'result': -1
                    },
                    {
                        'builder': 'bar-builder',
                        'buildnumber': 50,
                        'result': 0
                    },
                ],
            }),
            'https://codereview.chromium.org/api/11112222/3': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'foo-builder',
                        'buildnumber': 20,
                        'result': 1
                    },
                    {
                        'builder': 'bar-builder',
                        'buildnumber': 60,
                        'result': 0
                    },
                ],
                'files': {
                    'some/path/foo.cc': {'status': 'M'},
                    'some/path/bar.html': {'status': 'M'},
                    'some/path/deleted.html': {'status': 'D'},
                }
            }),
            'https://codereview.chromium.org/api/11113333': 'my non-JSON contents',
        })

    def test_latest_try_jobs(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld.latest_try_jobs(11112222, ('bar-builder', 'other-builder')),
            [Build('bar-builder', 60)])

    def test_latest_try_jobs_http_error(self):
        def raise_error(_):
            raise urllib2.URLError('Some request error message')
        web = self.mock_web()
        web.get_binary = raise_error
        rietveld = Rietveld(web)
        self.assertEqual(rietveld.latest_try_jobs(11112222, ('bar-builder',)), [])
        self.assertLog(['ERROR: Request failed to URL: https://codereview.chromium.org/api/11112222\n'])

    def test_latest_try_jobs_non_json_response(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(rietveld.latest_try_jobs(11113333, ('bar-builder',)), [])
        self.assertLog(['ERROR: Invalid JSON: my non-JSON contents\n'])

    def test_latest_try_jobs_with_patchset(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld.latest_try_jobs(11112222, ('bar-builder', 'other-builder'), patchset_number=2),
            [Build('bar-builder', 50)])

    def test_latest_try_jobs_no_relevant_builders(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(rietveld.latest_try_jobs(11112222, ('foo', 'bar')), [])

    def test_changed_files(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld.changed_files(11112222),
            ['some/path/bar.html', 'some/path/foo.cc'])

    def test_changed_files_no_results(self):
        rietveld = Rietveld(self.mock_web())
        self.assertIsNone(rietveld.changed_files(11113333))

    # Testing protected methods - pylint: disable=protected-access

    def test_filter_latest_jobs_empty(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(rietveld._filter_latest_builds([]), [])

    def test_filter_latest_jobs_higher_build_first(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld._filter_latest_builds([Build('foo', 5), Build('foo', 3), Build('bar', 5)]),
            [Build('bar', 5), Build('foo', 5)])

    def test_filter_latest_jobs_higher_build_last(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld._filter_latest_builds([Build('foo', 3), Build('bar', 5), Build('foo', 5)]),
            [Build('bar', 5), Build('foo', 5)])

    def test_filter_latest_jobs_no_build_number(self):
        rietveld = Rietveld(self.mock_web())
        self.assertEqual(
            rietveld._filter_latest_builds([Build('foo', 3), Build('bar'), Build('bar')]),
            [Build('bar'), Build('foo', 3)])
