# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def abs(filename):
    # open, abspath, etc. are all limited to the 260 char MAX_PATH and this causes
    # problems when we try to resolve long relative paths in the WebKit directory structure.
    return os.path.normpath(os.path.join(os.getcwd(), filename))
