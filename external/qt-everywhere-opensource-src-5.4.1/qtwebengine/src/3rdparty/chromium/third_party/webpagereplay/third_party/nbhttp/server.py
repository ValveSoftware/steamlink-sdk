#!/usr/bin/env python

__copyright__ = """\
Copyright (c) 2008-2009 Mark Nottingham

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""

"""
Non-Blocking HTTP Server

This library allow implementation of an HTTP/1.1 server that is "non-blocking,"
"asynchronous" and "event-driven" -- i.e., it achieves very high performance
and concurrency, so long as the application code does not block (e.g.,
upon network, disk or database access). Blocking on one request will block
the entire server.

Instantiate a Server with the following parameters:
  - host (string)
  - port (int)
  - req_start (callable)
  
req_start is called when a request starts. It must take the following arguments:
  - method (string)
  - uri (string)
  - req_hdrs (list of (name, value) tuples)
  - res_start (callable)
  - req_body_pause (callable)
and return:
  - req_body (callable)
  - req_done (callable)
    
req_body is called when part of the request body is available. It must take the 
following argument:
  - chunk (string)

req_done is called when the request is complete, whether or not it contains a 
body. It must take the following argument:
  - err (error dictionary, or None for no error)

Call req_body_pause when you want the server to temporarily stop sending the 
request body, or restart. You must provide the following argument:
  - paused (boolean; True means pause, False means unpause)
    
Call res_start when you want to start the response, and provide the following 
arguments:
  - status_code (string)
  - status_phrase (string)
  - res_hdrs (list of (name, value) tuples)
  - res_body_pause
It returns:
  - res_body (callable)
  - res_done (callable)
    
Call res_body to send part of the response body to the client. Provide the 
following parameter:
  - chunk (string)
  
Call res_done when the response is finished, and provide the 
following argument if appropriate:
  - err (error dictionary, or None for no error)
    
See the error module for the complete list of valid error dictionaries.

Where possible, errors in the request will be responded to with the appropriate
4xx HTTP status code. However, if a response has already been started, the
connection will be dropped (for example, when the request chunking or
indicated length are incorrect).
"""

__author__ = "Mark Nottingham <mnot@mnot.net>"

import os
import sys
import logging

import push_tcp
from http_common import HttpMessageHandler, \
    CLOSE, COUNTED, CHUNKED, \
    WAITING, HEADERS_DONE, \
    hop_by_hop_hdrs, \
    dummy, get_hdr

from error import ERR_HTTP_VERSION, ERR_HOST_REQ, ERR_WHITESPACE_HDR, ERR_TRANSFER_CODE

# FIXME: assure that the connection isn't closed before reading the entire req body
# TODO: filter out 100 responses to HTTP/1.0 clients that didn't ask for it.

class Server:
    "An asynchronous HTTP server."
    def __init__(self, host, port, request_handler):
        self.request_handler = request_handler
        self.server = push_tcp.create_server(host, port, self.handle_connection)
        self.log = logging.getLogger('server')
        self.log.setLevel(logging.WARNING)

        
    def handle_connection(self, tcp_conn):
        "Process a new push_tcp connection, tcp_conn."
        conn = HttpServerConnection(self.request_handler, tcp_conn)
        return conn._handle_input, conn._conn_closed, conn._res_body_pause


class HttpServerConnection(HttpMessageHandler):
    "A handler for an HTTP server connection."
    def __init__(self, request_handler, tcp_conn):
        HttpMessageHandler.__init__(self)
        self.request_handler = request_handler
        self._tcp_conn = tcp_conn
        self.req_body_cb = None
        self.req_done_cb = None
        self.method = None
        self.req_version = None
        self.connection_hdr = []
        self._res_body_pause_cb = None

    def res_start(self, status_code, status_phrase, res_hdrs, res_body_pause):
        "Start a response. Must only be called once per response."
        self._res_body_pause_cb = res_body_pause
        res_hdrs = [i for i in res_hdrs \
                    if not i[0].lower() in hop_by_hop_hdrs ]

        try:
            body_len = int(get_hdr(res_hdrs, "content-length").pop(0))
        except (IndexError, ValueError):
            body_len = None
        if body_len is not None:
            delimit = COUNTED
            res_hdrs.append(("Connection", "keep-alive"))
        elif 2.0 > self.req_version >= 1.1:
            delimit = CHUNKED
            res_hdrs.append(("Transfer-Encoding", "chunked"))
        else:
            delimit = CLOSE
            res_hdrs.append(("Connection", "close"))

        self._output_start("HTTP/1.1 %s %s" % (status_code, status_phrase), res_hdrs, delimit)
        return self.res_body, self.res_done

    def res_body(self, chunk):
        "Send part of the response body. May be called zero to many times."
        self._output_body(chunk)

    def res_done(self, err):
        """
        Signal the end of the response, whether or not there was a body. MUST be
        called exactly once for each response.

        If err is not None, it is an error dictionary (see the error module)
        indicating that an HTTP-specific (i.e., non-application) error occured
        in the generation of the response; this is useful for debugging.
        """
        self._output_end(err)

    def req_body_pause(self, paused):
        "Indicate that the server should pause (True) or unpause (False) the request."
        if self._tcp_conn and self._tcp_conn.tcp_connected:
            self._tcp_conn.pause(paused)

    # Methods called by push_tcp

    def _res_body_pause(self, paused):
        "Pause/unpause sending the response body."
        if self._res_body_pause_cb:
            self._res_body_pause_cb(paused)

    def _conn_closed(self):
        "The server connection has closed."
        if self._output_state != WAITING:
            pass # FIXME: any cleanup necessary?
#        self.pause()
#        self._queue = []
#        self.tcp_conn.handler = None
#        self.tcp_conn = None

    # Methods called by common.HttpRequestHandler

    def _output(self, chunk):
        self._tcp_conn.write(chunk)

    def _input_start(self, top_line, hdr_tuples, conn_tokens, transfer_codes, content_length):
        """
        Take the top set of headers from the input stream, parse them
        and queue the request to be processed by the application.
        """
        assert self._input_state == WAITING, "pipelining not supported" # FIXME: pipelining
        try: 
            method, _req_line = top_line.split(None, 1)
            uri, req_version = _req_line.rsplit(None, 1)
            self.req_version = float(req_version.rsplit('/', 1)[1])
        except ValueError:
            self._handle_error(ERR_HTTP_VERSION, top_line) # FIXME: more fine-grained
            raise ValueError
        if self.req_version == 1.1 and 'host' not in [t[0].lower() for t in hdr_tuples]:
            self._handle_error(ERR_HOST_REQ)
            raise ValueError
        if hdr_tuples[:1][:1][:1] in [" ", "\t"]:
            self._handle_error(ERR_WHITESPACE_HDR)
        for code in transfer_codes: # we only support 'identity' and chunked' codes
            if code not in ['identity', 'chunked']: 
                # FIXME: SHOULD also close connection
                self._handle_error(ERR_TRANSFER_CODE)
                raise ValueError
        # FIXME: MUST 400 request messages with whitespace between name and colon
        self.method = method
        self.connection_hdr = conn_tokens

        self.log.info("%s server req_start %s %s %s",
                      id(self), method, uri, self.req_version)
        self.req_body_cb, self.req_done_cb = self.request_handler(
                method, uri, hdr_tuples, self.res_start, self.req_body_pause)
        allows_body = (content_length) or (transfer_codes != [])
        return allows_body

    def _input_body(self, chunk):
        "Process a request body chunk from the wire."
        self.req_body_cb(chunk)
    
    def _input_end(self):
        "Indicate that the request body is complete."
        self.req_done_cb(None)

    def _input_error(self, err, detail=None):
        "Indicate a parsing problem with the request body."
        err['detail'] = detail
        if self._tcp_conn:
            self._tcp_conn.close()
            self._tcp_conn = None
        self.req_done_cb(err)

    def _handle_error(self, err, detail=None):
        "Handle a problem with the request by generating an appropriate response."
#        self._queue.append(ErrorHandler(status_code, status_phrase, body, self))
        assert self._output_state == WAITING
        if detail:
            err['detail'] = detail
        status_code, status_phrase = err.get('status', ('400', 'Bad Request'))
        hdrs = [
            ('Content-Type', 'text/plain'),
        ]
        body = err['desc']
        if err.has_key('detail'):
            body += " (%s)" % err['detail']
        self.res_start(status_code, status_phrase, hdrs, dummy)
        self.res_body(body)
        self.res_done()

    
def test_handler(method, uri, hdrs, res_start, req_pause):
    """
    An extremely simple (and limited) server request_handler.
    """
    code = "200"
    phrase = "OK"
    res_hdrs = [('Content-Type', 'text/plain')]
    res_body, res_done = res_start(code, phrase, res_hdrs, dummy)
    res_body('foo!')
    res_done(None)
    return dummy, dummy
    
if __name__ == "__main__":
    sys.stderr.write("PID: %s\n" % os.getpid())
    h, p = '127.0.0.1', int(sys.argv[1])
    server = Server(h, p, test_handler)
    push_tcp.run()
