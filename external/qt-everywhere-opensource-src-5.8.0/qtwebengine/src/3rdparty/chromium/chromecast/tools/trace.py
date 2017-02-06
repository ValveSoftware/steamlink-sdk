#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script was originally written by Alok Priyadarshi (alokp@)
# with some minor local modifications.

import contextlib
import json
import optparse
import os
import sys
import websocket

from tracinglib import TracingBackend, TracingClient

@contextlib.contextmanager
def Connect(device_ip, devtools_port):
  backend = TracingBackend()
  try:
    backend.Connect(device_ip, devtools_port)
    yield backend
  finally:
    backend.Disconnect()


def DumpTrace(trace, options):
  filepath = os.path.expanduser(options.output) if options.output \
      else os.path.join(os.getcwd(), 'trace.json')

  dirname = os.path.dirname(filepath)
  if dirname:
    if not os.path.exists(dirname):
      os.makedirs(dirname)
  else:
    filepath = os.path.join(os.getcwd(), filepath)

  with open(filepath, 'w') as f:
    json.dump(trace, f)
  return filepath


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from any running instance of Chrome.')
  parser.add_option(
      '-v', '--verbose', help='Verbose logging.', action='store_true')
  parser.add_option(
      '-p', '--port', help='Remote debugging port.', type='int', default=9222)
  parser.add_option(
      '-d', '--device', help='Device ip address.', type='string',
      default='127.0.0.1')

  tracing_opts = optparse.OptionGroup(parser, 'Tracing options')
  tracing_opts.add_option(
      '-c', '--category-filter',
      help='Apply filter to control what category groups should be traced.',
      type='string')
  tracing_opts.add_option(
      '--record-continuously',
      help='Keep recording until stopped. The trace buffer is of fixed size '
           'and used as a ring buffer. If this option is omitted then '
           'recording stops when the trace buffer is full.',
      action='store_true')
  parser.add_option_group(tracing_opts)

  output_options = optparse.OptionGroup(parser, 'Output options')
  output_options.add_option(
      '-o', '--output',
      help='Save trace output to file.')
  parser.add_option_group(output_options)

  return parser


def _ProcessOptions(options):
  websocket.enableTrace(options.verbose)


def main():
  parser = _CreateOptionParser()
  options, _args = parser.parse_args()
  _ProcessOptions(options)

  with Connect(options.device, options.port) as tracing_backend:
    tracing_backend.StartTracing(TracingClient(),
                                 options.category_filter,
                                 options.record_continuously)
    raw_input('Capturing trace. Press Enter to stop...')
    trace = tracing_backend.StopTracing()

  filepath = DumpTrace(trace, options)
  print('Done')
  print('Trace written to file://%s' % filepath)


if __name__ == '__main__':
  sys.exit(main())
