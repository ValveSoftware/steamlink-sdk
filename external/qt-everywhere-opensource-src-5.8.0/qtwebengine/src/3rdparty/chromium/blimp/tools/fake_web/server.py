#!/usr/bin/env python

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
import tempfile

LIGHTTPD_CONF = """
server.document-root = "{root}"
server.port = {port}
mimetype.assign = (
  ".html" => "text/html"
)
"""

def run_lighttpd(conf):
  """Run lighttpd in a subprocess and block until it exits.

  Takes the lighttpd configuration file as a string.
  """
  with tempfile.NamedTemporaryFile() as conf_file:
    conf_file.write(conf)
    conf_file.file.close()
    server = subprocess.Popen(['lighttpd', '-D', '-f', conf_file.name])
    try:
      server.wait()
    except KeyboardInterrupt:
      # Let lighttpd handle the signal.
      server.wait()

def main():
  script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  data_dir = os.path.join(script_dir, 'data')

  parser = argparse.ArgumentParser()
  parser.add_argument('-p', '--port', default=3000)
  args = parser.parse_args()

  conf = LIGHTTPD_CONF.format(port=args.port, root=data_dir)
  print conf
  run_lighttpd(conf)

if __name__ == '__main__':
  main()
