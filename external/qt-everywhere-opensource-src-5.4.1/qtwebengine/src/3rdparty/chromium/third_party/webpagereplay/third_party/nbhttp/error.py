#!/usr/bin/env python

"""
errors
"""

__author__ = "Mark Nottingham <mnot@mnot.net>"
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

# General parsing errors

ERR_CHUNK = {
    'desc': "Chunked encoding error",
}
ERR_EXTRA_DATA = {
    'desc': "Extra data received",
}

ERR_BODY_FORBIDDEN = {
    'desc': "This message does not allow a body",
}

ERR_HTTP_VERSION = {
    'desc': "Unrecognised HTTP version",  # FIXME: more specific status
}

ERR_READ_TIMEOUT = {
    'desc': "Read timeout", 
}

ERR_TRANSFER_CODE = {
    'desc': "Unknown request transfer coding",
    'status': ("501", "Not Implemented"),
}

ERR_WHITESPACE_HDR = {
    'desc': "Whitespace between request-line and first header",
    'status': ("400", "Bad Request"),
}

# client-specific errors

ERR_URL = {
    'desc': "Unsupported or invalid URI",
    'status': ("400", "Bad Request"),
}
ERR_LEN_REQ = {
    'desc': "Content-Length required",
    'status': ("411", "Length Required"),
}

ERR_CONNECT = {
    'desc': "Connection closed",
    'status': ("504", "Gateway Timeout"),
}

# server-specific errors

ERR_HOST_REQ = {
    'desc': "Host header required",
}
