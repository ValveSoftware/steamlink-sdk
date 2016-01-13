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

import daemonserver
import logging
import httparchive
import os
import sys
import threading
import time

import third_party
from nbhttp import spdy_server
from nbhttp import push_tcp
from nbhttp import http_common

CONTENT_LENGTH = 'content-length'
STATUS = 'status'
VERSION = 'version'

class ReplaySpdyServer(daemonserver.DaemonServer):
  def __init__(self, http_archive_fetch, custom_handlers,
               host='localhost', port=80, certfile=None, keyfile=None):
    """Initialize ReplaySpdyServer.

    The private key may be stored in |certfile|. If so, |keyfile|
    may be left unset.
    """
    #TODO(lzheng): figure out how to get the log level from main.
    self.log = logging.getLogger('ReplaySpdyServer')
    self.log.setLevel(logging.INFO)
    self.http_archive_fetch = http_archive_fetch
    self.custom_handlers = custom_handlers
    self.host = host
    self.port = port
    self.use_ssl = certfile is not None
    self.spdy_server = spdy_server.SpdyServer(
        host, port, self.use_ssl, certfile, keyfile, self.request_handler,
        self.log)

  def serve_forever(self):
    self.log.info('Replaying with SPDY on %s:%d', self.host, self.port)
    push_tcp.run()

  def cleanup(self):
    push_tcp.stop()
    self.log.info('Stopped spdy server')

  def request_handler(self, method, uri, hdrs, res_start, req_pause):
    """
    Based on method, host and uri to fetch the matching response and reply
    to browser using spdy.
    """
    dummy = http_common.dummy
    def simple_responder(code, content):
      res_hdrs = [('content-type', 'text/html'), ('version', 'HTTP/1.1')]
      res_body, res_done = res_start(str(code), content, res_hdrs, dummy)
      res_body(None)
      res_done(None)

    host = ''
    for name, value in hdrs:
      if name.lower() == 'host':
        host = value
    self.log.debug("request: %s, uri: %s, method: %s", host, uri, method)

    if method == 'GET':
      request = httparchive.ArchivedHttpRequest(
          method, host, uri, None, dict(hdrs))
      response_code = self.custom_handlers.handle(request)
      if response_code:
        simple_responder(response_code, "Handled by custom handlers")
        return dummy, dummy
      response = self.http_archive_fetch(request)
      if response:
        res_hdrs = [('version', 'HTTP/1.1')]
        for name, value in response.headers:
          name_lower = name.lower()
          if name_lower == CONTENT_LENGTH:
            res_hdrs.append((name, str(value)))
          elif name_lower in (STATUS, VERSION):
            pass
          else:
            res_hdrs.append((name_lower, value))
        res_body, res_done = res_start(
            str(response.status), response.reason, res_hdrs, dummy)
        body = ''
        for item in response.response_data:
          res_body(item)
        res_done(None)
      else:
        self.log.error("404 returned: %s %s", method, uri)
        simple_responder(404, "file not found")
    else:
      # TODO(lzheng): Add support for other methods.
      self.log.error("method: %s is not supported: %s", method, uri)
      simple_responder(500, "Not supported")
    return dummy, dummy


if __name__ == "__main__":
    logging.basicConfig()
    log = logging.getLogger('server')
    log.setLevel(logging.INFO)
    filename = sys.argv[1]
    host = '127.0.0.1'
    port = 8088
    server = ReplaySpdyServer(filename, host, port)
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.setDaemon(True)
    server_thread.start()
    time.sleep(60)
    server.cleanup();
