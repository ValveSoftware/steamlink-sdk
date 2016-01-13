# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # These flags are only included in the NaCl standalone build.
    # They won't get included when NaCl is built as part of another
    # project, such as Chrome.  We don't necessarily want to enable
    # these flags for Chrome source that is built as NaCl untrusted
    # code.
    'nacl_default_compile_flags': [
      '-Wundef',
      '-Wstrict-prototypes',
    ],
  },
}
