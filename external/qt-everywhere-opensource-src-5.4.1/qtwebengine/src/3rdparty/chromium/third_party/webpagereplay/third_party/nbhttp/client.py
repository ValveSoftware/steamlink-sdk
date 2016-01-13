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
Non-Blocking HTTP Client

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

__author__ = "Mark Nottingham <mnot@mnot.net>"

from urlparse import urlsplit, urlunsplit

import push_tcp
from http_common import HttpMessageHandler, \
    CLOSE, COUNTED, NOBODY, \
    WAITING, HEADERS_DONE, \
    idempotent_methods, no_body_status, hop_by_hop_hdrs, \
    dummy, get_hdr
from error import ERR_URL, ERR_CONNECT, ERR_LEN_REQ, ERR_READ_TIMEOUT, ERR_HTTP_VERSION

req_remove_hdrs = hop_by_hop_hdrs + ['host']

# TODO: proxy support
# TODO: next-hop version cache for Expect/Continue, etc.

class Client(HttpMessageHandler):
    "An asynchronous HTTP client."
    connect_timeout = None
    read_timeout = None
    retry_limit = 2

    def __init__(self, res_start_cb):
        HttpMessageHandler.__init__(self)
        self.res_start_cb = res_start_cb
        self.res_body_cb = None
        self.res_done_cb = None
        self.method = None
        self.uri = None
        self.req_hdrs = []
        self._tcp_conn = None
        self._conn_reusable = False
        self._req_body_pause_cb = None
        self._retries = 0
        self._timeout_ev = None
        self._output_buffer = []

    def req_start(self, method, uri, req_hdrs, req_body_pause):
        """
        Start a request to uri using method, where 
        req_hdrs is a list of (field_name, field_value) for
        the request headers.
        
        Returns a (req_body, req_done) tuple.
        """
        self._req_body_pause_cb = req_body_pause
        req_hdrs = [i for i in req_hdrs \
            if not i[0].lower() in req_remove_hdrs]
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
        if path == "":
            path = "/"
        uri = urlunsplit(('', '', path, query, ''))
        self.method, self.uri, self.req_hdrs = method, uri, req_hdrs
        self.req_hdrs.append(("Host", host)) # FIXME: port
        self.req_hdrs.append(("Connection", "keep-alive"))
        try:
            body_len = int(get_hdr(req_hdrs, "content-length").pop(0))
            delimit = COUNTED
        except (IndexError, ValueError):
            body_len = None
            delimit = NOBODY
        self._output_start("%s %s HTTP/1.1" % (self.method, self.uri), self.req_hdrs, delimit)
        _idle_pool.attach(host, port, self._handle_connect, self._handle_connect_error, self.connect_timeout)
        return self.req_body, self.req_done
    # TODO: if we sent Expect: 100-continue, don't wait forever (i.e., schedule something)

    def req_body(self, chunk):
        "Send part of the request body. May be called zero to many times."
        # FIXME: self._handle_error(ERR_LEN_REQ)
        self._output_body(chunk)
        
    def req_done(self, err):
        """
        Signal the end of the request, whether or not there was a body. MUST be
        called exactly once for each request. 
        
        If err is not None, it is an error dictionary (see the error module)
        indicating that an HTTP-specific (i.e., non-application) error occurred
        while satisfying the request; this is useful for debugging.
        """
        self._output_end(err)            

    def res_body_pause(self, paused):
        "Temporarily stop / restart sending the response body."
        if self._tcp_conn and self._tcp_conn.tcp_connected:
            self._tcp_conn.pause(paused)
        
    # Methods called by push_tcp

    def _handle_connect(self, tcp_conn):
        "The connection has succeeded."
        self._tcp_conn = tcp_conn
        self._output("") # kick the output buffer
        if self.read_timeout:
            self._timeout_ev = push_tcp.schedule(
                self.read_timeout, self._handle_error, ERR_READ_TIMEOUT, 'connect')
        return self._handle_input, self._conn_closed, self._req_body_pause

    def _handle_connect_error(self, host, port, err):
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
        if self.read_timeout:
            self._timeout_ev.delete()
        if self._input_buffer:
            self._handle_input("")
        if self._input_delimit == CLOSE:
            self._input_end()
        elif self._output_state == WAITING:
            # nothing has happened yet. TODO: will get more complex with pipelining.
            if self._retries < self.retry_limit:
                self._retry()
            else:
                self._handle_error(ERR_CONNECT, "Server closed the connection.")
        elif self.method in idempotent_methods and \
          self._retries < self.retry_limit and \
          self._input_state == WAITING:
            self._retry()
        elif self._input_state == WAITING: # haven't completed headers yet
            self._handle_error(ERR_CONNECT, "Server closed the connection.")
        else:
            self._input_error(ERR_CONNECT, "Server closed the connection.")

    def _retry(self):
        "Retry the request."
        if self._timeout_ev:
            self._timeout_ev.delete()
        self._retries += 1
        _idle_pool.attach(self._tcp_conn.host, self._tcp_conn.port, 
            self._handle_connect, self._handle_connect_error, self.connect_timeout)

    def _req_body_pause(self, paused):
        "The client needs the application to pause/unpause the request body."
        if self._req_body_pause_cb:
            self._req_body_pause_cb(paused)

    # Methods called by common.HttpMessageHandler

    def _input_start(self, top_line, hdr_tuples, conn_tokens, transfer_codes, content_length):
        """
        Take the top set of headers from the input stream, parse them
        and queue the request to be processed by the application.
        """
        if self.read_timeout:
            self._timeout_ev.delete()
        try: 
            res_version, status_txt = top_line.split(None, 1)
            res_version = float(res_version.rsplit('/', 1)[1])
            # TODO: check that the protocol is HTTP
        except (ValueError, IndexError):
            self._handle_error(ERR_HTTP_VERSION, top_line)
            raise ValueError
        try:
            res_code, res_phrase = status_txt.split(None, 1)
        except ValueError:
            res_code = status_txt.rstrip()
            res_phrase = ""
        if 'close' not in conn_tokens:
            if (res_version == 1.0 and 'keep-alive' in conn_tokens) or \
                res_version > 1.0:
                self._conn_reusable = True
        if self.read_timeout:
            self._timeout_ev = push_tcp.schedule(
                 self.read_timeout, self._input_error, ERR_READ_TIMEOUT, 'start')
        self.res_body_cb, self.res_done_cb = self.res_start_cb(
            res_version, res_code, res_phrase, hdr_tuples, self.res_body_pause)
        allows_body = (res_code not in no_body_status) or (self.method == "HEAD")
        return allows_body 

    def _input_body(self, chunk):
        "Process a response body chunk from the wire."
        if self.read_timeout:
            self._timeout_ev.delete()
        self.res_body_cb(chunk)
        if self.read_timeout:
            self._timeout_ev = push_tcp.schedule(
                 self.read_timeout, self._input_error, ERR_READ_TIMEOUT, 'body')

    def _input_end(self):
        "Indicate that the response body is complete."
        if self.read_timeout:
            self._timeout_ev.delete()
        if self._tcp_conn:
            if self._tcp_conn.tcp_connected and self._conn_reusable:
                # Note that we don't reset read_cb; if more bytes come in before
                # the next request, we'll still get them.
                _idle_pool.release(self._tcp_conn)
            else:
                self._tcp_conn.close()
                self._tcp_conn = None
        self.res_done_cb(None)

    def _input_error(self, err, detail=None):
        "Indicate a parsing problem with the response body."
        if self.read_timeout:
            self._timeout_ev.delete()
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
        if self._timeout_ev:
            self._timeout_ev.delete()
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


class _HttpConnectionPool:
    "A pool of idle TCP connections for use by the client."
    _conns = {}

    def attach(self, host, port, handle_connect, handle_connect_error, connect_timeout):
        "Find an idle connection for (host, port), or create a new one."
        while True:
            try:
                tcp_conn = self._conns[(host, port)].pop()
            except (IndexError, KeyError):
                push_tcp.create_client(host, port, handle_connect, handle_connect_error, 
                                       connect_timeout)
                break        
            if tcp_conn.tcp_connected:
                tcp_conn.read_cb, tcp_conn.close_cb, tcp_conn.pause_cb = \
                    handle_connect(tcp_conn)
                break
        
    def release(self, tcp_conn):
        "Add an idle connection back to the pool."
        if tcp_conn.tcp_connected:
            def idle_close():
                "Remove the connection from the pool when it closes."
                try:
                    self._conns[(tcp_conn.host, tcp_conn.port)].remove(tcp_conn)
                except ValueError:
                    pass
            tcp_conn.close_cb = idle_close
            if not self._conns.has_key((tcp_conn.host, tcp_conn.port)):
                self._conns[(tcp_conn.host, tcp_conn.port)] = [tcp_conn]
            else:
                self._conns[(tcp_conn.host, tcp_conn.port)].append(tcp_conn)

_idle_pool = _HttpConnectionPool()


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
    c = Client(printer)
    req_body_write, req_done = c.req_start("GET", request_uri, [], dummy)
    req_done(None)
    push_tcp.run()
            
if __name__ == "__main__":
    import sys
    test_client(sys.argv[1])
