#!/usr/bin/env python
# Copyright 2010 Google Inc. All Rights Reserved.
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

import BaseHTTPServer
import daemonserver
import errno
import httparchive
import logging
import os
import proxyshaper
import re
import socket
import SocketServer
import ssl
import subprocess
import sys
import time
import urlparse


class HttpProxyError(Exception):
  """Module catch-all error."""
  pass

class HttpProxyServerError(HttpProxyError):
  """Raised for errors like 'Address already in use'."""
  pass


class HttpArchiveHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  protocol_version = 'HTTP/1.1'  # override BaseHTTPServer setting

  # Since we do lots of small wfile.write() calls, turn on buffering.
  wbufsize = -1  # override StreamRequestHandler (a base class) setting

  def setup(self):
    """Override StreamRequestHandler method."""
    BaseHTTPServer.BaseHTTPRequestHandler.setup(self)
    if self.server.traffic_shaping_up_bps:
      self.rfile = proxyshaper.RateLimitedFile(
          self.server.get_active_request_count, self.rfile,
          self.server.traffic_shaping_up_bps)
    if self.server.traffic_shaping_down_bps:
      self.wfile = proxyshaper.RateLimitedFile(
          self.server.get_active_request_count, self.wfile,
          self.server.traffic_shaping_down_bps)

  # Make request handler logging match our logging format.
  def log_request(self, code='-', size='-'): pass
  def log_error(self, format, *args): logging.error(format, *args)
  def log_message(self, format, *args): logging.info(format, *args)

  def read_request_body(self):
    request_body = None
    length = int(self.headers.get('content-length', 0)) or None
    if length:
      request_body = self.rfile.read(length)
    return request_body

  def get_header_dict(self):
    return dict(self.headers.items())

  def get_archived_http_request(self):
    host = self.headers.get('host')
    if host is None:
      logging.error('Request without host header')
      return None

    parsed = urlparse.urlparse(self.path)
    params = ';%s' % parsed.params if parsed.params else ''
    query = '?%s' % parsed.query if parsed.query else ''
    fragment = '#%s' % parsed.fragment if parsed.fragment else ''
    full_path = '%s%s%s%s' % (parsed.path, params, query, fragment)

    return httparchive.ArchivedHttpRequest(
        self.command,
        host,
        full_path,
        self.read_request_body(),
        self.get_header_dict(),
        self.server.is_ssl)

  def send_archived_http_response(self, response):
    try:
      # We need to set the server name before we start the response.
      is_chunked = response.is_chunked()
      has_content_length = response.get_header('content-length') is not None
      self.server_version = response.get_header('server', 'WebPageReplay')
      self.sys_version = ''

      if response.version == 10:
        self.protocol_version = 'HTTP/1.0'

      # If we don't have chunked encoding and there is no content length,
      # we need to manually compute the content-length.
      if not is_chunked and not has_content_length:
        content_length = sum(len(c) for c in response.response_data)
        response.headers.append(('content-length', str(content_length)))

      is_replay = not self.server.http_archive_fetch.is_record_mode
      if is_replay and self.server.traffic_shaping_delay_ms:
        logging.debug('Using round trip delay: %sms',
                      self.server.traffic_shaping_delay_ms)
        time.sleep(self.server.traffic_shaping_delay_ms / 1000.0)
      if is_replay and self.server.use_delays:
        logging.debug('Using delays (ms): %s', response.delays)
        time.sleep(response.delays['headers'] / 1000.0)
        delays = response.delays['data']
      else:
        delays = [0] * len(response.response_data)
      self.send_response(response.status, response.reason)
      # TODO(mbelshe): This is lame - each write is a packet!
      for header, value in response.headers:
        if header in ('last-modified', 'expires'):
          self.send_header(header, response.update_date(value))
        elif header not in ('date', 'server'):
          self.send_header(header, value)
      self.end_headers()

      for chunk, delay in zip(response.response_data, delays):
        if delay:
          self.wfile.flush()
          time.sleep(delay / 1000.0)
        if is_chunked:
          # Write chunk length (hex) and data (e.g. "A\r\nTESSELATED\r\n").
          self.wfile.write('%x\r\n%s\r\n' % (len(chunk), chunk))
        else:
          self.wfile.write(chunk)
      if is_chunked:
        self.wfile.write('0\r\n\r\n')  # write final, zero-length chunk.
      self.wfile.flush()

      # TODO(mbelshe): This connection close doesn't seem to work.
      if response.version == 10:
        self.close_connection = 1

    except Exception, e:
      logging.error('Error sending response for %s%s: %s',
                    self.headers['host'], self.path, e)

  def handle_one_request(self):
    """Handle a single HTTP request."""
    try:
      self.raw_requestline = self.rfile.readline(65537)
      self.do_parse_and_handle_one_request()
    except socket.timeout, e:
      # A read or a write timed out.  Discard this connection
      self.log_error("Request timed out: %r", e)
      self.close_connection = 1
      return
    except socket.error, e:
      # Connection reset errors happen all the time due to the browser closing
      # without terminating the connection properly.  They can be safely
      # ignored.
      if e[0] != errno.ECONNRESET:
        raise

  def do_parse_and_handle_one_request(self):
    start_time = time.time()
    self.server.num_active_requests += 1
    request = None
    try:
      if len(self.raw_requestline) > 65536:
        self.requestline = ''
        self.request_version = ''
        self.command = ''
        self.send_error(414)
        return
      if not self.raw_requestline:
        self.close_connection = 1
        return
      if not self.parse_request():
        # An error code has been sent, just exit
        return

      try:
        request = self.get_archived_http_request()
        if request is None:
          self.send_error(500)
          return
        response = self.server.custom_handlers.handle(request)
        if not response:
          response = self.server.http_archive_fetch(request)
        if response:
          self.send_archived_http_response(response)
        else:
          self.send_error(404)
      finally:
        self.wfile.flush()  # Actually send the response if not already done.
    finally:
      request_time_ms = (time.time() - start_time) * 1000.0;
      if request:
        logging.debug('Served: %s (%dms)', request, request_time_ms)
      self.server.total_request_time += request_time_ms
      self.server.num_active_requests -= 1

  def send_error(self, status, body=None):
    """Override the default send error with a version that doesn't unnecessarily
    close the connection.
    """
    response = httparchive.create_response(status, body=body)
    self.send_archived_http_response(response)


class HttpProxyServer(SocketServer.ThreadingMixIn,
                      BaseHTTPServer.HTTPServer,
                      daemonserver.DaemonServer):
  HANDLER = HttpArchiveHandler

  # Increase the request queue size. The default value, 5, is set in
  # SocketServer.TCPServer (the parent of BaseHTTPServer.HTTPServer).
  # Since we're intercepting many domains through this single server,
  # it is quite possible to get more than 5 concurrent requests.
  request_queue_size = 128

  # Don't prevent python from exiting when there is thread activity.
  daemon_threads = True

  def __init__(self, http_archive_fetch, custom_handlers,
               host='localhost', port=80, use_delays=False, is_ssl=False,
               protocol='HTTP',
               down_bandwidth='0', up_bandwidth='0', delay_ms='0'):
    """Start HTTP server.

    Args:
      host: a host string (name or IP) for the web proxy.
      port: a port string (e.g. '80') for the web proxy.
      use_delays: if True, add response data delays during replay.
      is_ssl: True iff proxy is using SSL.
      up_bandwidth: Upload bandwidth
      down_bandwidth: Download bandwidth
           Bandwidths measured in [K|M]{bit/s|Byte/s}. '0' means unlimited.
      delay_ms: Propagation delay in milliseconds. '0' means no delay.
    """
    try:
      BaseHTTPServer.HTTPServer.__init__(self, (host, port), self.HANDLER)
    except Exception, e:
      raise HttpProxyServerError('Could not start HTTPServer on port %d: %s' %
                                 (port, e))
    self.http_archive_fetch = http_archive_fetch
    self.custom_handlers = custom_handlers
    self.use_delays = use_delays
    self.is_ssl = is_ssl
    self.traffic_shaping_down_bps = proxyshaper.GetBitsPerSecond(down_bandwidth)
    self.traffic_shaping_up_bps = proxyshaper.GetBitsPerSecond(up_bandwidth)
    self.traffic_shaping_delay_ms = int(delay_ms)
    self.num_active_requests = 0
    self.total_request_time = 0
    self.protocol = protocol

    # Note: This message may be scraped. Do not change it.
    logging.warning(
        '%s server started on %s:%d' % (self.protocol, self.server_address[0],
                                        self.server_address[1]))

  def cleanup(self):
    try:
      self.shutdown()
    except KeyboardInterrupt, e:
      pass
    logging.info('Stopped %s server. Total time processing requests: %dms',
                 self.protocol, self.total_request_time)

  def get_active_request_count(self):
    return self.num_active_requests

class HttpsProxyServer(HttpProxyServer):
  """SSL server."""

  def __init__(self, http_archive_fetch, custom_handlers, certfile, **kwargs):
    HttpProxyServer.__init__(self, http_archive_fetch, custom_handlers,
                             is_ssl=True, protocol='HTTPS', **kwargs)
    self.socket = ssl.wrap_socket(
        self.socket, certfile=certfile, server_side=True,
        do_handshake_on_connect=False)
    # Ancestor class, DaemonServer, calls serve_forever() during its __init__.

class HttpToHttpsProxyServer(HttpProxyServer):
  """Listens for HTTP requests but sends them to the target as HTTPS requests"""

  def __init__(self, http_archive_fetch, custom_handlers, **kwargs):
    HttpProxyServer.__init__(self, http_archive_fetch, custom_handlers,
                             is_ssl=True, protocol='HTTP-to-HTTPS', **kwargs)
