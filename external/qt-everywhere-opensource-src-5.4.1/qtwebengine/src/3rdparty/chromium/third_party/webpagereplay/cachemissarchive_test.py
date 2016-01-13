#!/usr/bin/env python
# Copyright 2011 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import ast
import cachemissarchive
from mockhttprequest import ArchivedHttpRequest
import os
import unittest
import util


def get_mock_requests():
  keepends = True
  return util.resource_string('mock-archive.txt').splitlines(keepends)


class CacheMissArchiveTest(unittest.TestCase):

  HEADERS = [('accept-encoding', 'gzip,deflate')]
  REQUEST = ArchivedHttpRequest(
      'GET', 'www.test.com', '/', None, HEADERS)

  def setUp(self):
    self.load_mock_archive()

  def load_mock_archive(self):
    self.cache_archive = cachemissarchive.CacheMissArchive('mock-archive')
    self.num_requests = 0
    urls_list = [
        'http://www.zappos.com/',
        'http://www.msn.com/',
        'http://www.amazon.com/',
        'http://www.google.com/',
    ]
    self.cache_archive.set_urls_list(urls_list)
    for line in get_mock_requests():
      # Each line contains: (command, host, path, request_body, headers)
      # Delimited by '%'
      args = line.split('%')
      headers = ast.literal_eval(args[4].strip('\n '))
      request = ArchivedHttpRequest(
          args[0], args[1], args[2], args[3], headers)
      self.cache_archive.record_request(request, is_record_mode=False,
                                        is_cache_miss=True)
      self.num_requests += 1

  def test_init(self):
    empty_archive = cachemissarchive.CacheMissArchive('empty-archive')
    self.assert_(not empty_archive.archive)

  def test_record_cache_miss(self):
    cache_archive = cachemissarchive.CacheMissArchive('empty-archive')
    referer = 'mock_referer'
    cache_archive.record_cache_miss(self.REQUEST, page_url=referer)
    self.assert_(cache_archive.archive[referer])

  def test__match_urls(self):
    self.assert_(self.cache_archive._match_urls(
        'http://www.cnn.com', 'http://www.cnn.com'))
    self.assert_(self.cache_archive._match_urls(
        'http://www.cnn.com', 'www.cnn.com'))
    self.assert_(not self.cache_archive._match_urls(
        'http://www.zappos.com', 'http://www.cnn.com'))
    self.assert_(not self.cache_archive._match_urls(
        'www.zappos.com', 'www.amazon.com'))

  def test_get_total_referers_small(self):
    cache_archive = cachemissarchive.CacheMissArchive('empty-archive')
    self.assertEqual(cache_archive.get_total_referers(), 0)
    referer = 'mock_referer'
    cache_archive.record_cache_miss(self.REQUEST, page_url=referer)
    self.assertEqual(cache_archive.get_total_referers(), 1)

  def test_get_total_referers_large(self):
    self.assertEqual(self.cache_archive.get_total_referers(), 4)

  def test_get_total_cache_misses(self):
    self.assertEqual(self.cache_archive.get_total_cache_misses(),
                     self.num_requests)

  def test_get_total_referer_cache_misses(self):
    self.assertEqual(self.cache_archive.get_total_referer_cache_misses(),
                     self.num_requests)

  def test_record_request(self):
    request = self.REQUEST
    cache_archive = cachemissarchive.CacheMissArchive('empty-archive')
    self.assertEqual(len(cache_archive.request_counts), 0)

    cache_archive.record_request(request, is_record_mode=True,
                                 is_cache_miss=False)
    self.assertEqual(len(cache_archive.request_counts), 1)
    self.assertEqual(cache_archive.request_counts[request], (1, 0))

    cache_archive.record_request(request, is_record_mode=False,
                                 is_cache_miss=False)
    self.assertEqual(len(cache_archive.request_counts), 1)
    self.assertEqual(cache_archive.request_counts[request], (1, 1))

  def test_get_cache_misses(self):
    self.assertEqual(
        len(self.cache_archive.get_cache_misses('http://www.zappos.com/')), 5)
    self.assertEqual(
        len(self.cache_archive.get_cache_misses('http://www.msn.com/')), 3)
    self.assertEqual(
        len(self.cache_archive.get_cache_misses('http://www.google.com/')), 1)
    self.assertEqual(
        len(self.cache_archive.get_cache_misses('http://www.amazon.com/')), 1)

if __name__ == '__main__':
  unittest.main()
