#!/usr/bin/env python
import logging

"""
shared SPDY infrastructure

This module contains utility functions for nbhttp and a base class
for the SPDY-specific portions of the client and server.
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

import struct

compressed_hdrs = True
try:
  import c_zlib
except TypeError:
  # c_zlib loads "libz". However, that fails on Windows.
  compressed_hdrs = False
  import sys
  print >>sys.stderr, (
      'WARNING: sdpy_common: import c_zlib failed. Using uncompressed headers.')

from http_common import dummy

# There is a null character ('\0') at the end of the dictionary. The '\0' might
# be removed in future spdy versions.
dictionary = \
"optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-" \
"languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi" \
"f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser" \
"-agent10010120020120220320420520630030130230330430530630740040140240340440" \
"5406407408409410411412413414415416417500501502503504505accept-rangesageeta" \
"glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic" \
"ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran" \
"sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati" \
"oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo" \
"ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe" \
"pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic" \
"ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1" \
".1statusversionurl\0"

# states
WAITING, READING_FRAME_DATA = 1, 2

# frame types
DATA_FRAME = 0x00
# Control frame, version number is 2.
CTL_FRM = 0x8002
CTL_SYN_STREAM = 0x01
CTL_SYN_REPLY = 0x02
CTL_RST_STREAM = 0x03
CTL_SETTINGS = 0x04
CTL_NOOP = 0x05
CTL_PING = 0x06
CTL_GOAWAY = 0x07

# flags
FLAG_NONE = 0x00
FLAG_FIN = 0x01
FLAG_UNIDIRECTIONAL = 0x02

PRIORITY_MASK = 0xc0
STREAM_MASK = 0x7fffffff

class SpdyMessageHandler:
    """
    This is a base class for something that has to parse and/or serialise
    SPDY messages, request or response.

    For parsing, it expects you to override _input_start, _input_body and
    _input_end, and call _handle_input when you get bytes from the network.

    For serialising, it expects you to override _output.
    """

    def __init__(self):
        self.log = lambda a:a
        self._input_buffer = ""
        self._input_state = WAITING
        self._input_frame_type = None
        self._input_flags = None
        self._input_stream_id = None
        self._input_frame_len = 0
        if compressed_hdrs:
            self._compress = c_zlib.Compressor(-1, dictionary)
            self._decompress = c_zlib.Decompressor(dictionary)
        else:
            self._compress = dummy
            self._decompress = dummy

    # input-related methods
    def _input_start(self, stream_id, hdr_tuples):
        """
        Take the top set of headers from a new request and queue it
        to be processed by the application.
        """
        raise NotImplementedError

    def _input_body(self, stream_id, chunk):
        "Process a body chunk from the wire."
        raise NotImplementedError

    def _input_end(self, stream_id):
        "Indicate that the response body is complete."
        raise NotImplementedError

    def _input_error(self, stream_id, err, detail=None):
        "Indicate a parsing problem with the body."
        raise NotImplementedError

    def _handle_input(self, data):
        """
        Given a chunk of input, figure out what state we're in and handle it,
        making the appropriate calls.
        """
        # TODO: look into reading/writing directly from the socket buffer with struct.pack_into / unpack_from.
        if self._input_buffer != "":
            data = self._input_buffer + data # will need to move to a list if writev comes around
            self._input_buffer = ""
        if self._input_state == WAITING: # waiting for a complete frame header
            if len(data) >= 8:
                (d1, self._input_flags, d2, d3) = struct.unpack("!IBBH", data[:8])
                if d1 >> 31 & 0x01: # control frame
                    version = ( d1 >> 16 ) & 0x7fff # TODO: check version
                    # FIXME: we use 0x00 internally to indicate data frame
                    self._input_frame_type = d1 & 0x0000ffff
                    self._input_stream_id = None
                else: # data frame
                    self._input_frame_type = DATA_FRAME
                    self._input_stream_id = d1 & STREAM_MASK
                self._input_frame_len = (( d2 << 16 ) + d3)
                self._input_state = READING_FRAME_DATA
                self._handle_input(data[8:])
            else:
                self._input_buffer = data
        elif self._input_state == READING_FRAME_DATA:
            if len(data) >= self._input_frame_len:
                frame_data = data[:self._input_frame_len]
                rest = data[self._input_frame_len:]
                if self._input_frame_type == DATA_FRAME:
                    self._input_body(self._input_stream_id, frame_data)
                    stream_id = self._input_stream_id # for FLAG_FIN below
                elif self._input_frame_type in [CTL_SYN_STREAM, CTL_SYN_REPLY]:
                    stream_id = struct.unpack("!I", frame_data[:4])[0] & STREAM_MASK # FIXME: what if they lied about the frame len?
                    stream_priority = (struct.unpack("!b", frame_data[8:9])[0] & PRIORITY_MASK) >> 6
                    tuple_pos = 4 + 2
                    if self._input_frame_type == CTL_SYN_STREAM:
                      associated_stream_id = struct.unpack("!I", frame_data[4:8])[0]
                      tuple_pos += 4
                    hdr_tuples = self._parse_hdrs(frame_data[tuple_pos:]) or self._input_error(stream_id, 1) # FIXME: proper error here
                    # FIXME: expose pri
                    self._input_start(stream_id, stream_priority, hdr_tuples)
                elif self._input_frame_type == CTL_RST_STREAM:
                    stream_id = struct.unpack("!I", frame_data[:4])[0] & STREAM_MASK
                    self._input_end(stream_id)
                elif self._input_frame_type == CTL_SETTINGS:
                    pass # FIXME
                elif self._input_frame_type == CTL_NOOP:
                    pass
                elif self._input_frame_type == CTL_PING:
                    pass # FIXME
                elif self._input_frame_type == CTL_GOAWAY:
                    pass # FIXME
                else: # unknown frame type
                    raise ValueError, "Unknown frame type" # FIXME: don't puke
                if self._input_flags & FLAG_FIN: # FIXME: invalid on CTL_RST_STREAM
                    self._input_end(stream_id)
                self._input_state = WAITING
                if rest:
                    self._handle_input(rest)
            else: # don't have complete frame yet
                self._input_buffer = data
        else:
            raise Exception, "Unknown input state %s" % self._input_state

    def _parse_hdrs(self, data):
        "Given a control frame data block, return a list of (name, value) tuples."
        # TODO: separate null-delimited into separate instances
        if self._decompress != dummy:
          data = self._decompress(data) # FIXME: catch errors
        cursor = 2
        (num_hdrs,) = struct.unpack("!h", data[:cursor]) # FIXME: catch errors
        hdrs = []
        while cursor < len(data):
            try:
                (name_len,) = struct.unpack("!h", data[cursor:cursor+2]) # FIXME: catch errors
                cursor += 2
                name = data[cursor:cursor+name_len] # FIXME: catch errors
                cursor += name_len
            except IndexError:
                raise
            except struct.error:
                raise
            try:
                (val_len,) = struct.unpack("!h", data[cursor:cursor+2]) # FIXME: catch errors
                cursor += 2
                value = data[cursor:cursor+val_len] # FIXME: catch errors
                cursor += val_len
            except IndexError:
                raise
            except struct.error:
                print len(data), cursor, data # FIXME
                raise
            hdrs.append((name, value))
        return hdrs

    ### output-related methods

    def _output(self, out):
        raise NotImplementedError

    def _handle_error(self, err):
        raise NotImplementedError

    def _ser_syn_frame(self, type, flags, stream_id, hdr_tuples):
        "Returns a SPDY SYN_[STREAM|REPLY] frame."
        #print "frame: " + str(self._ser_hdrs(hdr_tuples))
        hdrs = self._ser_hdrs(hdr_tuples)
        if self._compress != dummy:
          hdrs = self._compress(hdrs)
        if (type == CTL_SYN_STREAM):
          data = struct.pack("!IIH%ds" % len(hdrs),
              STREAM_MASK & stream_id,
              0x00,  # associated stream id
              0x00,  # unused
              hdrs
           )
        else:
          data = struct.pack("!IH%ds" % len(hdrs),
              STREAM_MASK & stream_id,
              0x00,  # unused
              hdrs
          )
        return self._ser_ctl_frame(type, flags, data)

    @staticmethod
    def _ser_ctl_frame(type, flags, data):
        "Returns a SPDY control frame."
        # TODO: check that data len doesn't overflow
        return struct.pack("!HHI%ds" % len(data),
            CTL_FRM,
            type,
            (flags << 24) + len(data),
            data
        )

    @staticmethod
    def _ser_data_frame(stream_id, flags, data):
        "Returns a SPDY data frame."
        # TODO: check that stream_id and data len don't overflow
        return struct.pack("!II%ds" % len(data),
            STREAM_MASK & stream_id,
            (flags << 24) + len(data),
            data
        )

    @staticmethod
    def _ser_hdrs(hdr_tuples):
        "Returns a SPDY header block from a list of (name, value) tuples."
        # TODO: collapse dups into null-delimited
        hdr_tuples.sort() # required by Chromium
        fmt = ["!H"]
        # filter junk from hdr_tuples
        broken = 0
        for (n,v) in hdr_tuples:
            if len(n) == 0 or len(v) == 0:
                broken += 1
        args = [len(hdr_tuples) - broken]
        for (n,v) in hdr_tuples:
            if len(n) == 0 or len(v) == 0:
                continue
            # TODO: check for overflowing n, v lengths
            fmt.append("H%dsH%ds" % (len(n), len(v)))
            args.extend([len(n), n, len(v), v])
        return struct.pack("".join(fmt), *args)
