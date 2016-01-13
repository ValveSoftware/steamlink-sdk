# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script locates the Android compilers given the bin directory of the
# android toolchain.

import glob
import subprocess
import sys

if len(sys.argv) != 2:
  print "Error: expecting one argument of the android toolchain dir."
  sys.exit(1)

# TODO(brettw) this logic seems like a bad idea. It was copied from
# common.gypi. It seems like the toolchain should just know the name given the
# current platform rather than having to rely on glob.
android_toolchain = sys.argv[1]
cc = glob.glob(android_toolchain + "/*-gcc")
cxx = glob.glob(android_toolchain + "/*-g++")

# We tolerate "no matches." In the Android AOSP WebView build, it runs this
# logic and the directory doesn't exist, giving no matches. But that build runs
# GYP to generate Android Makefiles which specify the compiler separately. So
# all we need to do in this case is ignore the error and continue with empty
# target compilers.
if len(cc) == 0:
  cc = [""]
if len(cxx) == 0:
  cxx = [""]
if len(cc) != 1 or len(cxx) != 1:
  print "More than one matching compiler."
  sys.exit(1)

# Get the host compilers from the current path.
which_gcc = subprocess.check_output(["which gcc"], shell=True).strip()
which_gxx = subprocess.check_output(["which g++"], shell=True).strip()

print ('["' + cc[0] + '","' + cxx[0] + '","' + which_gcc + '","' +
       which_gxx + '"]')
