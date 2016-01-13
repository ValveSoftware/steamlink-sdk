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

"""Create and view cache miss archives.

Usage:
./cachemissarchive.py <path to CacheMissArchive file>

This will print out some statistics of the cache archive.
"""

import logging
import os
import sys
from perftracker import runner_cfg
import persistentmixin


def format_request(request, join_val=' ', use_path=True,
                   use_request_body=False, headers=False):
  if use_path:
    request_parts = [request.command, request.host + request.path]
  else:
    request_parts = [request.command, request.host]
  if use_request_body:
    request_parts.append(request.request_body)
  if headers:
    request_parts.append(request.headers)
  return join_val.join([str(x) for x in request_parts])


class CacheMissArchive(persistentmixin.PersistentMixin):
  """Archives cache misses from playback mode.

  Uses runner_cfg.urls for tracking the current page url.

  Attributes:
    archive_file: output file to store cache miss data
    current_page_url: any cache misses will be marked as caused by this URL
    page_urls: the list of urls to record and keep track of
    archive: dict of cache misses, where the key is a page URL and
      the value is a list of ArchivedHttpRequest objects
    request_counts: dict that records the number of times a request is issued in
      both record and replay mode
  """

  def __init__(self, archive_file):
    """Initialize CacheMissArchive.

    Args:
      archive_file: output file to store data
    """
    self.archive_file = archive_file
    self.current_page_url = None

    # TODO: Pass in urls to CacheMissArchive without runner_cfg dependency
    if runner_cfg.urls:
      self.page_urls = runner_cfg.urls

    # { URL: [archived_http_request, ...], ... }
    self.archive = {}

    # { archived_http_request: (num_record_requests, num_replay_requests), ... }
    self.request_counts = {}

  def record_cache_miss(self, request, page_url=None):
    """Records a cache miss for given request.

    Args:
      request: instance of ArchivedHttpRequest that causes a cache miss
      page_url: specify the referer URL that caused this cache miss
    """
    if not page_url:
      page_url = self.current_page_url
    logging.debug('Cache miss on %s', request)
    self._append_archive(page_url, request)

  def set_urls_list(self, urls):
    self.page_urls = urls

  def record_request(self, request, is_record_mode, is_cache_miss=False):
    """Records the request into the cache archive.

    Should be updated on every HTTP request.

    Also updates the current page_url contained in runner_cfg.urls.

    Args:
      request: instance of ArchivedHttpRequest
      is_record_mode: indicates whether WPR is on record mode
      is_cache_miss: if True, records the request as a cache miss
    """
    self._record_request(request, is_record_mode)

    page_url = request.host + request.path

    for url in self.page_urls:
      if self._match_urls(page_url, url):
        self.current_page_url = url
        logging.debug('Updated current url to %s', self.current_page_url)
        break

    if is_cache_miss:
      self.record_cache_miss(request)

  def _record_request(self, request, is_record_mode):
    """Adds 1 to the appropriate request count.

    Args:
      request: instance of ArchivedHttpRequest
      is_record_mode: indicates whether WPR is on record mode
    """
    num_record, num_replay = self.request_counts.get(request, (0, 0))
    if is_record_mode:
      num_record += 1
    else:
      num_replay += 1
    self.request_counts[request] = (num_record, num_replay)

  def request_diff(self, is_show_all=False):
    """Calculates if there are requests sent in record mode that are
    not sent in replay mode and vice versa.

    Args:
      is_show_all: If True, only includes instance where the number of requests
        issued in record/replay mode differs. If False, includes all instances.
    Returns:
      A string displaying difference in requests between record and replay modes
    """
    str_list = ['Diff of requests sent in record mode versus replay mode\n']
    less = []
    equal = []
    more = []

    for request, (num_record, num_replay) in self.request_counts.items():
      format_req = format_request(request, join_val=' ',
                                  use_path=True, use_request_body=False)
      request_line = '%s record: %d, replay: %d' % (
          format_req, num_record, num_replay)
      if num_record < num_replay:
        less.append(request_line)
      elif num_record == num_replay:
        equal.append(request_line)
      else:
        more.append(request_line)

    if is_show_all:
      str_list.extend(sorted(equal))

    str_list.append('')
    str_list.extend(sorted(less))
    str_list.append('')
    str_list.extend(sorted(more))

    return '\n'.join(str_list)

  def _match_urls(self, url_1, url_2):
    """Returns true if urls match.

    Args:
      url_1: url string (e.g. 'http://www.cnn.com')
      url_2: same as url_1
    Returns:
      True if the two urls match, false otherwise
    """
    scheme = 'http://'
    if url_1.startswith(scheme):
      url_1 = url_1[len(scheme):]
    if url_2.startswith(scheme):
      url_2 = url_2[len(scheme):]
    return url_1 == url_2

  def _append_archive(self, page_url, request):
    """Appends the corresponding (page_url,request) pair to archived dictionary.

    Args:
      page_url: page_url string (e.g. 'http://www.cnn.com')
      request: instance of ArchivedHttpRequest
    """
    self.archive.setdefault(page_url, [])
    self.archive[page_url].append(request)

  def __repr__(self):
    return repr((self.archive_file, self.archive))

  def Persist(self):
    self.current_page_url = None
    persistentmixin.PersistentMixin.Persist(self, self.archive_file)

  def get_total_referers(self):
    return len(self.archive)

  def get_total_cache_misses(self):
    count = 0
    for k in self.archive:
      count += len(self.archive[k])
    return count

  def get_total_referer_cache_misses(self):
    count = 0
    if self.page_urls:
      count = sum(len(v) for k, v in self.archive.items()
                  if k in self.page_urls)
    return count

  def get_cache_misses(self, page_url, join_val=' ',
                       use_path=False, use_request_body=False):
    """Returns a list of cache miss requests from the page_url.

    Args:
      page_url: url of the request (e.g. http://www.zappos.com/)
      join_val: value to join output string with
      use_path: true if path is to be included in output display
      use_request_body: true if request_body is to be included in output display
    Returns:
      A list of cache miss requests (in textual representation) from page_url
    """
    misses = []
    if page_url in self.archive:
      cache_misses = self.archive[page_url]
      for k in cache_misses:
        misses.append(format_request(k, join_val, use_path, use_request_body))
    return misses

  def get_all_cache_misses(self, use_path=False):
    """Format cache misses into concise visualization."""
    all_cache_misses = ''
    for page_url in self.archive:
      misses = self.get_cache_misses(page_url, use_path=use_path)
      all_cache_misses = '%s%s --->\n  %s\n\n' % (
          all_cache_misses, page_url, '\n  '.join(misses))
    return all_cache_misses


if __name__ == '__main__':
  archive_file = sys.argv[1]
  cache_archive = CacheMissArchive.Load(archive_file)

  print 'Total cache misses: %d' % cache_archive.get_total_cache_misses()
  print 'Total page_urls cache misses: %d' % (
      cache_archive.get_total_referer_cache_misses())
  print 'Total referers: %d\n' % cache_archive.get_total_referers()
  print 'Referers are:'
  for ref in cache_archive.archive:
    print '%s  with %d cache misses' % (ref, len(cache_archive.archive[ref]))
  print
  print cache_archive.get_all_cache_misses(use_path=True)
  print
