# Copyright (C) 2012 Google Inc. All rights reserved.
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

import buildershandler
import json
import logging
import pprint
import unittest

class BuildersHandlerTest(unittest.TestCase):
    def test_fetch_buildbot_data(self):
        try:
            fetched_urls = []

            def fake_fetch_json(url):
                fetched_urls.append(url)

                if url == 'http://chrome-build-extract.appspot.com/get_master/chromium.webkit':
                    return {
                        'builders': {
                            'WebKit Win': None, 'WebKit Linux': None, 'WebKit Mac': None, 'WebKit Empty': None,
                        }
                    }

                if url == 'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Linux&master=chromium.webkit&num_builds=1':
                    return {
                        'builds': [
                            {'steps': [{'name': 'foo_tests_only'}, {'name': 'webkit_tests'}, {'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]},
                        ],
                    }

                if url == 'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Win&master=chromium.webkit&num_builds=1':
                    return {
                        'builds': [
                            {'steps': [{'name': 'foo_tests_ignore'}, {'name': 'webkit_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]},
                        ],
                    }

                if url == 'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Mac&master=chromium.webkit&num_builds=1':
                    return {
                        'builds': [
                            {'steps': [{'name': 'foo_tests_perf'}, {'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]},
                        ],
                    }

                if url == 'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Empty&master=chromium.webkit&num_builds=1':
                    return {'builds': []}

                logging.error('Cannot fetch fake url: %s' % url)

            old_fetch_json = buildershandler.fetch_json
            buildershandler.fetch_json = fake_fetch_json

            masters = [
                {'name': 'ChromiumWebkit', 'url_name': 'chromium.webkit'},
            ]

            buildbot_data = buildershandler.fetch_buildbot_data(masters)

            expected_fetched_urls = [
                'http://chrome-build-extract.appspot.com/get_master/chromium.webkit',
                'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Win&master=chromium.webkit&num_builds=1',
                'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Mac&master=chromium.webkit&num_builds=1',
                'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Empty&master=chromium.webkit&num_builds=1',
                'http://chrome-build-extract.appspot.com/get_builds?builder=WebKit%20Linux&master=chromium.webkit&num_builds=1',
            ]
            self.assertEqual(set(fetched_urls), set(expected_fetched_urls))

            expected_masters = {
                'masters': [{
                    'tests': {
                        'browser_tests': {'builders': ['WebKit Linux', 'WebKit Mac']},
                        'mini_installer_test': {'builders': ['WebKit Linux', 'WebKit Mac', 'WebKit Win']},
                        'layout-tests': {'builders': ['WebKit Linux', 'WebKit Win']}},
                    'name': 'ChromiumWebkit',
                    'url_name': 'chromium.webkit',
                }],
                "no_upload_test_types": buildershandler.TEST_STEPS_THAT_DO_NOT_UPLOAD_YET,
            }
            expected_json = buildershandler.dump_json(expected_masters)

            self.assertEqual(buildbot_data, expected_json)
        finally:
            buildershandler.fetch_json = old_fetch_json

    def test_fetch_buildbot_data_failure(self):
        try:
            fetched_urls = []

            def fake_fetch_json(url):
                fetched_urls.append(url)

                if url == 'http://chrome-build-extract.appspot.com/get_master/chromium.webkit':
                    return None

                if url == 'http://chrome-build-extract.appspot.com/get_master/chromium.gpu':
                    return {
                        'builders': {
                            'Win GPU': None, 'Win Empty': None,
                        }
                    }

                if url == 'http://chrome-build-extract.appspot.com/get_master/chromium.fyi':
                    return {
                        'builders': {
                            'Mac FYI': None,
                        }
                    }

                if (url == 'http://chrome-build-extract.appspot.com/get_builds?builder=Win%20Empty&master=chromium.gpu&num_builds=1'
                    or url == 'http://chrome-build-extract.appspot.com/get_builds?builder=Win%20GPU&master=chromium.gpu&num_builds=1'):
                    return {
                        'builds': [
                            {'steps': []},
                        ],
                    }

                logging.error('Cannot fetch fake url: %s' % url)

            old_fetch_json = buildershandler.fetch_json
            buildershandler.fetch_json = fake_fetch_json

            masters = [
                {'name': 'ChromiumGPU', 'url_name': 'chromium.gpu'},
                {'name': 'ChromiumWebkit', 'url_name': 'chromium.webkit'},
                {'name': 'ChromiumFYI', 'url_name': 'chromium.fyi'},
            ]

            expected_fetched_urls = [
                'http://chrome-build-extract.appspot.com/get_master/chromium.webkit',
                'http://chrome-build-extract.appspot.com/get_master/chromium.gpu',
                'http://chrome-build-extract.appspot.com/get_builds?builder=Win%20GPU&master=chromium.gpu&num_builds=1',
                'http://chrome-build-extract.appspot.com/get_builds?builder=Win%20Empty&master=chromium.gpu&num_builds=1',
            ]
            with self.assertRaises(buildershandler.FetchBuildersException):
                buildbot_data = buildershandler.fetch_buildbot_data(masters)
            self.assertEqual(set(expected_fetched_urls), set(fetched_urls))

        finally:
            buildershandler.fetch_json = old_fetch_json


if __name__ == '__main__':
    unittest.main()
