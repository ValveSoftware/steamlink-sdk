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
Non-Blocking SPDY Client

This library allow implementation of an HTTP/1.1 client that is "non-blocking,"
"asynchronous" and "event-driven" -- i.e., it achieves very high performance
and concurrency, so long as the application code does not block (e.g.,
upon network, disk or database access). Blocking on one response will block
the entire client.

Instantiate a Client with the following parameter:
  - res_start (callable)
  
Call req_start on the Client instance to begin a request. It takes the following 
arguments:
  - method (string)
  - uri (string)
  - req_hdrs (list of (name, value) tuples)
  - req_body_pause (callable)
and returns:
  - req_body (callable)
  - req_done (callable)
    
Call req_body to send part of the request body. It takes the following 
argument:
  - chunk (string)

Call req_done when the request is complete, whether or not it contains a 
body. It takes the following argument:
  - err (error dictionary, or None for no error)

req_body_pause is called when the client needs you  to temporarily stop sending 
the request body, or restart. It must take the following argument:
  - paused (boolean; True means pause, False means unpause)
    
res_start is called to start the response, and must take the following 
arguments:
  - status_code (string)
  - status_phrase (string)
  - res_hdrs (list of (name, value) tuples)
  - res_body_pause
It must return:
  - res_body (callable)
  - res_done (callable)
    
res_body is called when part of the response body is available. It must accept
the following parameter:
  - chunk (string)
  
res_done is called when the response is finished, and must accept the 
following argument:
  - err (error dictionary, or None if no error)
    
See the error module for the complete list of valid error dictionaries.

Where possible, errors in the response will be indicated with the appropriate
5xx HTTP status code (i.e., by calling res_start, res_body and res_done with
an error dictionary). However, if a response has already been started, the
connection will be dropped (for example, when the response chunking or
indicated length are incorrect). In these cases, res_done will still be called
with the appropriate error dictionary.
"""

# FIXME: update docs for API change (move res_start)

__author__ = "Mark Nottingham <mnot@mnot.net>"

from urlparse import urlsplit

import push_tcp
from error import ERR_CONNECT, ERR_URL
from http_common import WAITING, \
    hop_by_hop_hdrs, dummy, get_hdr
from spdy_common import SpdyMessageHandler, CTL_SYN_STREAM, FLAG_NONE, FLAG_FIN

req_remove_hdrs = hop_by_hop_hdrs + ['host']

# TODO: read timeout support (needs to be in push_tcp?)

class SpdyClient(SpdyMessageHandler):
    "An asynchronous SPDY client."
    proxy = None
    connect_timeout = None

    def req_start(self, method, uri, req_hdrs, res_start_cb, req_body_pause):
        """
        Start a request to uri using method, where
        req_hdrs is a list of (field_name, field_value) for
        the request headers.

        Returns a (req_body, req_done) tuple.
        """
        if self.proxy:
            (host, port) = self.proxy
        else: # find out where to connect to the hard way
            (scheme, authority, path, query, fragment) = urlsplit(uri)
            if scheme.lower() != 'http':
                self._handle_error(ERR_URL, "Only HTTP URLs are supported")
                return dummy, dummy
            if "@" in authority:
                userinfo, authority = authority.split("@", 1)
            if ":" in authority:
                host, port = authority.rsplit(":", 1)
                try:
                    port = int(port)
                except ValueError:
                    self._handle_error(ERR_URL, "Non-integer port in URL")
                    return dummy, dummy
            else:
                host, port = authority, 80 
        conn = _conn_pool.get(host, port, SpdyConnection, self.connect_timeout)
        return conn.req_start(method, uri, req_hdrs, res_start_cb, req_body_pause)
 

class SpdyConnection(SpdyMessageHandler):
    "A SPDY connection."

    def __init__(self, log=None):
        SpdyMessageHandler.__init__(self)
        self.log = log or dummy
        self._tcp_conn = None
        self._req_body_pause_cb = None  # FIXME: re-think pausing
        self._streams = {}
        self._output_buffer = []
        self._highest_stream_id = -1

    def req_start(self, method, uri, req_hdrs, res_start_cb, req_body_pause):
        req_hdrs = [i for i in req_hdrs if not i[0].lower() in req_remove_hdrs]
        req_hdrs.append(('method', method))
        req_hdrs.append(('url', uri))
        req_hdrs.append(('version', 'HTTP/1.1'))
        self._highest_stream_id += 2 # TODO: check to make sure it's not too high.. what then?
        stream_id = self._highest_stream_id
        self._streams[stream_id] = [res_start_cb, req_body_pause, None, None]
        self._output(self._ser_syn_frame(CTL_SYN_STREAM, FLAG_NONE, stream_id, req_hdrs))
        def req_body(*args):
            return self.req_body(stream_id, *args)
        def req_done(*args):
            return self.req_done(stream_id, *args)
        return req_body, req_done

    def req_body(self, stream_id, chunk):
        "Send part of the request body. May be called zero to many times."
        self._output(self._ser_data_frame(stream_id, FLAG_NONE, chunk))
        
    def req_done(self, stream_id, err):
        """
        Signal the end of the request, whether or not there was a body. MUST be
        called exactly once for each request. 
        
        If err is not None, it is an error dictionary (see the error module)
        indicating that an HTTP-specific (i.e., non-application) error occurred
        while satisfying the request; this is useful for debugging.
        """
        self._output(self._ser_data_frame(stream_id, FLAG_FIN, ""))
        # TODO: delete stream after checking that input side is half-closed

    def res_body_pause(self, paused):
        "Temporarily stop / restart sending the response body."
        if self._tcp_conn and self._tcp_conn.tcp_connected:
            self._tcp_conn.pause(paused)
        
    # Methods called by push_tcp

    def handle_connect(self, tcp_conn):
        "The connection has succeeded."
        self._tcp_conn = tcp_conn
        self._output("") # kick the output buffer
        return self._handle_input, self._conn_closed, self._req_body_pause

    def handle_connect_error(self, host, port, err):
        "The connection has failed."
        import os, types, socket
        if type(err) == types.IntType:
            err = os.strerror(err)
        elif isinstance(err, socket.error):
            err = err[1]
        else:
            err = str(err)
        self._handle_error(ERR_CONNECT, err)

    def _conn_closed(self):
        "The server closed the connection."
        if self._input_buffer:
            self._handle_input("")
        # TODO: figure out what to do with existing conns

    def _req_body_pause(self, paused):
        "The client needs the application to pause/unpause the request body."
        # FIXME: figure out how pausing should work.
        if self._req_body_pause_cb:
            self._req_body_pause_cb(paused)

    # Methods called by common.SpdyMessageHandler

    def _input_start(self, stream_id, stream_priority, hdr_tuples):
        """
        Take the top set of headers from the input stream, parse them
        and queue the request to be processed by the application.
        """
        status = get_hdr(hdr_tuples, 'status')[0]
        try:
            res_code, res_phrase = status.split(None, 1)
        except ValueError:
            res_code = status.rstrip()
            res_phrase = ""
        self._streams[stream_id][1:2] = self._streams[stream_id][0](
            "HTTP/1.1", res_code, res_phrase, hdr_tuples, self.res_body_pause)

    def _input_body(self, stream_id, chunk):
        "Process a response body chunk from the wire."
        self._streams[stream_id][1](chunk)

    def _input_end(self, stream_id):
        "Indicate that the response body is complete."
        self._streams[stream_id][2](None)
        # TODO: delete stream if output side is half-closed.

    def _input_error(self, err, detail=None):
        "Indicate a parsing problem with the response body."
        if self._tcp_conn:
            self._tcp_conn.close()
            self._tcp_conn = None
        err['detail'] = detail
        self.res_done_cb(err)

    def _output(self, chunk):
        self._output_buffer.append(chunk)
        if self._tcp_conn and self._tcp_conn.tcp_connected:
            self._tcp_conn.write("".join(self._output_buffer))
            self._output_buffer = []

    # misc

    def _handle_error(self, err, detail=None):
        "Handle a problem with the request by generating an appropriate response."
        assert self._input_state == WAITING
        if self._tcp_conn:
            self._tcp_conn.close()
            self._tcp_conn = None
        if detail:
            err['detail'] = detail
        status_code, status_phrase = err.get('status', ('504', 'Gateway Timeout'))
        hdrs = [
            ('Content-Type', 'text/plain'),
            ('Connection', 'close'),
        ]
        body = err['desc']
        if err.has_key('detail'):
            body += " (%s)" % err['detail']
        res_body_cb, res_done_cb = self.res_start_cb(
              "1.1", status_code, status_phrase, hdrs, dummy)
        res_body_cb(str(body))
        push_tcp.schedule(0, res_done_cb, err)


class _SpdyConnectionPool:
    "A pool of open connections for use by the client."
    _conns = {}

    def get(self, host, port, connection_handler, connect_timeout):
        "Find a connection for (host, port), or create a new one."
        try:
            conn = self._conns[(host, port)]
        except KeyError:
            conn = connection_handler()
            push_tcp.create_client(
                host, port,
                conn.handle_connect, conn.handle_connect_error,
                connect_timeout
            )
            self._conns[(host, port)] = conn
        return conn

    #TODO: remove conns from _conns when they close

_conn_pool = _SpdyConnectionPool()


def test_client(request_uri):
    "A simple demonstration of a client."
    def printer(version, status, phrase, headers, res_pause):
        "Print the response headers."
        print "HTTP/%s" % version, status, phrase
        print "\n".join(["%s:%s" % header for header in headers])
        print
        def body(chunk):
            print chunk
        def done(err):
            if err:
                print "*** ERROR: %s (%s)" % (err['desc'], err['detail'])
            push_tcp.stop()
        return body, done
    c = SpdyClient()
    req_body_write, req_done = c.req_start("GET", request_uri, [], printer, dummy)
    req_done(None)
    push_tcp.run()
            
if __name__ == "__main__":
    import sys
    test_client(sys.argv[1])
