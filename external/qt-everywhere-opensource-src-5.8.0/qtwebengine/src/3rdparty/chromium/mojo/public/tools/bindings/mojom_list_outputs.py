#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

def main():
  parser = argparse.ArgumentParser(
      description="GYP helper script for mapping mojoms => generated outputs.")
  parser.add_argument("--basedir", required=True)
  parser.add_argument("--variant", required=True)
  parser.add_argument("mojom", nargs="*")

  args = parser.parse_args()

  variant = args.variant if args.variant != "none" else None

  for mojom in args.mojom:
    full = os.path.join("<(SHARED_INTERMEDIATE_DIR)", args.basedir, mojom)
    base, ext = os.path.splitext(full)

    # Ignore non-mojom files.
    if ext != ".mojom":
      continue

    # Fix filename escaping issues on Windows.
    base = base.replace("\\", "/")
    if variant:
      print base + ".mojom-%s.cc" % variant
      print base + ".mojom-%s.h" % variant
      print base + ".mojom-%s-internal.h" % variant
    else:
      print base + ".mojom.cc"
      print base + ".mojom.h"
      print base + ".mojom-internal.h"
      print base + ".mojom.js"

  return 0

if __name__ == "__main__":
  sys.exit(main())
