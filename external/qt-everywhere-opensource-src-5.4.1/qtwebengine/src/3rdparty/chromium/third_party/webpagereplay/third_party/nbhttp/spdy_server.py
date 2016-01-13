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
Non-Blocking SPDY Server

This library allow implementation of an SPDY server that is "non-blocking,"
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
from spdy_common import SpdyMessageHandler, CTL_SYN_REPLY, FLAG_NONE, FLAG_FIN
from http_common import get_hdr, dummy

# FIXME: assure that the connection isn't closed before reading the entire req body

class SpdyServer:
    "An asynchronous SPDY server."
    def __init__(self,
                 host,
                 port,
                 use_ssl,
                 certfile,
                 keyfile,
                 request_handler,
                 log=None):
        self.request_handler = request_handler
        self.use_ssl = use_ssl
        self.server = push_tcp.create_server(host, port, use_ssl, certfile, keyfile, self.handle_connection)
        self.log = log

    def handle_connection(self, tcp_conn):
        "Process a new push_tcp connection, tcp_conn."
        conn = SpdyServerConnection(self.request_handler, tcp_conn, self.log)
        return conn._handle_input, conn._conn_closed, conn._res_body_pause


class SpdyServerConnection(SpdyMessageHandler):
    "A handler for a SPDY server connection."
    def __init__(self, request_handler, tcp_conn, log=None):
        SpdyMessageHandler.__init__(self)
        self.request_handler = request_handler
        self._tcp_conn = tcp_conn
        self.log = log or dummy
        self._streams = {}
        self._res_body_pause_cb = False
        self.log.debug("new connection %s" % id(self))
        # SPDY has 4 priorities.  write_queue is an array of [0..3], one for each priority.
        self.write_queue = []
        for index in range(0,4):
            self.write_queue.append([])
        # Write pending when a write to the output queue has been scheduled
        self.write_pending = False

    def res_start(self, stream_id, stream_priority, status_code, status_phrase, res_hdrs, res_body_pause):
        "Start a response. Must only be called once per response."
        self.log.debug("res_start %s" % stream_id)
        self._res_body_pause_cb = res_body_pause
        res_hdrs.append(('status', "%s %s" % (status_code, status_phrase)))
        # TODO: hop-by-hop headers?
        self._queue_frame(stream_priority, self._ser_syn_frame(CTL_SYN_REPLY, FLAG_NONE, stream_id, res_hdrs))
        def res_body(*args):
            return self.res_body(stream_id, stream_priority, *args)
        def res_done(*args):
            return self.res_done(stream_id, stream_priority, *args)
        return res_body, res_done

    def res_body(self, stream_id, stream_priority, chunk):
        "Send part of the response body. May be called zero to many times."
        if chunk:
            do_chunking = True
            if stream_priority == 0:
                do_chunking = True
            if do_chunking:
                kMaxChunkSize = 1460 * 4
                start_pos = 0
                chunk_size = len(chunk)
                while start_pos < chunk_size:
                    size = min(chunk_size - start_pos, kMaxChunkSize)
                    self._queue_frame(stream_priority, self._ser_data_frame(stream_id, FLAG_NONE, chunk[start_pos:start_pos + size]))
                    start_pos += size
            else:
                self._queue_frame(stream_priority, self._ser_data_frame(stream_id, FLAG_NONE, chunk))

    def res_done(self, stream_id, stream_priority, err):
        """
        Signal the end of the response, whether or not there was a body. MUST be
        called exactly once for each response.

        If err is not None, it is an error dictionary (see the error module)
        indicating that an HTTP-specific (i.e., non-application) error occured
        in the generation of the response; this is useful for debugging.
        """
        self._queue_frame(stream_priority, self._ser_data_frame(stream_id, FLAG_FIN, ""))
        # TODO: delete stream after checking that input side is half-closed

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
        pass # FIXME: any cleanup necessary?
#        self.pause()
#        self.tcp_conn.handler = None
#        self.tcp_conn = None

    def _has_write_data(self):
        for index in range(0, 4):
            if len(self.write_queue[index]) > 0:
                return True
        return False

    def _write_frame_callback(self):
        self.write_pending = False

        # Find the highest priority data chunk and send it.
        for index in range(0, 4):
            if len(self.write_queue[index]) > 0:
                data = self.write_queue[index][0]
                self.write_queue[index] = self.write_queue[index][1:]
                self._output(data)
                break
        if self._has_write_data():
            self._schedule_write()
        
    def _schedule_write(self):
        # We only need one write scheduled at a time.
        if not self.write_pending:
            push_tcp.schedule(0, self._write_frame_callback)
            self.write_pending = True

    def _queue_frame(self, priority, chunk):
        self.write_queue[priority].append(chunk)
        self._schedule_write()

    # Methods called by common.SpdyRequestHandler

    def _output(self, chunk):
        if self._tcp_conn:
            self._tcp_conn.write(chunk)

    def _input_start(self, stream_id, stream_priority, hdr_tuples):
        self.log.debug("request start %s %s" % (stream_id, hdr_tuples))
        method = get_hdr(hdr_tuples, 'method')[0] # FIXME: error handling
        uri = get_hdr(hdr_tuples, 'url')[0] # FIXME: error handling
        assert not self._streams.has_key(stream_id) # FIXME
        def res_start(*args):
            return self.res_start(stream_id, stream_priority, *args)
        # TODO: sanity checks / catch errors from requst_handler
        self._streams[stream_id] = self.request_handler(
            method, uri, hdr_tuples, res_start, self.req_body_pause)

    def _input_body(self, stream_id, chunk):
        "Process a request body chunk from the wire."
        if self._streams.has_key(stream_id):
            self._streams[stream_id][0](chunk)

    def _input_end(self, stream_id):
        "Indicate that the request body is complete."
        if self._streams.has_key(stream_id):
            self._streams[stream_id][1](None)
        # TODO: delete stream if output side is half-closed.

    def _input_error(self, stream_id, err, detail=None):
        "Indicate a parsing problem with the request body."
        # FIXME: rework after fixing spdy_common
        err['detail'] = detail
        if self._tcp_conn:
            self._tcp_conn.close()
            self._tcp_conn = None
        if self._streams.has_key(stream_id):
            self._streams[stream_id][1](err)

    # TODO: re-evaluate if this is necessary in SPDY
    def _handle_error(self, err, detail=None):
        "Handle a problem with the request by generating an appropriate response."
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
    res_hdrs = [('Content-Type', 'text/plain'), ('version', 'HTTP/1.1')]
    res_body, res_done = res_start(code, phrase, res_hdrs, dummy)
    res_body('This is SPDY.')
    res_done(None)
    return dummy, dummy


if __name__ == "__main__":
    logging.basicConfig()
    log = logging.getLogger('server')
    log.setLevel(logging.INFO)
    log.info("PID: %s\n" % os.getpid())
    h, p = '127.0.0.1', int(sys.argv[1])
    server = SpdyServer(h, p, test_handler, log)
    push_tcp.run()
