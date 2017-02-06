# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'fontconfig',
      'type': '<(component)',
      'dependencies' : [
        '../zlib/zlib.gyp:zlib',
        '../../build/linux/system.gyp:freetype2',
        '../libxml/libxml.gyp:libxml',
      ],
      'defines': [
          'HAVE_CONFIG_H',
          'FC_CACHEDIR="/var/cache/fontconfig"',
          'FONTCONFIG_PATH="/etc/fonts"',
      ],
      'conditions': [
        ['clang==1', {
          # Work around a null-conversion warning. See crbug.com/358852.
          'cflags': [
            '-Wno-non-literal-null-conversion',
          ],
        }],
      ],
      'sources': [
        'chromium/empty.cc',
        'src/src/fcarch.h',
        'src/src/fcatomic.c',
        'src/src/fcblanks.c',
        'src/src/fccache.c',
        'src/src/fccfg.c',
        'src/src/fccharset.c',
        'src/src/fccompat.c',
        'src/src/fcdbg.c',
        'src/src/fcdefault.c',
        'src/src/fcdir.c',
        'src/src/fcformat.c',
        'src/src/fcfreetype.c',
        'src/src/fcfs.c',
        'src/src/fchash.c',
        'src/src/fcinit.c',
        'src/src/fclang.c',
        'src/src/fclist.c',
        'src/src/fcmatch.c',
        'src/src/fcmatrix.c',
        'src/src/fcname.c',
        'src/src/fcobjs.c',
        'src/src/fcpat.c',
        'src/src/fcserialize.c',
        'src/src/fcstat.c',
        'src/src/fcstr.c',
        'src/src/fcxml.c',
        'src/src/ftglue.h',
        'src/src/ftglue.c',
      ],
      'include_dirs': [
        'src',
        'include',
        'include/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
           'src',
        ],
      },
    },
  ],
}
