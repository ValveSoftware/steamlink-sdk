# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Invokes gperf for the GN build.
# Usage: gperf.py [--developer_dir PATH_TO_XCODE] gperf ...
# TODO(brettw) this can be removed once the run_executable rules have been
# checked in for the GN build.

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--developer_dir", required=False)
    args, unknownargs = parser.parse_known_args()
    if args.developer_dir:
        os.environ['DEVELOPER_DIR'] = args.developer_dir

    subprocess.check_call(unknownargs)

if __name__ == '__main__':
    main()
